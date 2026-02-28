#pragma once

/**
 * @file pragma.h
 * @brief Student-facing parallel programming API with full OpenMP clause support.
 *
 * Provides macros wrapping OpenMP pragmas with monitor hooks for
 * statistics, work/span tracking, and delay injection.
 *
 * ## Work/Span Measurement Architecture
 *
 * Work (T1) — total CPU time spent in computational constructs.
 * Span (T_inf) — critical-path length (time with infinite CPUs).
 * Parallelism = Work / Span — theoretical max speedup.
 *
 * **Work-timed guards** (call workBegin/workEnd):
 *   ForGuard, SectionsGuard, ForSimdGuard, TaskloopGuard, TaskloopSimdGuard,
 *   SingleGuard, TaskBody, CtxInit (primary ctor + copy-ctor on all threads).
 *
 * Pattern: thread 0 increments the construct counter; ALL threads call
 * workBegin/workEnd to accumulate per-thread elapsed time into work_ns.
 * A thread-local nesting guard (tl_in_work_timing) prevents double-counting
 * when constructs nest (e.g. OMP_FOR inside OMP_SECTIONS). All guards are
 * RAII — the flag is guaranteed to be reset even on exceptions.
 *
 * **Span tracking** — two strategies:
 *   - Task-based (OMP_TASK/OMP_SINGLE): DAG-based span via spanInit/Prepare/
 *     Enter/Exit/Sync/Finalize. Tracks critical path through task tree.
 *   - For-based (OMP_FOR, OMP_PARALLEL_FOR, etc.): wall-clock fallback.
 *     Wall-clock ≈ max(thread_time) due to implicit barrier — a correct
 *     approximation (exact for balanced workloads). Applied in TestEngine
 *     when tasks_created == 0 or span_ns == 0.
 *
 * **Known limitations**:
 *   - Barrier wait time is included in work_ns and wall-clock span (slight overcount).
 *   - Taskloop work timing covers only the creating thread's chunk.
 *   - Nested parallel regions: outer span does not compose with inner span
 *     (each writes independently via atomicMax; span_ns = max of all levels).
 *   - CtxInit work timing includes fork/join overhead on the master thread.
 *
 * All macros accept arbitrary OpenMP clauses via __VA_ARGS__.
 *
 * Block constructs (take { body }):
 *   OMP_PARALLEL, OMP_SINGLE, OMP_TASK, OMP_FOR, OMP_CRITICAL,
 *   OMP_CRITICAL_NAMED, OMP_SECTIONS, OMP_SECTION, OMP_MASTER,
 *   OMP_MASKED, OMP_ORDERED, OMP_TASKGROUP
 *
 * Combined constructs (parallel + work-sharing):
 *   OMP_PARALLEL_FOR, OMP_PARALLEL_FOR_SIMD, OMP_PARALLEL_SECTIONS
 *
 * Standalone (no body):
 *   OMP_BARRIER, OMP_TASKWAIT, OMP_TASKYIELD, OMP_FLUSH, OMP_FLUSH_SEQ
 *
 * Atomic:
 *   OMP_ATOMIC, OMP_ATOMIC_READ, OMP_ATOMIC_WRITE, OMP_ATOMIC_CAPTURE
 *
 * SIMD / Taskloop:
 *   OMP_SIMD, OMP_FOR_SIMD, OMP_TASKLOOP, OMP_TASKLOOP_SIMD
 *
 * Cancellation:
 *   OMP_CANCEL_PARALLEL, OMP_CANCEL_FOR, OMP_CANCEL_SECTIONS,
 *   OMP_CANCEL_TASKGROUP, OMP_CANCELLATION_POINT_*
 *
 * Declarative (compile-time):
 *   OMP_THREADPRIVATE, OMP_DECLARE_SIMD, OMP_DECLARE_REDUCTION
 *
 * Runtime API (namespace par):
 *   Thread info: num_threads, thread_id, max_threads, num_procs, in_parallel,
 *                get_thread_limit, in_final, get_proc_bind
 *   Control:     set_num_threads, set_dynamic, get_dynamic, set_nested, get_nested,
 *                set_schedule, get_schedule, set_max_active_levels, get_max_active_levels
 *   Nesting:     get_level, get_active_level, get_ancestor_thread_num, get_team_size
 *   Timing:      wtime, wtick
 *   Locks:       Lock, LockGuard, NestLock, NestLockGuard
 *
 * Example (reduction):
 * @code
 * double sum = 0;
 * OMP_PARALLEL_FOR(default(shared) reduction(+:sum) schedule(dynamic))
 * for (int i = 0; i < n; ++i)
 *     sum += arr[i];
 * @endcode
 *
 * Example (task-based quicksort):
 * @code
 * OMP_PARALLEL(default(none) shared(array, low, high, gen)) {
 *     OMP_SINGLE() {
 *         do_qsort(array, low, high, gen);
 *     }
 *     OMP_TASKWAIT;
 * }
 * @endcode
 */

#include <omp.h>
#include <par/monitor.h>
#include <par/memory_guard.h>


// ============================================================
//  Internal stringify helpers
// ============================================================

#define _OMP_STR(...)    #__VA_ARGS__
#define _OMP_PRAGMA(...) _Pragma(_OMP_STR(__VA_ARGS__))


// ============================================================
//  RAII guard structs (used internally by macros)
// ============================================================

namespace par {
    namespace macro {

        /**
     * Wraps onParallelBegin() / onParallelEnd() around a parallel region.
     * Used by OMP_PARALLEL via a for-loop trick.
     */
        struct ParallelGuard {
            MonitorContext* ctx;
            MemoryContext* mem_ctx;
            bool done = false;

            ParallelGuard()
                : ctx(monitor::detail::currentContext()),
                  mem_ctx(memory::detail::currentContext()) {
                monitor::detail::onParallelBegin();
            }

            void finalize() {
                done = true;
                monitor::detail::onParallelEnd();
            }
        };

        /** Propagates the MonitorContext to each OMP thread. */
        struct ThreadInit {
            bool once = true;

            explicit ThreadInit(const ParallelGuard& g) {
                monitor::detail::setContext(g.ctx);
                memory::detail::setContext(g.mem_ctx);
            }
        };

        /**
     * Hooks for #pragma omp single: statistics + span root init/finalize.
     * Saves/restores outer span state to support nested parallel regions.
     */
        struct SingleGuard {
            bool once = true;
            long long t0 = 0;
            monitor::detail::SpanSaved outer_span;

            SingleGuard() {
                monitor::detail::onSingle();
                outer_span = monitor::detail::spanSaveState();
                monitor::detail::spanInitRoot();
                t0 = monitor::detail::workBegin();
            }

            ~SingleGuard() {
                monitor::detail::workEnd(t0);
                monitor::detail::spanFinalizeRoot();
                monitor::detail::spanRestoreState(outer_span);
            }
        };

        /**
     * Pre-task hook: statistics, delay injection, span child context.
     * Captures the monitor context for propagation into the task body.
     */
        struct TaskPre {
            bool done = false;
            monitor::detail::SpanChildCtx span_ctx;
            MonitorContext* ctx;
            MemoryContext* mem_ctx;

            TaskPre()
                : ctx(monitor::detail::currentContext()),
                  mem_ctx(memory::detail::currentContext()) {
                monitor::detail::onTaskCreate();
                monitor::detail::maybeInjectDelay();
                span_ctx = monitor::detail::spanPrepareChild();
            }
        };

        /** Task body hook: context propagation + span enter/exit + work timing. */
        struct TaskBody {
            bool once = true;
            monitor::detail::SpanSaved span_saved;
            monitor::detail::SpanChildCtx span_ctx;
            long long t0 = 0;

            explicit TaskBody(const TaskPre& pre) : span_ctx(pre.span_ctx) {
                monitor::detail::setContext(pre.ctx);
                memory::detail::setContext(pre.mem_ctx);
                span_saved = monitor::detail::spanEnterTask(span_ctx);
                t0 = monitor::detail::workBegin();
            }

            ~TaskBody() {
                monitor::detail::workEnd(t0);
                monitor::detail::spanExitTask(span_saved, span_ctx);
            }
        };

        /**
     * Generic single-hook guard (for-loop trick, executes hook once).
     * MasterOnly=true filters to thread 0 only (for worksharing constructs
     * where all threads encounter the guard but we want to count once).
     */
        template<void(*Hook)(), bool MasterOnly = false>
        struct SimpleGuard {
            bool once = true;

            SimpleGuard() {
                if(!MasterOnly || omp_get_thread_num() == 0) Hook();
            }
        };

        /**
     * Work-timed guard: calls counter hooks on construction, times work
     * via workBegin/workEnd. MasterOnly=true gates hooks to thread 0
     * (for worksharing constructs where all threads create the guard).
     *
     * Taskloop/taskloop-simd use MasterOnly=false because they are
     * normally encountered by a single thread (inside OMP_SINGLE).
     */
        template<bool MasterOnly, void(* ...Hooks)()>
        struct WorkTimedGuard {
            bool once = true;
            long long t0 = 0;

            WorkTimedGuard() {
                if(!MasterOnly || omp_get_thread_num() == 0) (Hooks(), ...);
                t0 = monitor::detail::workBegin();
            }

            ~WorkTimedGuard() { monitor::detail::workEnd(t0); }
        };

        using ForGuard = WorkTimedGuard<true, &monitor::detail::onForLoop>;
        using SectionsGuard = WorkTimedGuard<true, &monitor::detail::onSections>;
        using ForSimdGuard = WorkTimedGuard<true, &monitor::detail::onForLoop, &monitor::detail::onSimd>;
        using TaskloopGuard = WorkTimedGuard<false, &monitor::detail::onTaskCreate, &monitor::detail::onForLoop>;
        using TaskloopSimdGuard = WorkTimedGuard<
            false, &monitor::detail::onTaskCreate, &monitor::detail::onForLoop, &monitor::detail::onSimd>;

        using CriticalGuard = SimpleGuard<&monitor::detail::onCritical>;
        using MasterGuard = SimpleGuard<&monitor::detail::onMaster>;
        using OrderedGuard = SimpleGuard<&monitor::detail::onOrdered, true>;
        using SimdGuard = SimpleGuard<&monitor::detail::onSimd>;
        using TaskgroupGuard = SimpleGuard<&monitor::detail::onTaskgroup>;
        using AtomicGuard = SimpleGuard<&monitor::detail::onAtomic>;

        /**
     * Combined parallel + work-sharing guard (variadic).
     * Calls onParallelBegin() + all ExtraHooks on construction,
     * onParallelEnd() on finalize. Used by OMP_PARALLEL_FOR / etc.
     */
        template<void(* ...ExtraHooks)()>
        struct ParallelComboGuard {
            MonitorContext* ctx;
            MemoryContext* mem_ctx;
            bool done = false;

            ParallelComboGuard()
                : ctx(monitor::detail::currentContext()),
                  mem_ctx(memory::detail::currentContext()) {
                monitor::detail::onParallelBegin();
                (ExtraHooks(), ...);
            }

            void finalize() {
                done = true;
                monitor::detail::onParallelEnd();
            }
        };

        using ParallelForGuard = ParallelComboGuard<&monitor::detail::onForLoop>;
        using ParallelSectionsGuard = ParallelComboGuard<&monitor::detail::onSections>;
        using ParallelForSimdGuard = ParallelComboGuard<&monitor::detail::onForLoop, &monitor::detail::onSimd>;

        /**
     * Context propagation + work timing for combined parallel constructs
     * (parallel for, parallel sections, parallel for simd).
     *
     * Used via firstprivate() — OpenMP copy-initializes one copy per worker thread,
     * invoking the copy constructor which calls setContext() + workBegin() on that thread.
     *
     * The primary constructor also calls workBegin() so the master thread's work
     * is timed. If the runtime copy-constructs for the master too, the nesting
     * guard (tl_in_work_timing) prevents double-counting — the inner workBegin()
     * returns 0 and its workEnd() is a no-op.
     *
     * Relies on the OpenMP guarantee that firstprivate copy-init runs on the new thread.
     */
        struct CtxInit {
            MonitorContext* ctx;
            MemoryContext* mem_ctx;
            bool once = true;
            long long t0 = 0;

            explicit CtxInit(MonitorContext* c, MemoryContext* mc)
                : ctx(c), mem_ctx(mc) {
                t0 = monitor::detail::workBegin();
            }

            CtxInit(const CtxInit& o) : ctx(o.ctx), mem_ctx(o.mem_ctx), once(o.once), t0(0) {
                monitor::detail::setContext(ctx);
                memory::detail::setContext(mem_ctx);
                t0 = monitor::detail::workBegin();
            }

            ~CtxInit() { if(t0 != 0) monitor::detail::workEnd(t0); }
        };

    }
} // namespace par::macro


// ============================================================
//  Block macros (take { body } after them)
// ============================================================

/**
 * Parallel region with arbitrary clauses.
 * shared(_omp_g) is added automatically for the internal guard.
 * Works correctly with default(none) since _omp_g is explicitly shared.
 *
 * Usage:
 *   OMP_PARALLEL(default(none) shared(arr) private(i) reduction(+:sum)) {
 *       // parallel body
 *   }
 */
#define OMP_PARALLEL(...) \
    for (::par::macro::ParallelGuard _omp_g; !_omp_g.done; _omp_g.finalize()) \
    _OMP_PRAGMA(omp parallel shared(_omp_g) __VA_ARGS__) \
    for (::par::macro::ThreadInit _omp_ti(_omp_g); _omp_ti.once; _omp_ti.once = false)

/**
 * Single construct — one thread executes, others wait at implicit barrier.
 *
 * Usage:
 *   OMP_SINGLE() { ... }
 *   OMP_SINGLE(copyprivate(x)) { ... }
 */
#define OMP_SINGLE(...) \
    _OMP_PRAGMA(omp single __VA_ARGS__) \
    for (::par::macro::SingleGuard _omp_sg; _omp_sg.once; _omp_sg.once = false)

/**
 * Task construct with arbitrary clauses.
 * firstprivate(_omp_tp) is added automatically for context propagation.
 *
 * Usage:
 *   OMP_TASK(firstprivate(lo, hi)) { do_work(arr, lo, hi); }
 *   OMP_TASK(default(none) shared(arr) firstprivate(lo, hi)) { ... }
 */
#define OMP_TASK(...) \
    for (::par::macro::TaskPre _omp_tp; !_omp_tp.done; _omp_tp.done = true) \
    _OMP_PRAGMA(omp task firstprivate(_omp_tp) __VA_ARGS__) \
    for (::par::macro::TaskBody _omp_tb(_omp_tp); _omp_tb.once; _omp_tb.once = false)

/**
 * For work-sharing construct (must be inside a parallel region).
 * The actual for-loop statement must follow immediately.
 *
 * Usage:
 *   OMP_FOR(schedule(dynamic, 64) nowait)
 *   for (int i = 0; i < n; ++i) { ... }
 */
#define OMP_FOR(...) \
    for (::par::macro::ForGuard _omp_fg; _omp_fg.once; _omp_fg.once = false) \
    _OMP_PRAGMA(omp for __VA_ARGS__)

/**
 * Unnamed critical section.
 *
 * Usage:
 *   OMP_CRITICAL { counter++; }
 */
#define OMP_CRITICAL \
    _OMP_PRAGMA(omp critical) \
    for (::par::macro::CriticalGuard _omp_cg; _omp_cg.once; _omp_cg.once = false)

/**
 * Named critical section.
 *
 * Usage:
 *   OMP_CRITICAL_NAMED(my_lock) { data.push_back(x); }
 */
#define OMP_CRITICAL_NAMED(name) \
    _OMP_PRAGMA(omp critical(name)) \
    for (::par::macro::CriticalGuard _omp_cg; _omp_cg.once; _omp_cg.once = false)

/**
 * Sections construct.
 *
 * Usage:
 *   OMP_SECTIONS() { OMP_SECTION { a(); } OMP_SECTION { b(); } }
 */
#define OMP_SECTIONS(...) \
    for (::par::macro::SectionsGuard _omp_secg; _omp_secg.once; _omp_secg.once = false) \
    _OMP_PRAGMA(omp sections __VA_ARGS__)

/** Section within a sections construct. */
#define OMP_SECTION \
    _Pragma("omp section")

/**
 * Master construct — only the master thread executes the block.
 *
 * Usage:
 *   OMP_MASTER { printf("master thread\n"); }
 */
#define OMP_MASTER \
    _Pragma("omp master") \
    for (::par::macro::MasterGuard _omp_mg; _omp_mg.once; _omp_mg.once = false)

/**
 * Masked construct (OpenMP 5.1, replaces master with optional filter clause).
 *
 * Usage:
 *   OMP_MASKED() { printf("master thread\n"); }
 *   OMP_MASKED(filter(2)) { printf("thread 2 only\n"); }
 */
#define OMP_MASKED(...) \
    _OMP_PRAGMA(omp masked __VA_ARGS__) \
    for (::par::macro::MasterGuard _omp_mg; _omp_mg.once; _omp_mg.once = false)

/** Ordered construct. */
#define OMP_ORDERED \
    _Pragma("omp ordered") \
    for (::par::macro::OrderedGuard _omp_og; _omp_og.once; _omp_og.once = false)


// ============================================================
//  Combined constructs (parallel + work-sharing)
// ============================================================

/**
 * Combined parallel for — creates a parallel region and distributes
 * loop iterations. The for-loop must follow immediately.
 * All clauses (schedule, reduction, default, etc.) go in one place.
 *
 * Usage:
 *   OMP_PARALLEL_FOR(default(shared) schedule(dynamic) reduction(+:sum))
 *   for (int i = 0; i < n; ++i) { sum += arr[i]; }
 */
#define OMP_PARALLEL_FOR(...) \
    for (::par::macro::ParallelForGuard _omp_pfg; !_omp_pfg.done; _omp_pfg.finalize()) \
    for (::par::macro::CtxInit _omp_ci(_omp_pfg.ctx, _omp_pfg.mem_ctx); _omp_ci.once; _omp_ci.once = false) \
    _OMP_PRAGMA(omp parallel for shared(_omp_pfg) firstprivate(_omp_ci) __VA_ARGS__)

/**
 * Combined parallel for simd.
 *
 * Usage:
 *   OMP_PARALLEL_FOR_SIMD(default(shared) schedule(static) simdlen(4))
 *   for (int i = 0; i < n; ++i) { ... }
 */
#define OMP_PARALLEL_FOR_SIMD(...) \
    for (::par::macro::ParallelForSimdGuard _omp_pfsg; !_omp_pfsg.done; _omp_pfsg.finalize()) \
    for (::par::macro::CtxInit _omp_ci(_omp_pfsg.ctx, _omp_pfsg.mem_ctx); _omp_ci.once; _omp_ci.once = false) \
    _OMP_PRAGMA(omp parallel for simd shared(_omp_pfsg) firstprivate(_omp_ci) __VA_ARGS__)

/**
 * Combined parallel sections.
 *
 * Usage:
 *   OMP_PARALLEL_SECTIONS(default(shared)) {
 *       OMP_SECTION { a(); }
 *       OMP_SECTION { b(); }
 *   }
 */
#define OMP_PARALLEL_SECTIONS(...) \
    for (::par::macro::ParallelSectionsGuard _omp_psg; !_omp_psg.done; _omp_psg.finalize()) \
    for (::par::macro::CtxInit _omp_ci(_omp_psg.ctx, _omp_psg.mem_ctx); _omp_ci.once; _omp_ci.once = false) \
    _OMP_PRAGMA(omp parallel sections shared(_omp_psg) firstprivate(_omp_ci) __VA_ARGS__)


// ============================================================
//  Standalone macros (no body)
// ============================================================

/** Barrier synchronization. */
#define OMP_BARRIER \
    do { ::par::monitor::detail::onBarrier(); \
         _Pragma("omp barrier") \
         ::par::monitor::detail::maybeInjectDelay(); } while(0)

/** Wait for child tasks to complete. */
#define OMP_TASKWAIT \
    do { ::par::monitor::detail::onTaskwait(); \
         _Pragma("omp taskwait") \
         ::par::monitor::detail::spanSyncChildren(); } while(0)

/** Yield the current task (hint to the runtime). */
#define OMP_TASKYIELD \
    do { ::par::monitor::detail::onTaskyield(); \
         _Pragma("omp taskyield") } while(0)


// ============================================================
//  Atomic macros (one statement after the macro)
// ============================================================

#define OMP_ATOMIC \
    for (::par::macro::AtomicGuard _omp_ag; _omp_ag.once; _omp_ag.once = false) \
    _Pragma("omp atomic")
#define OMP_ATOMIC_READ \
    for (::par::macro::AtomicGuard _omp_ag; _omp_ag.once; _omp_ag.once = false) \
    _Pragma("omp atomic read")
#define OMP_ATOMIC_WRITE \
    for (::par::macro::AtomicGuard _omp_ag; _omp_ag.once; _omp_ag.once = false) \
    _Pragma("omp atomic write")
#define OMP_ATOMIC_CAPTURE \
    for (::par::macro::AtomicGuard _omp_ag; _omp_ag.once; _omp_ag.once = false) \
    _Pragma("omp atomic capture")


// ============================================================
//  Additional constructs
// ============================================================

/** Memory flush. */
#define OMP_FLUSH \
    do { ::par::monitor::detail::onFlush(); _Pragma("omp flush") } while(0)
#define OMP_FLUSH_SEQ \
    do { ::par::monitor::detail::onFlush(); _Pragma("omp flush seq_cst") } while(0)

/** Taskgroup — all child tasks complete by end of block. */
#define OMP_TASKGROUP \
    for (::par::macro::TaskgroupGuard _omp_tgg; _omp_tgg.once; _omp_tgg.once = false) \
    _Pragma("omp taskgroup")

/**
 * Taskloop — task-based parallel loop (OpenMP 4.5+).
 * Should be used inside OMP_SINGLE (only one thread generates tasks).
 * Work timing covers only the creating thread's chunk; runtime-spawned
 * task iterations are not individually instrumented.
 */
#define OMP_TASKLOOP(...) \
    for (::par::macro::TaskloopGuard _omp_tlg; _omp_tlg.once; _omp_tlg.once = false) \
    _OMP_PRAGMA(omp taskloop __VA_ARGS__)

/**
 * Taskloop simd — combined taskloop + SIMD (OpenMP 4.5+).
 * Same usage constraints as OMP_TASKLOOP.
 */
#define OMP_TASKLOOP_SIMD(...) \
    for (::par::macro::TaskloopSimdGuard _omp_tlsg; _omp_tlsg.once; _omp_tlsg.once = false) \
    _OMP_PRAGMA(omp taskloop simd __VA_ARGS__)

/** Cancel constructs. */
#define OMP_CANCEL_PARALLEL \
    do { ::par::monitor::detail::onCancel(); _Pragma("omp cancel parallel") } while(0)
#define OMP_CANCEL_FOR \
    do { ::par::monitor::detail::onCancel(); _Pragma("omp cancel for") } while(0)
#define OMP_CANCEL_SECTIONS \
    do { ::par::monitor::detail::onCancel(); _Pragma("omp cancel sections") } while(0)
#define OMP_CANCEL_TASKGROUP \
    do { ::par::monitor::detail::onCancel(); _Pragma("omp cancel taskgroup") } while(0)

/** Cancellation points. */
#define OMP_CANCELLATION_POINT_PARALLEL \
    do { ::par::monitor::detail::onCancel(); _Pragma("omp cancellation point parallel") } while(0)
#define OMP_CANCELLATION_POINT_FOR \
    do { ::par::monitor::detail::onCancel(); _Pragma("omp cancellation point for") } while(0)
#define OMP_CANCELLATION_POINT_SECTIONS \
    do { ::par::monitor::detail::onCancel(); _Pragma("omp cancellation point sections") } while(0)
#define OMP_CANCELLATION_POINT_TASKGROUP \
    do { ::par::monitor::detail::onCancel(); _Pragma("omp cancellation point taskgroup") } while(0)

/** SIMD vectorization (OpenMP 4.0+). */
#define OMP_SIMD(...) \
    for (::par::macro::SimdGuard _omp_smg; _omp_smg.once; _omp_smg.once = false) \
    _OMP_PRAGMA(omp simd __VA_ARGS__)
#define OMP_FOR_SIMD(...) \
    for (::par::macro::ForSimdGuard _omp_fsg; _omp_fsg.once; _omp_fsg.once = false) \
    _OMP_PRAGMA(omp for simd __VA_ARGS__)


// ============================================================
//  Declarative directives (no runtime hook — compile-time only)
// ============================================================

/**
 * Threadprivate — makes global/static vars private to each thread.
 * Must appear at file scope after the variable declaration.
 *
 * Usage:
 *   static int my_var;
 *   OMP_THREADPRIVATE(my_var)
 */
#define OMP_THREADPRIVATE(...) \
    _OMP_PRAGMA(omp threadprivate(__VA_ARGS__))

/**
 * Declare SIMD — declares a function variant for SIMD execution.
 * Must appear before the function definition.
 *
 * Usage:
 *   OMP_DECLARE_SIMD(uniform(a) linear(i))
 *   double f(double* a, int i) { return a[i] * 2.0; }
 */
#define OMP_DECLARE_SIMD(...) \
    _OMP_PRAGMA(omp declare simd __VA_ARGS__)

/**
 * Declare reduction — defines a user-defined reduction operator.
 *
 * Usage:
 *   OMP_DECLARE_REDUCTION(merge : std::vector<int> : omp_out.insert(omp_out.end(), omp_in.begin(), omp_in.end()))
 */
#define OMP_DECLARE_REDUCTION(...) \
    _OMP_PRAGMA(omp declare reduction(__VA_ARGS__))


// ============================================================
//  Runtime query / control functions + Lock API
// ============================================================

namespace par {

    // ---- Thread / team info ----

    /** @brief Number of threads in the current parallel team. */
    inline int num_threads() { return omp_get_num_threads(); }

    /** @brief ID of the calling thread (0-based). */
    inline int thread_id() { return omp_get_thread_num(); }

    /** @brief Max threads that would be used in the next parallel region. */
    inline int max_threads() { return omp_get_max_threads(); }

    /** @brief Number of available processors. */
    inline int num_procs() { return omp_get_num_procs(); }

    /** @brief True if called from within a parallel region. */
    inline bool in_parallel() { return omp_in_parallel() != 0; }

    /** @brief Max total threads the implementation can provide. */
    inline int get_thread_limit() { return omp_get_thread_limit(); }

    // ---- Thread control ----

    /** @brief Set the number of threads for the next parallel region. */
    inline void set_num_threads(int n) { omp_set_num_threads(n); }

    /** @brief Enable or disable dynamic thread adjustment. */
    inline void set_dynamic(bool enable) { omp_set_dynamic(enable ? 1 : 0); }

    /** @brief Query whether dynamic thread adjustment is enabled. */
    inline bool get_dynamic() { return omp_get_dynamic() != 0; }

    /** @brief Enable or disable nested parallelism. */
    inline void set_nested(bool enable) { omp_set_nested(enable ? 1 : 0); }

    /** @brief Query whether nested parallelism is enabled. */
    inline bool get_nested() { return omp_get_nested() != 0; }

    // ---- Schedule ----

    /** @brief Set the default runtime schedule. */
    inline void set_schedule(omp_sched_t kind, int chunk_size) {
        omp_set_schedule(kind, chunk_size);
    }

    /** @brief Get the current runtime schedule (kind + chunk_size). */
    inline void get_schedule(omp_sched_t* kind, int* chunk_size) {
        omp_get_schedule(kind, chunk_size);
    }

    // ---- Nesting info ----

    /** @brief Set maximum allowed nesting depth of parallel regions. */
    inline void set_max_active_levels(int levels) { omp_set_max_active_levels(levels); }

    /** @brief Get maximum allowed nesting depth. */
    inline int get_max_active_levels() { return omp_get_max_active_levels(); }

    /** @brief Current nesting level (0 = outside any parallel region). */
    inline int get_level() { return omp_get_level(); }

    /** @brief Number of active (non-serialized) nesting levels. */
    inline int get_active_level() { return omp_get_active_level(); }

    /** @brief Thread ID of the ancestor at the given nesting level. */
    inline int get_ancestor_thread_num(int level) { return omp_get_ancestor_thread_num(level); }

    /** @brief Team size at the given nesting level. */
    inline int get_team_size(int level) { return omp_get_team_size(level); }

    // ---- Task info ----

    /** @brief True if executing in a final task (all child tasks are included). */
    inline bool in_final() { return omp_in_final() != 0; }

    // ---- Proc bind ----

    /** @brief Get the thread affinity policy for the current parallel region. */
    inline omp_proc_bind_t get_proc_bind() { return omp_get_proc_bind(); }

    // ---- Cancellation ----

    /** @brief True if cancellation is enabled (OMP_CANCELLATION=true). */
    inline bool get_cancellation() { return omp_get_cancellation() != 0; }

    // ---- Timing ----

    /** @brief Wall-clock time in seconds. */
    inline double wtime() { return omp_get_wtime(); }

    /** @brief Timer resolution in seconds. */
    inline double wtick() { return omp_get_wtick(); }

    // ---- Lock API (RAII wrapper for omp_lock_t) ----

    class Lock {
        omp_lock_t lk_;

        public:
            Lock() { omp_init_lock(&lk_); }
            ~Lock() { omp_destroy_lock(&lk_); }
            void lock() { omp_set_lock(&lk_); }
            void unlock() { omp_unset_lock(&lk_); }
            bool try_lock() { return omp_test_lock(&lk_) != 0; }

            Lock(const Lock&) = delete;
            Lock& operator=(const Lock&) = delete;
    };

    class LockGuard {
        Lock& lk_;

        public:
            explicit LockGuard(Lock& lk) : lk_(lk) { lk_.lock(); }
            ~LockGuard() { lk_.unlock(); }

            LockGuard(const LockGuard&) = delete;
            LockGuard& operator=(const LockGuard&) = delete;
    };

    // ---- Nested Lock API (RAII wrapper for omp_nest_lock_t) ----

    class NestLock {
        omp_nest_lock_t lk_;

        public:
            NestLock() { omp_init_nest_lock(&lk_); }
            ~NestLock() { omp_destroy_nest_lock(&lk_); }
            void lock() { omp_set_nest_lock(&lk_); }
            void unlock() { omp_unset_nest_lock(&lk_); }
            /** @brief Returns nesting count (>0 if lock acquired, 0 if not). */
            int try_lock() { return omp_test_nest_lock(&lk_); }

            NestLock(const NestLock&) = delete;
            NestLock& operator=(const NestLock&) = delete;
    };

    class NestLockGuard {
        NestLock& lk_;

        public:
            explicit NestLockGuard(NestLock& lk) : lk_(lk) { lk_.lock(); }
            ~NestLockGuard() { lk_.unlock(); }

            NestLockGuard(const NestLockGuard&) = delete;
            NestLockGuard& operator=(const NestLockGuard&) = delete;
    };

} // namespace par