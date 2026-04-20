#pragma once

/**
 * @file job_queue.h
 * @brief Single-queue job dispatcher with phase-level performance exclusivity.
 *
 * All submitted jobs land in one queue; N correctness worker threads pull jobs
 * from it concurrently. Each running job's Pipeline drives two phases:
 *   - correctness phase (parallel-safe across jobs),
 *   - performance phase (sandbox-exclusive: only one job in this phase at a
 *     time, no correctness phases concurrent with it).
 *
 * The phase-level exclusivity is enforced via enter_*_phase() / exit_*_phase()
 * which Pipeline calls around each lane. JobQueue owns the mutex/cv/flags;
 * other components access them only through these methods.
 */

#include <api_types.h>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <pipeline.h>
#include <progress_callback.h>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>

class JobQueue : public Pipeline::LaneController {
    public:
        struct JobInfo {
            std::string job_id;
            job_status status = job_status::queued;
            int queue_position = 0;
            nlohmann::json request;
            nlohmann::json result;
            std::string error;
            std::chrono::steady_clock::time_point submitted_at;
            std::chrono::steady_clock::time_point started_at;
            std::chrono::steady_clock::time_point finished_at;
        };

        /// Function that executes a single job and returns the result JSON.
        /// Called from a worker thread. May throw on failure.
        using JobExecutor = std::function<
            nlohmann::json(
                const nlohmann::json& request,
                std::function<void(job_status)> status_updater,
                progress::callback on_progress
            )
        >;

        /// Optional callback invoked after a job completes (success or failure).
        using CompletionCallback = std::function<void(const nlohmann::json& result)>;

        /**
     * @brief Construct the queue.
     * @param executor Function called for each job.
     * @param correctness_workers Number of concurrent correctness worker threads.
     */
        explicit JobQueue(JobExecutor executor, int correctness_workers = 4);

        ~JobQueue();

        /// Set / replace the executor. Safe to call after construction as long
        /// as no jobs have been submitted yet (used by TestRunnerService to
        /// resolve the JobQueue <-> Pipeline <-> JobQueue cycle).
        void set_executor(JobExecutor executor);

        JobQueue(const JobQueue&) = delete;
        JobQueue& operator=(const JobQueue&) = delete;

        /// Submit a job for execution. Returns the assigned job_id.
        /// Mode is determined at execution time from assignment config.json.
        std::string submit(
            nlohmann::json request,
            CompletionCallback on_complete = {},
            progress::callback on_progress = {}
        );

        /// Cancel a queued job. Returns false if already running or not found.
        bool cancel(const std::string& job_id);

        /// Get a snapshot of a job's current state. Throws if job_id is unknown.
        JobInfo get_job_info(const std::string& job_id) const;

        /// Get an overview of the queue, active workers, and tracked jobs.
        nlohmann::json get_status() const;

        /// Dynamically resize the correctness worker thread pool.
        void resize_correctness_pool(int new_size);

        /// Set how long completed/failed/cancelled jobs are retained before cleanup.
        void set_job_retention_seconds(int sec);
        int job_retention_seconds() const { return job_retention_sec_; }

        // ---- Phase-level sandbox exclusivity --------------------------------
        // Called by Pipeline around its correctness / performance lanes. The
        // contract:
        //   - correctness phases can run concurrently across jobs;
        //   - a performance phase is sandbox-exclusive - no other phase runs;
        //   - when a worker requests perf, new correctness phases wait until
        //     in-flight correctness phases drain and the perf phase finishes.

        /// Block until no perf phase is running / pending, then enter correctness phase.
        void enter_correctness_phase() override;
        /// Leave correctness phase; may unblock waiting perf phase entries.
        void exit_correctness_phase() override;
        /// Block until perf phase is free AND all correctness phases drained, then enter.
        void enter_perf_phase() override;
        /// Leave perf phase; unblocks correctness / perf waiters.
        void exit_perf_phase() override;

    private:
        JobExecutor executor_;

        // Shared state (protected by mutex_)
        std::unordered_map<std::string, JobInfo> jobs_;
        std::unordered_map<std::string, CompletionCallback> callbacks_;
        std::unordered_map<std::string, progress::callback> progress_callbacks_;

        std::deque<std::string> correctness_queue_;

        // Scheduling state
        int active_correctness_jobs_ =
            0;     ///< Number of running jobs (any phase) - for queue position labels / status.
        int active_correctness_phases_ = 0;   ///< Number of jobs currently INSIDE correctness phase.
        int perf_pending_ = 0;                ///< Number of jobs waiting to enter perf phase.
        bool perf_running_ = false;            ///< True while a perf phase is executing.

        mutable std::mutex mutex_;
        std::condition_variable queue_cv_;     ///< Wakes correctness workers for queue dequeue.
        std::condition_variable phase_cv_;     ///< Wakes phase-entry waiters.

        bool stop_ = false;
        int drain_count_ = 0;        ///< Number of correctness workers to drain (for pool downsizing).
        int job_retention_sec_ = 300;///< How long to keep terminal jobs before cleanup.

        // Worker threads
        std::vector<std::thread> correctness_workers_;

        void correctness_worker_loop();
        void execute_job(const std::string& job_id);
        void update_queue_positions();

        /// Remove completed/failed/cancelled jobs older than job_retention_sec_.
        /// Caller must hold mutex_.
        void cleanup_old_jobs();
};
