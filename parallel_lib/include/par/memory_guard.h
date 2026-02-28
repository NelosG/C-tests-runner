#pragma once

/**
 * @file memory_guard.h
 * @brief Per-job memory tracking and limiting for student code.
 *
 * Mirrors the MonitorContext pattern: a MemoryContext is created per job,
 * activated on the job thread, and propagated to OMP child threads via
 * the guard structs in pragma.h.
 *
 * Student code is unaware of this — operator new/delete overrides are
 * injected into student_solution.dll at compile time via cmake_generator.
 * Those overrides call onAlloc/onFree, which update the active context's
 * stats and enforce the memory limit by throwing std::bad_alloc.
 */

#include <atomic>
#include <memory>


namespace par {

    /**
     * @brief Accumulated memory statistics from student code allocations.
     *
     * All counters are atomic for safe concurrent updates from OMP threads.
     * Aligned to avoid false sharing on the hot path (current_bytes).
     */
    struct MemoryStats {
        alignas(64) std::atomic<long long> current_bytes{0};
        alignas(64) std::atomic<long long> peak_bytes{0};
        std::atomic<long long> allocations{0};
        std::atomic<long long> deallocations{0};
        std::atomic<bool> limit_exceeded{false};
    };

    /**
     * @brief Per-job memory state.
     *
     * Each test job creates its own context so concurrent jobs don't
     * corrupt each other's tracking or limits.
     */
    struct MemoryContext {
        long long limit_bytes = 0;  ///< 0 = unlimited
        MemoryStats stats;

        void resetStats();
    };

    namespace memory {

        /** @brief Create a new independent memory context. */
        std::unique_ptr<MemoryContext> createContext(long long limit_bytes = 0);

        /**
         * @brief Set the active memory context for the calling thread.
         * @param ctx Pointer to context, or nullptr to deactivate.
         *
         * Propagated to OMP child threads via OMP_PARALLEL / OMP_TASK macros.
         */
        void activateContext(MemoryContext* ctx);

        namespace detail {
            /** @brief Get the active MemoryContext* for the calling thread. */
            MemoryContext* currentContext();

            /** @brief Set the active MemoryContext* for the calling thread. */
            void setContext(MemoryContext* ctx);

            /**
             * @brief Non-throwing allocation check for C allocators (malloc, calloc, etc).
             *
             * Updates current_bytes, peak_bytes, allocation count.
             * Returns false if limit exceeded (rolls back current_bytes first).
             */
            bool tryAlloc(long long bytes);

            /**
             * @brief Called on every C++ allocation from student code (operator new).
             *
             * Delegates to tryAlloc; throws std::bad_alloc on failure.
             */
            void onAlloc(long long bytes);

            /**
             * @brief Called on every deallocation from student code.
             *
             * Decrements current_bytes and increments deallocation count.
             */
            void onFree(long long bytes);
        }

    } // namespace memory
} // namespace par
