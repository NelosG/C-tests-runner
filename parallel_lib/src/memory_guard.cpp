#include <new>
#include <par/memory_guard.h>


namespace par {

    // ---- Thread-local active context ----
    //
    // Each job thread sets this before running tests. OMP child threads
    // inherit it via OMP_PARALLEL / OMP_TASK context propagation in pragma.h.
    // If null, onAlloc/onFree are no-ops (no tracking overhead).

    static thread_local MemoryContext* tl_mem_ctx = nullptr;

    // ---- MemoryContext methods ----

    void MemoryContext::resetStats() {
        stats.current_bytes.store(0, std::memory_order_relaxed);
        stats.peak_bytes.store(0, std::memory_order_relaxed);
        stats.allocations.store(0, std::memory_order_relaxed);
        stats.deallocations.store(0, std::memory_order_relaxed);
        stats.limit_exceeded.store(false, std::memory_order_relaxed);
    }

    // ---- Public API ----

    namespace memory {

        std::unique_ptr<MemoryContext> createContext(long long limit_bytes) {
            auto ctx = std::make_unique<MemoryContext>();
            ctx->limit_bytes = limit_bytes;
            return ctx;
        }

        void activateContext(MemoryContext* ctx) {
            tl_mem_ctx = ctx;
        }

        namespace detail {

            MemoryContext* currentContext() {
                return tl_mem_ctx;
            }

            void setContext(MemoryContext* ctx) {
                tl_mem_ctx = ctx;
            }

            bool tryAlloc(long long bytes) {
                auto* ctx = tl_mem_ctx;
                if (!ctx) return true;

                auto& s = ctx->stats;
                auto current = s.current_bytes.fetch_add(bytes, std::memory_order_relaxed) + bytes;

                // atomicMax for peak
                auto prev_peak = s.peak_bytes.load(std::memory_order_relaxed);
                while (current > prev_peak) {
                    if (s.peak_bytes.compare_exchange_weak(prev_peak, current, std::memory_order_relaxed))
                        break;
                }

                s.allocations.fetch_add(1, std::memory_order_relaxed);

                // Limit check
                if (ctx->limit_bytes > 0 && current > ctx->limit_bytes) {
                    s.limit_exceeded.store(true, std::memory_order_relaxed);
                    s.current_bytes.fetch_sub(bytes, std::memory_order_relaxed);
                    return false;
                }
                return true;
            }

            void onAlloc(long long bytes) {
                if (!tryAlloc(bytes)) throw std::bad_alloc();
            }

            void onFree(long long bytes) {
                auto* ctx = tl_mem_ctx;
                if (!ctx) return;

                ctx->stats.current_bytes.fetch_sub(bytes, std::memory_order_relaxed);
                ctx->stats.deallocations.fetch_add(1, std::memory_order_relaxed);
            }

        } // namespace detail
    } // namespace memory
} // namespace par
