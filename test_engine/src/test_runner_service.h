#pragma once

/**
 * @file test_runner_service.h
 * @brief Orchestrates test jobs: build -> load -> test -> report -> cleanup.
 *
 * Owns a shared JobQueue — all adapters submit through this service.
 * Workflow: solution built first, then test plugins compiled against the real student DLL.
 */

#include <build_service.h>
#include <filesystem>
#include <functional>
#include <job_queue.h>
#include <memory>
#include <plugin_loader.h>
#include <string>
#include <nlohmann/json.hpp>

namespace par { struct MonitorContext; }
class TestRegistry;

class TestRunnerService {
    public:
        using CompletionCallback = JobQueue::CompletionCallback;

        explicit TestRunnerService(BuildService::BuildConfig config);

        /// Submit a job to the shared queue. Returns the assigned job_id.
        std::string submit(nlohmann::json request, CompletionCallback on_complete = {});

        /// Get a snapshot of a job's current state.
        JobQueue::JobInfo getJobInfo(const std::string& job_id) const;

        /// Get an overview of all queues, active workers, and tracked jobs.
        nlohmann::json getQueueStatus() const;

        /// Cancel a queued job.
        bool cancel(const std::string& job_id);

        /// Dynamically resize the correctness worker thread pool.
        void setMaxCorrectnessWorkers(int n);

        /// Set how long completed/failed/cancelled jobs are retained before cleanup.
        void setJobRetentionSeconds(int sec);

        /**
         * @brief Execute a test job (cached workflow).
         *
         * Request must contain test_id and test_dir (or test_git_url).
         * Supports mode: "correctness", "performance", "all".
         */
        nlohmann::json execute(
            const nlohmann::json& request,
            std::function<void(JobQueue::JobStatus)> status_updater
        );

    private:
        /// Result of loading DLL plugins.
        struct loaded_plugins {
            PluginLoader loader;
            std::string load_error;
            int plugins_failed = 0;
            int plugins_total = 0;
        };

        /// Resolve paths for test and solution directories, clone from git if needed.
        struct ResolvedPaths {
            std::string test_dir;
            std::string solution_dir;
            std::filesystem::path temp_clone_dir;
        };
        ResolvedPaths resolvePaths(const nlohmann::json& request, const std::string& job_id);

        /// Load student solution + test plugin DLLs in correct order.
        loaded_plugins loadPlugins(
            const BuildService::SolutionBuildResult& sol_result,
            const BuildService::TestBuildResult& test_result
        );

        /// NUMA pin + run tests + assemble JSON result.
        void runTests(
            nlohmann::json& result,
            const std::string& job_id,
            const std::string& mode,
            int threads, int numa_node,
            TestRegistry& registry,
            par::MonitorContext& ctx,
            const std::string& plugin_load_error
        );

        /// Build info JSON from build results.
        static nlohmann::json formatBuildInfo(
            const BuildService::SolutionBuildResult& sol,
            const BuildService::TestBuildResult& test,
            const std::string& plugin_load_error
        );

        std::string exe_dir_;
        int correctness_workers_;
        BuildService build_service_;
        std::unique_ptr<JobQueue> queue_;
};
