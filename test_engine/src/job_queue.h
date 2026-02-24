#pragma once

/**
 * @file job_queue.h
 * @brief Two-lane job queue: concurrent correctness + serial performance.
 *
 * Correctness jobs run concurrently via a thread pool.
 * Performance jobs run one at a time with exclusive CPU access
 * (no correctness jobs running simultaneously).
 */

#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>

class JobQueue {
    public:
        enum class JobStatus {
            QUEUED,
            BUILDING,
            RUNNING,
            COMPLETED,
            FAILED,
            CANCELLED
        };

        struct JobInfo {
            std::string job_id;
            JobStatus status = JobStatus::QUEUED;
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
            nlohmann::json(const nlohmann::json & request, std::function < void(JobStatus) > status_updater)
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

        JobQueue(const JobQueue&) = delete;
        JobQueue& operator=(const JobQueue&) = delete;

        /// Submit a job for execution. Returns the assigned job_id.
		/// Lane (correctness/performance) is determined by the "mode" field in request.
		/// mode "all" routes to the performance lane (exclusive CPU access).
        std::string submit(nlohmann::json request, CompletionCallback on_complete = {});

        /// Cancel a queued job. Returns false if already running or not found.
        bool cancel(const std::string& job_id);

        /// Get a snapshot of a job's current state. Throws if job_id is unknown.
        JobInfo getJobInfo(const std::string& job_id) const;

        /// Get an overview of all queues, active workers, and tracked jobs.
        nlohmann::json getStatus() const;

        static std::string statusToString(JobStatus s);

        /// Dynamically resize the correctness worker thread pool.
        /// If new_size > current: spawn additional workers.
        /// If new_size < current: mark excess workers for draining.
        void resizeCorrectnessPool(int new_size);

        /// Set how long completed/failed/cancelled jobs are retained before cleanup.
        void setJobRetentionSeconds(int sec);

    private:
        JobExecutor executor_;

        // Shared state (protected by mutex_)
        std::unordered_map<std::string, JobInfo> jobs_;
        std::unordered_map<std::string, CompletionCallback> callbacks_;
        std::string current_perf_job_id_;

        // Two lanes
        std::deque<std::string> correctness_queue_;
        std::deque<std::string> performance_queue_;

        // Scheduling state
        int active_correctness_ = 0;   ///< Number of running correctness jobs.
        bool perf_running_ = false;     ///< True while a performance job is executing.

        mutable std::mutex mutex_;
        std::condition_variable correctness_cv_;  ///< Wakes correctness workers.
        std::condition_variable performance_cv_;  ///< Wakes the performance worker.
        bool stop_ = false;
        int drain_count_ = 0;  ///< Number of correctness workers to drain (for pool downsizing).
        int job_retention_sec_ = 300;  ///< How long to keep terminal jobs before cleanup.

        // Worker threads
        std::vector<std::thread> correctness_workers_;
        std::thread performance_worker_;

        void correctnessWorkerLoop();
        void performanceWorkerLoop();
        void executeJob(const std::string& job_id);
        void updateQueuePositions();

        /// Remove completed/failed/cancelled jobs older than job_retention_sec_.
        /// Caller must hold mutex_.
        void cleanupOldJobs();
};