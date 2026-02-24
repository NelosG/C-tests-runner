#pragma once

/**
 * @file monitor.h
 * @brief Runtime monitoring and control API for the parallel library.
 *
 * Used by the test engine to:
 * - Count parallel constructs (verify student actually parallelized)
 * - Override thread counts (student can't bypass)
 * - Inject random delays to expose race conditions (STRESS mode)
 * - Measure work and span for parallel efficiency analysis
 *
 * Work (T1) — total computation across all tasks (sequential time).
 * Span (T_inf) — critical-path length through the task DAG (time with inf CPUs).
 * Parallelism = Work / Span — upper bound on useful speedup.
 *
 * All state is per-MonitorContext. Multiple contexts can coexist for
 * concurrent correctness testing. The active context is stored in a
 * thread-local pointer propagated through OpenMP parallel regions.
 */

#include <atomic>
#include <memory>


namespace par {

    /**
 * @brief Accumulated statistics from parallel construct usage.
 *
 * All counters are atomic for safe concurrent updates from OMP threads.
 */
    struct Stats {
        std::atomic<int> parallel_regions{0};
        std::atomic<int> tasks_created{0};
        std::atomic<int> single_regions{0};
        std::atomic<int> taskwaits{0};
        std::atomic<int> barriers{0};
        std::atomic<int> criticals{0};
        std::atomic<int> for_loops{0};
        std::atomic<int> atomics{0};
        std::atomic<int> sections{0};
        std::atomic<int> masters{0};
        std::atomic<int> ordered{0};
        std::atomic<int> taskgroups{0};
        std::atomic<int> simd_constructs{0};
        std::atomic<int> cancels{0};
        std::atomic<int> flushes{0};
        std::atomic<int> taskyields{0};
        std::atomic<int> max_threads_observed{0};

        std::atomic<long long> work_ns{0};
        std::atomic<long long> span_ns{0};
    };

    /**
 * @brief Operating mode for the parallel runtime.
 */
    enum class Mode {
        NORMAL,   ///< No instrumentation — minimal overhead.
        MONITOR,  ///< Count all parallel constructs + track work/span.
        STRESS    ///< Monitor + inject random delays to expose races.
    };

    /**
 * @brief Per-job monitor state.
 *
 * Each test job creates its own context so concurrent jobs don't
 * corrupt each other's statistics or mode settings.
 */
    struct MonitorContext {
        Mode mode = Mode::NORMAL;
        int max_threads = 0;
        Stats stats;

        void resetStats();
    };

    /**
 * @brief Test engine control interface for the parallel runtime.
 */
    namespace monitor {

        // ---- Context management (for concurrent jobs) ----

        /** @brief Create a new independent monitor context. */
        std::unique_ptr<MonitorContext> createContext();

        /**
     * @brief Set the active context for the calling thread.
     * @param ctx Pointer to context, or nullptr to deactivate.
     *
     * This is propagated to OMP child threads via OMP_PARALLEL / OMP_TASK macros.
     */
        void activateContext(MonitorContext* ctx);

        // ---- Convenience API (operates on the active context) ----

        void setMode(Mode mode);
        Mode getMode();

        void setMaxThreads(int n);
        int getMaxThreads();

        /** @brief Access the active context's statistics. */
        Stats& stats();

        /** @brief Reset all counters to zero in the active context. */
        void resetStats();

        /**
     * @brief Internal hooks called by pragma.h macros/guards.
     * @note Not for student use.
     */
        namespace detail {
            /** @brief Get the active MonitorContext* for the calling thread. */
            MonitorContext* currentContext();

            /** @brief Set the active MonitorContext* for the calling thread. */
            void setContext(MonitorContext* ctx);

            void onParallelBegin();
            void onParallelEnd();
            void onTaskCreate();
            void onSingle();
            void onTaskwait();
            void onBarrier();
            void onCritical();
            void onForLoop();
            void onAtomic();
            void onSections();
            void onMaster();
            void onOrdered();
            void onTaskgroup();
            void onSimd();
            void onCancel();
            void onFlush();
            void onTaskyield();

            void maybeInjectDelay();

            /**
             * @brief Begin work timing for the calling thread.
             * @return Timestamp (ns) if timing started, or 0 if already inside a
             *         work-timed region (nesting guard tl_in_work_timing is set).
             *
             * Called by: ForGuard, SectionsGuard, ForSimdGuard, TaskloopGuard,
             * TaskloopSimdGuard, SingleGuard, TaskBody, CtxInit (both ctors).
             */
            long long workBegin();

            /**
             * @brief End work timing and accumulate elapsed time into work_ns.
             * @param start_ns Value returned by workBegin(); 0 means nested (no-op).
             *
             * Resets the nesting guard so the next outer region can resume timing.
             * All callers are RAII guards — tl_in_work_timing is guaranteed to be
             * reset even during stack unwinding from exceptions.
             */
            void workEnd(long long start_ns);

            // ---- Span (critical-path) tracking ----

            struct SpanChildCtx {
                long long parent_depth = 0;
                std::shared_ptr<std::atomic<long long>> parent_children_max;
            };

            struct SpanSaved {
                long long depth = 0;
                long long strand_start = 0;
                std::shared_ptr<std::atomic<long long>> children_max;
            };

            /** @brief Save current span TLS state (for nesting). */
            SpanSaved spanSaveState();

            /** @brief Restore previously saved span TLS state. */
            void spanRestoreState(const SpanSaved& saved);

            void spanInitRoot();
            void spanFinalizeRoot();
            SpanChildCtx spanPrepareChild();
            SpanSaved spanEnterTask(const SpanChildCtx& ctx);
            void spanExitTask(SpanSaved& saved, const SpanChildCtx& ctx);
            void spanSyncChildren();
        }

    } // namespace monitor
} // namespace par
