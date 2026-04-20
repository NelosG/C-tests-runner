#include <api_types.h>
#include <iostream>
#include <job_queue.h>
#include <log_utils.h>
#include <random>
#include <sstream>
#include <stdexcept>

/// Generate a short random job ID (e.g. "j-a3f7b2c1").
static std::string generate_job_id() {
    thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<uint32_t> dist;
    std::ostringstream ss;
    ss << "j-" << std::hex << dist(gen);
    return ss.str();
}

JobQueue::JobQueue(JobExecutor executor, const int correctness_workers)
    : executor_(std::move(executor)) {
    for(int i = 0; i < correctness_workers; ++i) {
        correctness_workers_.emplace_back([this]() { correctness_worker_loop(); });
    }
}

void JobQueue::set_executor(JobExecutor executor) {
    std::lock_guard lock(mutex_);
    executor_ = std::move(executor);
}

JobQueue::~JobQueue() {
    {
        std::lock_guard lock(mutex_);
        stop_ = true;
    }
    queue_cv_.notify_all();
    phase_cv_.notify_all();

    for(auto& w : correctness_workers_) {
        if(w.joinable()) w.join();
    }
}

std::string JobQueue::submit(
    nlohmann::json request,
    CompletionCallback on_complete,
    progress::callback on_progress
) {
    std::string job_id = request.value("jobId", "");
    if(job_id.empty()) {
        job_id = generate_job_id();
        request["jobId"] = job_id;
    }
    // Mode is determined by assignment config.json at execution time. We don't
    // know it at submit, so all jobs share one queue; per-phase exclusivity is
    // applied by Pipeline via enter_*_phase() / exit_*_phase().
    std::lock_guard lock(mutex_);

    cleanup_old_jobs();

    JobInfo info;
    info.job_id = job_id;
    info.status = job_status::queued;
    info.request = std::move(request);
    info.submitted_at = std::chrono::steady_clock::now();

    if(on_complete) {
        callbacks_[job_id] = std::move(on_complete);
    }
    if(on_progress) {
        progress_callbacks_[job_id] = std::move(on_progress);
    }

    info.queue_position = static_cast<int>(correctness_queue_.size()) + 1;
    jobs_[job_id] = std::move(info);
    correctness_queue_.push_back(job_id);
    queue_cv_.notify_one();
    LOG("JobQueue") << job_id << " queued (pos=" << correctness_queue_.size() << ")\n";

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

    correctness_queue_.erase(
        std::remove(correctness_queue_.begin(), correctness_queue_.end(), job_id),
        correctness_queue_.end()
    );

    callbacks_.erase(job_id);
    progress_callbacks_.erase(job_id);
    update_queue_positions();
    return true;
}

JobQueue::JobInfo JobQueue::get_job_info(const std::string& job_id) const {
    std::lock_guard lock(mutex_);
    auto it = jobs_.find(job_id);
    if(it == jobs_.end()) {
        throw std::runtime_error("Unknown job_id: " + job_id);
    }
    return it->second;
}

nlohmann::json JobQueue::get_status() const {
    std::lock_guard lock(mutex_);

    nlohmann::json status;
    bool busy = perf_running_ || active_correctness_jobs_ > 0;
    status["status"] = to_string(busy ? queue_status::busy : queue_status::idle);
    status["queueSize"] = correctness_queue_.size();
    status["activeJobs"] = active_correctness_jobs_;
    status["maxCorrectnessWorkers"] = static_cast<int>(correctness_workers_.size()) - drain_count_;
    status["perfPhaseRunning"] = perf_running_;
    status["perfPhasePending"] = perf_pending_;

    auto jobs_arr = nlohmann::json::array();
    auto job_entry = [&](const JobInfo& job, int position = 0) {
        nlohmann::json entry = {
            {"jobId", job.job_id},
            {"status", to_string(job.status)}
        };
        if(position > 0) entry["position"] = position;
        return entry;
    };

    // Currently running jobs (we don't track which one is in which phase here)
    for(const auto& [id, info] : jobs_) {
        if(info.status == job_status::building || info.status == job_status::running) {
            jobs_arr.push_back(job_entry(info));
        }
    }

    // Queued jobs
    for(size_t i = 0; i < correctness_queue_.size(); ++i) {
        auto it = jobs_.find(correctness_queue_[i]);
        if(it != jobs_.end()) {
            jobs_arr.push_back(job_entry(it->second, static_cast<int>(i) + 1));
        }
    }

    status["jobs"] = jobs_arr;
    return status;
}

void JobQueue::cleanup_old_jobs() {
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
            progress_callbacks_.erase(it->first);
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

void JobQueue::update_queue_positions() {
    for(size_t i = 0; i < correctness_queue_.size(); ++i) {
        jobs_[correctness_queue_[i]].queue_position = static_cast<int>(i) + 1;
    }
}

// ============================================================================
// Phase-level exclusivity (called from Pipeline)
// ============================================================================

void JobQueue::enter_correctness_phase() {
    std::unique_lock lock(mutex_);
    phase_cv_.wait(
        lock,
        [&] {
            return stop_ || (!perf_running_ && perf_pending_ == 0);
        }
    );
    // If we woke because of shutdown, don't start a new phase - return so
    // the caller's RAII guard (exit_correctness_phase) doesn't decrement
    // a counter we never incremented.
    if(stop_) return;
    ++active_correctness_phases_;
}

void JobQueue::exit_correctness_phase() {
    {
        std::lock_guard lock(mutex_);
        if(active_correctness_phases_ > 0) --active_correctness_phases_;
    }
    phase_cv_.notify_all();
}

void JobQueue::enter_perf_phase() {
    std::unique_lock lock(mutex_);
    ++perf_pending_;
    // Wake correctness workers / waiting phases so they see perf_pending_ and re-check.
    phase_cv_.notify_all();
    queue_cv_.notify_all();
    phase_cv_.wait(
        lock,
        [&] {
            return stop_ || (!perf_running_ && active_correctness_phases_ == 0);
        }
    );
    --perf_pending_;
    // Shutdown case: don't lock the perf lane. exit_perf_phase will still be
    // called by the caller's RAII guard, but it'll just clear an already-clear
    // flag - safe no-op.
    if(stop_) return;
    perf_running_ = true;
}

void JobQueue::exit_perf_phase() {
    {
        std::lock_guard lock(mutex_);
        perf_running_ = false;
    }
    phase_cv_.notify_all();
    queue_cv_.notify_all();
}

// ============================================================================
// Pool resizing
// ============================================================================

void JobQueue::resize_correctness_pool(int new_size) {
    if(new_size < 1) new_size = 1;

    std::lock_guard lock(mutex_);
    int current = static_cast<int>(correctness_workers_.size()) - drain_count_;

    if(new_size == current) return;

    if(new_size > current) {
        int to_spawn = new_size - current;
        int reclaim = std::min(drain_count_, to_spawn);
        drain_count_ -= reclaim;
        to_spawn -= reclaim;
        for(int i = 0; i < to_spawn; ++i) {
            correctness_workers_.emplace_back([this]() { correctness_worker_loop(); });
        }
        std::cout << "[JobQueue] Resized correctness pool: " << current << " -> " << new_size << "\n";
    } else {
        drain_count_ += (current - new_size);
        queue_cv_.notify_all();
        std::cout << "[JobQueue] Resized correctness pool: " << current << " -> " << new_size
            << " (draining " << drain_count_ << " worker(s))\n";
    }
}

void JobQueue::set_job_retention_seconds(int sec) {
    if(sec < 1) sec = 1;
    std::lock_guard lock(mutex_);
    job_retention_sec_ = sec;
    std::cout << "[JobQueue] Job retention set to " << sec << "s\n";
}

// ============================================================================
// Worker loop
// ============================================================================

void JobQueue::correctness_worker_loop() {
    while(true) {
        std::string job_id;

        {
            std::unique_lock lock(mutex_);
            // Pause dequeue while a perf phase is running or pending - saves a
            // worker thread from immediately blocking at enter_correctness_phase().
            queue_cv_.wait(
                lock,
                [this]() {
                    return stop_ || drain_count_ > 0 ||
                        (!correctness_queue_.empty() && !perf_running_ && perf_pending_ == 0);
                }
            );

            if(drain_count_ > 0) {
                --drain_count_;
                return;
            }
            if(stop_ && correctness_queue_.empty()) return;
            if(correctness_queue_.empty() || perf_running_ || perf_pending_ > 0) continue;

            job_id = correctness_queue_.front();
            correctness_queue_.pop_front();
            ++active_correctness_jobs_;
            update_queue_positions();

            auto& info = jobs_[job_id];
            info.status = job_status::building;
            info.queue_position = 0;
            info.started_at = std::chrono::steady_clock::now();
        }

        execute_job(job_id);

        {
            std::lock_guard lock(mutex_);
            --active_correctness_jobs_;
        }
        phase_cv_.notify_all();
    }
}

void JobQueue::execute_job(const std::string& job_id) {
    nlohmann::json request;
    progress::callback on_progress;
    JobExecutor exec_copy;   // local copy -> safe against any future set_executor()
    {
        std::lock_guard lock(mutex_);
        request = jobs_[job_id].request;
        auto pit = progress_callbacks_.find(job_id);
        if(pit != progress_callbacks_.end()) on_progress = pit->second;
        exec_copy = executor_;
    }

    LOG("JobQueue") << job_id << " started\n";

    auto status_updater = [this, job_id](job_status new_status) {
        std::lock_guard lock(mutex_);
        jobs_[job_id].status = new_status;
    };

    nlohmann::json final_result;
    try {
        if(!exec_copy) throw std::runtime_error("JobQueue: executor not set");
        nlohmann::json result = exec_copy(request, status_updater, on_progress);

        std::string result_status = result.value("status", "");
        bool is_failed = (result_status == to_string(job_status::failed));

        std::lock_guard lock(mutex_);
        auto& info = jobs_[job_id];
        info.status = is_failed ? job_status::failed : job_status::completed;
        info.result = result;
        info.error = result.value("error", "");
        info.queue_position = -1;
        info.finished_at = std::chrono::steady_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            info.finished_at - info.started_at
        ).count();
        final_result = std::move(result);

        if(is_failed) {
            LOG_ERR("JobQueue") << job_id << " failed (" << elapsed << "ms): "
                << final_result.value("error", "unknown") << "\n";
        } else {
            LOG("JobQueue") << job_id << " completed (" << elapsed << "ms)\n";
        }
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
        LOG_ERR("JobQueue") << job_id << " failed (" << elapsed << "ms) with exception: " << e.what() << "\n";
    }

    CompletionCallback cb;
    {
        std::lock_guard lock(mutex_);
        auto it = callbacks_.find(job_id);
        if(it != callbacks_.end()) {
            cb = std::move(it->second);
            callbacks_.erase(it);
        }
        progress_callbacks_.erase(job_id);
    }
    if(cb) {
        try { cb(final_result); } catch(const std::exception& e) {
            std::cerr << "[JobQueue] Completion callback error for " << job_id << ": " << e.what() << "\n";
        }
    }
}
