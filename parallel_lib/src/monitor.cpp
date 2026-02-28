#include <chrono>
#include <memory>
#include <omp.h>
#include <random>
#include <thread>
#include <par/monitor.h>


namespace par {

    // ---- Thread-local active context ----
    //
    // Each job thread sets this before running tests. OMP child threads
    // inherit it via OMP_PARALLEL / OMP_TASK context propagation.
    // If null, all hooks behave as Mode::NORMAL (no-op).

    static thread_local MonitorContext* tl_ctx = nullptr;

    // ---- Default context for CLI/single-threaded use ----
    // When no explicit context is activated, the convenience API
    // (setMode, setMaxThreads, etc.) operates on this default.

    static MonitorContext g_default_ctx;

    // ---- Thread-local work nesting guard ----
    //
    // Prevents double-counting work when work-timed constructs nest.
    // Example: OMP_FOR inside OMP_SECTIONS — both have work-timed guards,
    // but only the outermost should accumulate elapsed time.
    //
    // Mechanism: workBegin() checks the flag; if already set, returns 0
    // (signaling "nested — don't time"). workEnd() skips accumulation
    // when start_ns == 0 and resets the flag when start_ns != 0.
    //
    // All callers are RAII guards (ForGuard, SectionsGuard, SingleGuard, etc.),
    // so the flag is guaranteed to be reset during stack unwinding.

    static thread_local bool tl_in_work_timing = false;

    // ---- Thread-local span tracking state ----

    static thread_local long long tl_span_depth = 0;
    static thread_local long long tl_strand_start_ns = 0;
    static thread_local std::shared_ptr<std::atomic<long long>> tl_children_max;

    // ---- Helpers ----
    //
    // All atomic operations use memory_order_relaxed. This is correct because:
    // - Stats counters are read only after implicit barriers at parallel region
    //   boundaries (TestEngine reads stats after the test function returns).
    // - atomicMax on span_ns uses CAS; stale reads cause harmless retries.
    // - The barrier provides the necessary happens-before relationship.

    static long long now_ns() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }

    static void atomicMax(std::atomic<long long>& target, long long value) {
        long long cur = target.load(std::memory_order_relaxed);
        while(value > cur) {
            if(target.compare_exchange_weak(cur, value, std::memory_order_relaxed))
                break;
        }
    }

    static inline MonitorContext* ctx() {
        return tl_ctx ? tl_ctx : &g_default_ctx;
    }

    // ---- MonitorContext methods ----

    void MonitorContext::resetStats() {
        stats.parallel_regions.store(0, std::memory_order_relaxed);
        stats.tasks_created.store(0, std::memory_order_relaxed);
        stats.single_regions.store(0, std::memory_order_relaxed);
        stats.taskwaits.store(0, std::memory_order_relaxed);
        stats.barriers.store(0, std::memory_order_relaxed);
        stats.criticals.store(0, std::memory_order_relaxed);
        stats.for_loops.store(0, std::memory_order_relaxed);
        stats.atomics.store(0, std::memory_order_relaxed);
        stats.sections.store(0, std::memory_order_relaxed);
        stats.masters.store(0, std::memory_order_relaxed);
        stats.ordered.store(0, std::memory_order_relaxed);
        stats.taskgroups.store(0, std::memory_order_relaxed);
        stats.simd_constructs.store(0, std::memory_order_relaxed);
        stats.cancels.store(0, std::memory_order_relaxed);
        stats.flushes.store(0, std::memory_order_relaxed);
        stats.taskyields.store(0, std::memory_order_relaxed);
        stats.max_threads_observed.store(0, std::memory_order_relaxed);
        stats.work_ns.store(0, std::memory_order_relaxed);
        stats.span_ns.store(0, std::memory_order_relaxed);
    }

    // ---- Public control API ----

    namespace monitor {

        std::unique_ptr<MonitorContext> createContext() {
            return std::make_unique<MonitorContext>();
        }

        void activateContext(MonitorContext* c) {
            tl_ctx = c;
        }

        void setMode(Mode mode) {
            ctx()->mode = mode;
        }

        Mode getMode() {
            return ctx()->mode;
        }

        void setMaxThreads(int n) {
            ctx()->max_threads = n;
            if(n > 0) {
                omp_set_num_threads(n);
            }
        }

        int getMaxThreads() {
            return ctx()->max_threads;
        }

        Stats& stats() {
            return ctx()->stats;
        }

        void resetStats() {
            ctx()->resetStats();
        }

        // ---- Internal detail hooks ----

        namespace detail {

            MonitorContext* currentContext() {
                return tl_ctx;
            }

            void setContext(MonitorContext* c) {
                tl_ctx = c;
            }

            void onParallelBegin() {
                auto* c = ctx();
                // Enforce thread count override before every parallel region
                if(c->max_threads > 0) {
                    omp_set_num_threads(c->max_threads);
                    omp_set_dynamic(0);
                }

                if(c->mode != Mode::NORMAL) {
                    c->stats.parallel_regions.fetch_add(1, std::memory_order_relaxed);

                    int mt = omp_get_max_threads();
                    int cur = c->stats.max_threads_observed.load(std::memory_order_relaxed);
                    while(mt > cur) {
                        if(c->stats.max_threads_observed.compare_exchange_weak(cur, mt, std::memory_order_relaxed))
                            break;
                    }
                }
            }

            void onParallelEnd() {}

            void onTaskCreate() {
                auto* c = ctx();
                if(c->mode != Mode::NORMAL) {
                    c->stats.tasks_created.fetch_add(1, std::memory_order_relaxed);
                }
            }

            void onSingle() {
                auto* c = ctx();
                if(c->mode != Mode::NORMAL) {
                    c->stats.single_regions.fetch_add(1, std::memory_order_relaxed);
                }
            }

            void onTaskwait() {
                auto* c = ctx();
                if(c->mode != Mode::NORMAL) {
                    c->stats.taskwaits.fetch_add(1, std::memory_order_relaxed);
                }
            }

            void onBarrier() {
                auto* c = ctx();
                if(c->mode != Mode::NORMAL) {
                    c->stats.barriers.fetch_add(1, std::memory_order_relaxed);
                }
            }

            void onCritical() {
                auto* c = ctx();
                if(c->mode != Mode::NORMAL) {
                    c->stats.criticals.fetch_add(1, std::memory_order_relaxed);
                }
            }

            void onForLoop() {
                auto* c = ctx();
                if(c->mode != Mode::NORMAL) {
                    c->stats.for_loops.fetch_add(1, std::memory_order_relaxed);
                }
            }

            void onAtomic() {
                auto* c = ctx();
                if(c->mode != Mode::NORMAL) {
                    c->stats.atomics.fetch_add(1, std::memory_order_relaxed);
                }
            }

            void onSections() {
                auto* c = ctx();
                if(c->mode != Mode::NORMAL) {
                    c->stats.sections.fetch_add(1, std::memory_order_relaxed);
                }
            }

            void onMaster() {
                auto* c = ctx();
                if(c->mode != Mode::NORMAL) {
                    c->stats.masters.fetch_add(1, std::memory_order_relaxed);
                }
            }

            void onOrdered() {
                auto* c = ctx();
                if(c->mode != Mode::NORMAL) {
                    c->stats.ordered.fetch_add(1, std::memory_order_relaxed);
                }
            }

            void onTaskgroup() {
                auto* c = ctx();
                if(c->mode != Mode::NORMAL) {
                    c->stats.taskgroups.fetch_add(1, std::memory_order_relaxed);
                }
            }

            void onSimd() {
                auto* c = ctx();
                if(c->mode != Mode::NORMAL) {
                    c->stats.simd_constructs.fetch_add(1, std::memory_order_relaxed);
                }
            }

            void onCancel() {
                auto* c = ctx();
                if(c->mode != Mode::NORMAL) {
                    c->stats.cancels.fetch_add(1, std::memory_order_relaxed);
                }
            }

            void onFlush() {
                auto* c = ctx();
                if(c->mode != Mode::NORMAL) {
                    c->stats.flushes.fetch_add(1, std::memory_order_relaxed);
                }
            }

            void onTaskyield() {
                auto* c = ctx();
                if(c->mode != Mode::NORMAL) {
                    c->stats.taskyields.fetch_add(1, std::memory_order_relaxed);
                }
            }

            void maybeInjectDelay() {
                auto* c = ctx();
                if(c->mode == Mode::STRESS) {
                    thread_local std::mt19937 gen(
                        static_cast<unsigned>(std::hash<std::thread::id>{}(std::this_thread::get_id()))
                    );
                    std::uniform_int_distribution<int> dist(0, 100);
                    int us = dist(gen);
                    if(us > 50) {
                        std::this_thread::sleep_for(std::chrono::microseconds(us - 50));
                    }
                }
            }

            // ---- Work (T1) tracking ----

            long long workBegin() {
                auto* c = ctx();
                if(c->mode != Mode::NORMAL && !tl_in_work_timing) {
                    tl_in_work_timing = true;
                    return now_ns();
                }
                return 0;
            }

            void workEnd(long long start_ns) {
                if(start_ns == 0) return;
                auto* c = ctx();
                if(c->mode != Mode::NORMAL) {
                    long long elapsed = now_ns() - start_ns;
                    c->stats.work_ns.fetch_add(elapsed, std::memory_order_relaxed);
                }
                // Always reset the nesting guard, even if mode changed to NORMAL
                // between workBegin() and workEnd(). Prevents tl_in_work_timing
                // from getting stuck permanently on this thread.
                tl_in_work_timing = false;
            }

            // ---- Span (T_inf) tracking ----

            SpanSaved spanSaveState() {
                return {tl_span_depth, tl_strand_start_ns, tl_children_max};
            }

            void spanRestoreState(const SpanSaved& saved) {
                tl_span_depth = saved.depth;
                tl_strand_start_ns = saved.strand_start;
                tl_children_max = saved.children_max;
            }

            void spanInitRoot() {
                auto* c = ctx();
                if(c->mode == Mode::NORMAL) return;
                tl_span_depth = 0;
                tl_children_max = std::make_shared<std::atomic<long long>>(0);
                tl_strand_start_ns = now_ns();
            }

            void spanFinalizeRoot() {
                auto* c = ctx();
                if(c->mode == Mode::NORMAL) return;

                // Close the final serial strand (post-last-taskwait code).
                long long now = now_ns();
                if(tl_strand_start_ns > 0) {
                    tl_span_depth += (now - tl_strand_start_ns);
                    tl_strand_start_ns = 0;
                }

                // Merge trailing children (tasks spawned after last taskwait).
                // At this point tl_span_depth = accumulated serial strands.
                // children_max = max span of children in the current sync group.
                // Critical path = max(serial_strands, max_child_path) because
                // the serial continuation runs in parallel with spawned tasks.
                if(tl_children_max) {
                    long long cm = tl_children_max->load(std::memory_order_relaxed);
                    if(cm > tl_span_depth) {
                        tl_span_depth = cm;
                    }
                }

                atomicMax(c->stats.span_ns, tl_span_depth);
            }

            SpanChildCtx spanPrepareChild() {
                auto* c = ctx();
                if(c->mode == Mode::NORMAL) {
                    return {0, nullptr};
                }
                long long now = now_ns();
                if(tl_strand_start_ns > 0) {
                    tl_span_depth += (now - tl_strand_start_ns);
                    tl_strand_start_ns = now;
                }
                return {tl_span_depth, tl_children_max};
            }

            SpanSaved spanEnterTask(const SpanChildCtx& span_ctx) {
                auto* c = ctx();
                if(c->mode == Mode::NORMAL) return {};

                SpanSaved saved;
                saved.depth = tl_span_depth;
                saved.strand_start = tl_strand_start_ns;
                saved.children_max = std::move(tl_children_max);

                tl_span_depth = span_ctx.parent_depth;
                tl_children_max = std::make_shared<std::atomic<long long>>(0);
                tl_strand_start_ns = now_ns();
                return saved;
            }

            void spanExitTask(SpanSaved& saved, const SpanChildCtx& span_ctx) {
                auto* c = ctx();
                // spanEnterTask also no-ops in NORMAL — TLS was never modified, nothing to restore
                if(c->mode == Mode::NORMAL) return;

                long long now = now_ns();
                if(tl_strand_start_ns > 0) {
                    tl_span_depth += (now - tl_strand_start_ns);
                }
                if(tl_children_max) {
                    long long cm = tl_children_max->load(std::memory_order_relaxed);
                    if(cm > tl_span_depth) {
                        tl_span_depth = cm;
                    }
                }
                long long final_depth = tl_span_depth;

                if(span_ctx.parent_children_max) {
                    atomicMax(*span_ctx.parent_children_max, final_depth);
                }
                atomicMax(c->stats.span_ns, final_depth);

                tl_span_depth = saved.depth;
                tl_strand_start_ns = saved.strand_start;
                tl_children_max = std::move(saved.children_max);
            }

            void spanSyncChildren() {
                auto* c = ctx();
                if(c->mode == Mode::NORMAL) return;
                long long now = now_ns();
                if(tl_strand_start_ns > 0) {
                    tl_span_depth += (now - tl_strand_start_ns);
                }
                if(tl_children_max) {
                    long long cm = tl_children_max->load(std::memory_order_relaxed);
                    if(cm > tl_span_depth) {
                        tl_span_depth = cm;
                    }
                    tl_children_max->store(0, std::memory_order_relaxed);
                }
                if(tl_strand_start_ns > 0) {
                    tl_strand_start_ns = now;
                }
            }

        } // namespace detail
    } // namespace monitor
} // namespace par