#include "runner.h"

#include <memory>
#include <omp.h>
#include <par/monitor.h>

namespace runner {

    namespace {
        std::unique_ptr<par::MonitorContext> monitor_ctx_;
    }

    void setup() {
        const auto& cfg = runner::config();

        omp_set_num_threads(cfg.thread_count);
        omp_set_dynamic(0);

        if(cfg.monitor_mode == "stress") {
            monitor_ctx_ = par::monitor::create_context();
            monitor_ctx_->mode = par::Mode::STRESS;
            monitor_ctx_->max_threads = cfg.thread_count;
            par::monitor::activate_context(monitor_ctx_.get());
        } else if(cfg.monitor_mode == "monitor") {
            monitor_ctx_ = par::monitor::create_context();
            monitor_ctx_->mode = par::Mode::MONITOR;
            monitor_ctx_->max_threads = cfg.thread_count;
            par::monitor::activate_context(monitor_ctx_.get());
        }

        if(monitor_ctx_) {
            monitor_ctx_->reset_stats();
        }

        // Finish hook writes parallel stats directly into Meta struct (no JSON)
        runner::set_finish_hook([](runner::Meta& meta) {
            if(monitor_ctx_) {
                auto& s = monitor_ctx_->stats;
                meta.parallel_regions = s.parallel_regions.load(std::memory_order_relaxed);
                meta.tasks_created    = s.tasks_created.load(std::memory_order_relaxed);
                meta.max_threads_used = s.max_threads_observed.load(std::memory_order_relaxed);
                meta.single_regions   = s.single_regions.load(std::memory_order_relaxed);
                meta.taskwaits        = s.taskwaits.load(std::memory_order_relaxed);
                meta.barriers         = s.barriers.load(std::memory_order_relaxed);
                meta.criticals        = s.criticals.load(std::memory_order_relaxed);
                meta.for_loops        = s.for_loops.load(std::memory_order_relaxed);
                meta.atomics          = s.atomics.load(std::memory_order_relaxed);
                meta.sections         = s.sections.load(std::memory_order_relaxed);
                meta.masters          = s.masters.load(std::memory_order_relaxed);
                meta.ordered          = s.ordered.load(std::memory_order_relaxed);
                meta.taskgroups       = s.taskgroups.load(std::memory_order_relaxed);
                meta.simd_constructs  = s.simd_constructs.load(std::memory_order_relaxed);
                meta.cancels          = s.cancels.load(std::memory_order_relaxed);
                meta.flushes          = s.flushes.load(std::memory_order_relaxed);
                meta.taskyields       = s.taskyields.load(std::memory_order_relaxed);
                meta.work_ns          = s.work_ns.load(std::memory_order_relaxed);
                meta.span_ns          = s.span_ns.load(std::memory_order_relaxed);

                par::monitor::activate_context(nullptr);
                monitor_ctx_.reset();
            }
        });
    }

} // namespace runner
