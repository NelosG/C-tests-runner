#pragma once

/**
 * @file test_runner_service.h
 * @brief Facade owning the JobQueue + dependencies (BuildService, sandbox,
 *        SandboxTestExecutor, Pipeline). Adapters submit through this service.
 *
 * Job execution itself lives in Pipeline; this class only wires the queue
 * worker callback to Pipeline::execute() and exposes queue-management
 * methods (submit / cancel / getJobInfo / queue status).
 */

#include <build_service.h>
#include <cpu_isolator.h>
#include <functional>
#include <job_queue.h>
#include <memory>
#include <pipeline.h>
#include <progress_callback.h>
#include <resource_manager.h>
#include <sandbox_launcher.h>
#include <sandbox_test_executor.h>
#include <string>
#include <nlohmann/json.hpp>

class TestRunnerService {
    public:
        using CompletionCallback = JobQueue::CompletionCallback;

        TestRunnerService(
            BuildService::BuildConfig build_config,
            SandboxLauncher::Config sandbox_config,
            CpuIsolator::Config cpu_config,
            ResourceManager& resource_manager
        );

        /// Submit a job to the shared queue. Returns the assigned job_id.
        /// @param on_progress optional best-effort callback invoked on phase
        ///        transitions and around each test invocation. See progress_callback.h.
        std::string submit(
            nlohmann::json request,
            CompletionCallback on_complete = {},
            progress::callback on_progress = {}
        );

        /// Get a snapshot of a job's current state.
        JobQueue::JobInfo get_job_info(const std::string& job_id) const;

        /// Get an overview of all queues, active workers, and tracked jobs.
        nlohmann::json get_queue_status() const;

        /// Cancel a queued job.
        bool cancel(const std::string& job_id);

        /// Dynamically resize the correctness worker thread pool.
        void set_max_correctness_workers(int n);

        /// Set how long completed/failed/cancelled jobs are retained before cleanup.
        void set_job_retention_seconds(int sec);
        int job_retention_seconds() const;

        /// Per-job defaults - atomics inside BuildService; safe to read/write
        /// from any adapter thread while worker threads execute jobs.
        long long default_memory_limit_mb() const { return build_service_.default_memory_limit_mb(); }
        void set_default_memory_limit_mb(long long v) { build_service_.set_default_memory_limit_mb(v); }

        void set_default_threads(int n) { build_service_.set_default_threads(n); }
        void set_default_wall_time_sec(int sec) { build_service_.set_default_wall_time_sec(sec); }
        void set_default_cpu_time_sec(int sec) { build_service_.set_default_cpu_time_sec(sec); }
        void set_sandbox_process_multiplier(int n) { build_service_.set_sandbox_process_multiplier(n); }

        /// Atomic snapshot of all live per-job defaults - used by adapters for
        /// the `engineConfig` block in `info` events.
        struct EngineConfigSnapshot {
            long long default_memory_limit_mb;
            int default_threads;
            int default_wall_time_sec;
            int default_cpu_time_sec;
            int sandbox_process_multiplier;
        };

        EngineConfigSnapshot engine_config_snapshot() const {
            return {
                build_service_.default_memory_limit_mb(),
                build_service_.default_threads(),
                build_service_.default_wall_time_sec(),
                build_service_.default_cpu_time_sec(),
                build_service_.sandbox_process_multiplier()
            };
        }

        /// Read-only view of the *static* BuildService config (paths, generator etc.).
        /// Live per-job defaults must be read via the typed getters above -
        /// `BuildConfig`'s scalar fields are seed values only.
        const BuildService::BuildConfig& build_service_config() const {
            return build_service_.config();
        }

        /// Configured number of concurrent correctness workers (used by adapters
        /// e.g. for AMQP prefetch sizing).
        int correctness_workers() const { return build_service_.config().correctness_workers; }

        /// Engine node identifier - embedded in progress events emitted by
        /// Pipeline::execute. Set by `server.cpp` from `--node-id` arg / config;
        /// CLI invocations leave it empty. Called once at startup, before any
        /// submit() - no need for atomic / lock.
        void set_node_id(std::string id) { node_id_ = std::move(id); }
        const std::string& node_id() const { return node_id_; }

    private:
        BuildService build_service_;
        SandboxLauncher sandbox_;
        CpuIsolator cpu_isolator_;
        SandboxTestExecutor test_executor_;
        // queue_ must outlive pipeline_ because Pipeline holds a LaneController&
        // reference into it. Order of member declaration is the construction order:
        // queue_ is constructed first (with a placeholder executor), pipeline_ next,
        // then the real executor is wired in via queue_->set_executor() in the body.
        std::unique_ptr<JobQueue> queue_;
        Pipeline pipeline_;
        std::string node_id_;
};
