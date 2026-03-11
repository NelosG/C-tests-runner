#include <api_types.h>
#include <iostream>
#include <job_queue.h>
#include <random>
#include <sstream>
#include <stdexcept>

/// Generate a short random job ID (e.g. "j-a3f7b2c1").
static std::string generateJobId() {
    thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<uint32_t> dist;
    std::ostringstream ss;
    ss << "j-" << std::hex << dist(gen);
    return ss.str();
}

JobQueue::JobQueue(JobExecutor executor, const int correctness_workers)
    : executor_(std::move(executor)) {
    // Start correctness worker pool
    for(int i = 0; i < correctness_workers; ++i) {
        correctness_workers_.emplace_back([this]() { correctnessWorkerLoop(); });
    }
    // Start single performance worker
    performance_worker_ = std::thread([this]() { performanceWorkerLoop(); });
}

JobQueue::~JobQueue() {
    {
        std::lock_guard lock(mutex_);
        stop_ = true;
    }
    correctness_cv_.notify_all();
    performance_cv_.notify_all();

    for(auto& w : correctness_workers_) {
        if(w.joinable()) w.join();
    }
    if(performance_worker_.joinable()) {
        performance_worker_.join();
    }
}

std::string JobQueue::submit(nlohmann::json request, CompletionCallback on_complete) {
    std::string job_id = request.value("jobId", "");
    if(job_id.empty()) {
        job_id = generateJobId();
        request["jobId"] = job_id;
    }
    const std::string mode = request.value("mode", to_string(test_mode::correctness));

    std::lock_guard lock(mutex_);

    cleanupOldJobs();

    JobInfo info;
    info.job_id = job_id;
    info.status = job_status::queued;
    info.request = std::move(request);
    info.submitted_at = std::chrono::steady_clock::now();

    if(on_complete) {
        callbacks_[job_id] = std::move(on_complete);
    }

    // mode "all" and "performance" both need exclusive CPU access
    if(mode == to_string(test_mode::performance) || mode == to_string(test_mode::all)) {
        info.queue_position = static_cast<int>(performance_queue_.size()) + 1;
        jobs_[job_id] = std::move(info);
        performance_queue_.push_back(job_id);
        performance_cv_.notify_one();
        std::cout << "[JobQueue] " << job_id << " queued -> performance lane"
            << " (mode=" << mode << ", pos=" << performance_queue_.size() << ")\n";
    } else {
        info.queue_position = static_cast<int>(correctness_queue_.size()) + 1;
        jobs_[job_id] = std::move(info);
        correctness_queue_.push_back(job_id);
        correctness_cv_.notify_one();
        std::cout << "[JobQueue] " << job_id << " queued -> correctness lane"
            << " (pos=" << correctness_queue_.size() << ")\n";
    }

    return job_id;
}

bool JobQueue::cancel(const std::string& job_id) {
    std::lock_guard lock(mutex_);

    auto it = jobs_.find(job_id);
    if(it == jobs_.end()) return false;
    if(it->second.status != job_status::queued) return false;

    it->second.status = job_status::cancelled;
    it->second.queue_position = -1;
    it->second.finished_at = std::chrono::steady_clock::now();

    // Remove from whichever queue it's in
    correctness_queue_.erase(
        std::remove(correctness_queue_.begin(), correctness_queue_.end(), job_id),
        correctness_queue_.end()
    );
    performance_queue_.erase(
        std::remove(performance_queue_.begin(), performance_queue_.end(), job_id),
        performance_queue_.end()
    );

    callbacks_.erase(job_id);
    updateQueuePositions();
    return true;
}

JobQueue::JobInfo JobQueue::getJobInfo(const std::string& job_id) const {
    std::lock_guard lock(mutex_);
    auto it = jobs_.find(job_id);
    if(it == jobs_.end()) {
        throw std::runtime_error("Unknown job_id: " + job_id);
    }
    return it->second;
}

nlohmann::json JobQueue::getStatus() const {
    std::lock_guard lock(mutex_);

    nlohmann::json status;
    bool busy = perf_running_ || active_correctness_ > 0;
    status["status"] = to_string(busy ? queue_status::busy : queue_status::idle);
    status["correctnessQueueSize"] = correctness_queue_.size();
    status["performanceQueueSize"] = performance_queue_.size();
    status["activeCorrectness"] = active_correctness_;
    status["maxCorrectnessWorkers"] = static_cast<int>(correctness_workers_.size()) - drain_count_;
    status["perfRunning"] = perf_running_;

    if(!current_perf_job_id_.empty()) {
        status["currentPerfJob"] = current_perf_job_id_;
    }

    auto jobs_arr = nlohmann::json::array();

    // Helper: extract common fields from a job entry
    auto job_entry = [&](const JobInfo& job, const std::string& lane, int position = 0) {
        nlohmann::json entry = {
            {"jobId", job.job_id},
            {"status", to_string(job.status)},
            {"lane", lane}
        };
        if(position > 0) entry["position"] = position;

        if(!job.request.is_null()) {
            entry["mode"] = job.request.value("mode", to_string(test_mode::correctness));
            std::string sol_dir = job.request.value("solutionDir", "");
            if(!sol_dir.empty()) {
                auto pos = sol_dir.find_last_of("/\\");
                entry["solution"] = (pos != std::string::npos) ? sol_dir.substr(pos + 1) : sol_dir;
            }
        }
        return entry;
    };

    // Currently running perf job
    if(!current_perf_job_id_.empty()) {
        auto it = jobs_.find(current_perf_job_id_);
        if(it != jobs_.end()) {
            jobs_arr.push_back(job_entry(it->second, "performance"));
        }
    }

    // Queued correctness jobs
    for(size_t i = 0; i < correctness_queue_.size(); ++i) {
        auto it = jobs_.find(correctness_queue_[i]);
        if(it != jobs_.end()) {
            jobs_arr.push_back(job_entry(it->second, "correctness", static_cast<int>(i) + 1));
        }
    }

    // Queued performance jobs
    for(size_t i = 0; i < performance_queue_.size(); ++i) {
        auto it = jobs_.find(performance_queue_[i]);
        if(it != jobs_.end()) {
            jobs_arr.push_back(job_entry(it->second, "performance", static_cast<int>(i) + 1));
        }
    }

    status["jobs"] = jobs_arr;
    return status;
}

void JobQueue::cleanupOldJobs() {
    auto now = std::chrono::steady_clock::now();
    auto threshold = std::chrono::seconds(job_retention_sec_);
    int removed = 0;

    for(auto it = jobs_.begin(); it != jobs_.end();) {
        auto& info = it->second;
        bool is_terminal = info.status == job_status::completed
            || info.status == job_status::failed
            || info.status == job_status::cancelled;

        if(is_terminal && (now - info.finished_at) > threshold) {
            callbacks_.erase(it->first);
            it = jobs_.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }

    if(removed > 0) {
        std::cout << "[JobQueue] Cleaned up " << removed << " old job(s)\n";
    }
}

void JobQueue::updateQueuePositions() {
    for(size_t i = 0; i < correctness_queue_.size(); ++i) {
        jobs_[correctness_queue_[i]].queue_position = static_cast<int>(i) + 1;
    }
    for(size_t i = 0; i < performance_queue_.size(); ++i) {
        jobs_[performance_queue_[i]].queue_position = static_cast<int>(i) + 1;
    }
}

void JobQueue::executeJob(const std::string& job_id) {
    nlohmann::json request;
    {
        std::lock_guard lock(mutex_);
        request = jobs_[job_id].request;
    }

    std::string mode = request.value("mode", to_string(test_mode::correctness));
    std::cout << "[JobQueue] " << job_id << " started (mode=" << mode << ")\n";

    auto status_updater = [this, job_id](job_status new_status) {
        std::lock_guard lock(mutex_);
        jobs_[job_id].status = new_status;
    };

    nlohmann::json final_result;
    try {
        nlohmann::json result = executor_(request, status_updater);

        std::lock_guard lock(mutex_);
        auto& info = jobs_[job_id];
        info.status = job_status::completed;
        info.result = result;
        info.queue_position = -1;
        info.finished_at = std::chrono::steady_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            info.finished_at - info.started_at
        ).count();
        final_result = std::move(result);
        std::cout << "[JobQueue] " << job_id << " completed (" << elapsed << "ms)\n";
    } catch(const std::exception& e) {
        std::lock_guard lock(mutex_);
        auto& info = jobs_[job_id];
        info.status = job_status::failed;
        info.error = e.what();
        info.queue_position = -1;
        info.finished_at = std::chrono::steady_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            info.finished_at - info.started_at
        ).count();
        final_result = {{"jobId", job_id}, {"status", to_string(job_status::failed)}, {"error", e.what()}};
        std::cerr << "[JobQueue] " << job_id << " failed (" << elapsed << "ms): " << e.what() << "\n";
    }

    // Invoke completion callback outside the lock
    CompletionCallback cb;
    {
        std::lock_guard lock(mutex_);
        auto it = callbacks_.find(job_id);
        if(it != callbacks_.end()) {
            cb = std::move(it->second);
            callbacks_.erase(it);
        }
    }
    if(cb) {
        try { cb(final_result); } catch(const std::exception& e) {
            std::cerr << "[JobQueue] Completion callback error for " << job_id << ": " << e.what() << "\n";
        }
    }
}

void JobQueue::resizeCorrectnessPool(int new_size) {
    if(new_size < 1) new_size = 1;

    std::lock_guard lock(mutex_);
    int current = static_cast<int>(correctness_workers_.size()) - drain_count_;

    if(new_size == current) return;

    if(new_size > current) {
        // Spawn additional workers (first reclaim any pending drains)
        int to_spawn = new_size - current;
        int reclaim = std::min(drain_count_, to_spawn);
        drain_count_ -= reclaim;
        to_spawn -= reclaim;
        for(int i = 0; i < to_spawn; ++i) {
            correctness_workers_.emplace_back([this]() { correctnessWorkerLoop(); });
        }
        std::cout << "[JobQueue] Resized correctness pool: " << current << " -> " << new_size << "\n";
    } else {
        // Mark excess workers for draining
        drain_count_ += (current - new_size);
        correctness_cv_.notify_all();
        std::cout << "[JobQueue] Resized correctness pool: " << current << " -> " << new_size
            << " (draining " << drain_count_ << " worker(s))\n";
    }
}

void JobQueue::setJobRetentionSeconds(int sec) {
    if(sec < 1) sec = 1;
    std::lock_guard lock(mutex_);
    job_retention_sec_ = sec;
    std::cout << "[JobQueue] Job retention set to " << sec << "s\n";
}

void JobQueue::correctnessWorkerLoop() {
    while(true) {
        std::string job_id;

        {
            std::unique_lock lock(mutex_);
            correctness_cv_.wait(
                lock,
                [this]() {
                    return stop_ || drain_count_ > 0 ||
                        (!correctness_queue_.empty() && !perf_running_);
                }
            );

            if(drain_count_ > 0) {
                --drain_count_;
                return;
            }
            if(stop_&& correctness_queue_
            .
            empty()
            )
            return;
            if(correctness_queue_.empty() || perf_running_) continue;

            job_id = correctness_queue_.front();
            correctness_queue_.pop_front();
            ++active_correctness_;
            updateQueuePositions();

            auto& info = jobs_[job_id];
            info.status = job_status::building;
            info.queue_position = 0;
            info.started_at = std::chrono::steady_clock::now();
        }

        executeJob(job_id);

        {
            std::lock_guard lock(mutex_);
            --active_correctness_;
        }
        // Wake performance worker (it may be waiting for correctness to drain)
        performance_cv_.notify_one();
    }
}

void JobQueue::performanceWorkerLoop() {
    while(true) {
        std::string job_id;

        {
            std::unique_lock lock(mutex_);
            performance_cv_.wait(
                lock,
                [this]() {
                    return stop_ ||
                        (!performance_queue_.empty() && active_correctness_ == 0 && !perf_running_);
                }
            );

            if(stop_&& performance_queue_
            .
            empty()
            )
            return;
            if(performance_queue_.empty() || active_correctness_ != 0 || perf_running_) continue;

            job_id = performance_queue_.front();
            performance_queue_.pop_front();
            perf_running_ = true;
            current_perf_job_id_ = job_id;
            updateQueuePositions();

            auto& info = jobs_[job_id];
            info.status = job_status::building;
            info.queue_position = 0;
            info.started_at = std::chrono::steady_clock::now();
        }

        executeJob(job_id);

        {
            std::lock_guard lock(mutex_);
            perf_running_ = false;
            current_perf_job_id_.clear();
        }
        // Wake correctness workers (they may have been blocked by perf_running_)
        correctness_cv_.notify_all();
        // Also wake perf worker for next perf job if any
        performance_cv_.notify_one();
    }
}