// Unit tests for JobQueue - the dispatcher that fans submitted jobs out to
// correctness worker threads with a phase-level perf-exclusivity contract.
//
// We avoid race-condition-style tests (those are inherently flaky); instead
// we drive the queue with a controllable JobExecutor and assert on observable
// state (job_id uniqueness, status JSON, cancel semantics, retention).

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <gtest/gtest.h>
#include <job_queue.h>
#include <mutex>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <thread>


namespace {

    using namespace std::chrono_literals;

    /// Wait until `pred()` is true or `deadline` passes. Returns true on success.
    /// Used to synchronize with the worker thread without sleep loops.
    template<typename Pred>
    bool wait_until(Pred pred, std::chrono::steady_clock::duration timeout = 2s) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while(std::chrono::steady_clock::now() < deadline) {
            if(pred()) return true;
            std::this_thread::sleep_for(5ms);
        }
        return pred();
    }

    /// Executor that immediately returns a "completed" result containing the request.
    JobQueue::JobExecutor instant_executor() {
        return [](
            const nlohmann::json& request,
            std::function<void(job_status)> /*status*/,
            progress::callback /*progress*/
        ) {
            return nlohmann::json{
                {"jobId", request.value("jobId", "")},
                {"status", "completed"}
            };
        };
    }

    /// Executor that throws - exercises the catch path of execute_job().
    JobQueue::JobExecutor throwing_executor() {
        return [](
            const nlohmann::json& /*req*/,
            std::function<void(job_status)> /*status*/,
            progress::callback /*progress*/
        ) -> nlohmann::json {
            throw std::runtime_error("boom");
        };
    }

    /// Executor that blocks until `release` is signaled. Lets us probe queue state
    /// while a job is in flight without race conditions.
    struct BlockingExecutor {
        std::mutex m;
        std::condition_variable cv;
        bool released = false;
        std::atomic<int> started_count{0};

        JobQueue::JobExecutor make() {
            return [this](
                const nlohmann::json& request,
                std::function<void(job_status)> status,
                progress::callback /*progress*/
            ) {
                started_count.fetch_add(1);
                status(job_status::running);

                std::unique_lock lock(m);
                cv.wait(lock, [&] { return released; });

                return nlohmann::json{
                    {"jobId", request.value("jobId", "")},
                    {"status", "completed"}
                };
            };
        }

        void release() {
            {
                std::lock_guard lock(m);
                released = true;
            }
            cv.notify_all();
        }
    };

} // namespace

// -----------------------------------------------------------------------------
// submit() - id generation + queueing semantics
// -----------------------------------------------------------------------------

TEST(JobQueue, submit_generates_unique_id_when_not_provided) {
    JobQueue q(instant_executor(), 0);  // no workers: jobs stay queued
    auto id1 = q.submit(nlohmann::json::object());
    auto id2 = q.submit(nlohmann::json::object());
    EXPECT_FALSE(id1.empty());
    EXPECT_FALSE(id2.empty());
    EXPECT_NE(id1, id2);
    EXPECT_EQ(id1.substr(0, 2), "j-");  // matches generate_job_id() format
}

TEST(JobQueue, submit_keeps_caller_provided_id) {
    JobQueue q(instant_executor(), 0);
    auto id = q.submit(nlohmann::json{{"jobId", "custom-1"}});
    EXPECT_EQ(id, "custom-1");
}

TEST(JobQueue, submit_assigns_increasing_queue_positions) {
    JobQueue q(instant_executor(), 0);
    auto a = q.submit(nlohmann::json::object());
    auto b = q.submit(nlohmann::json::object());
    auto c = q.submit(nlohmann::json::object());
    EXPECT_EQ(q.get_job_info(a).queue_position, 1);
    EXPECT_EQ(q.get_job_info(b).queue_position, 2);
    EXPECT_EQ(q.get_job_info(c).queue_position, 3);
    EXPECT_EQ(q.get_job_info(a).status, job_status::queued);
}

// -----------------------------------------------------------------------------
// cancel()
// -----------------------------------------------------------------------------

TEST(JobQueue, cancel_queued_job_removes_it_and_updates_positions) {
    JobQueue q(instant_executor(), 0);
    auto a = q.submit(nlohmann::json::object());
    auto b = q.submit(nlohmann::json::object());
    auto c = q.submit(nlohmann::json::object());

    EXPECT_TRUE(q.cancel(b));
    EXPECT_EQ(q.get_job_info(b).status, job_status::cancelled);

    // Surviving jobs renumbered.
    EXPECT_EQ(q.get_job_info(a).queue_position, 1);
    EXPECT_EQ(q.get_job_info(c).queue_position, 2);
}

TEST(JobQueue, cancel_unknown_job_returns_false) {
    JobQueue q(instant_executor(), 0);
    EXPECT_FALSE(q.cancel("does-not-exist"));
}

TEST(JobQueue, cancel_running_job_returns_false) {
    BlockingExecutor blocker;
    JobQueue q(blocker.make(), 1);
    auto id = q.submit(nlohmann::json::object());

    // Wait until the worker actually picked the job up.
    ASSERT_TRUE(wait_until([&]{ return blocker.started_count.load() == 1; }));

    EXPECT_FALSE(q.cancel(id))
        << "JobQueue::cancel is documented to refuse non-queued jobs";

    blocker.release();
}

TEST(JobQueue, cancel_terminal_job_returns_false) {
    JobQueue q(instant_executor(), 1);
    auto id = q.submit(nlohmann::json::object());

    ASSERT_TRUE(
        wait_until([&]{
            return q.get_job_info(id).status == job_status::completed;
            })
    );

    EXPECT_FALSE(q.cancel(id))
        << "completed jobs can't be cancelled retroactively";
}

// -----------------------------------------------------------------------------
// get_job_info
// -----------------------------------------------------------------------------

TEST(JobQueue, get_job_info_throws_on_unknown_id) {
    JobQueue q(instant_executor(), 0);
    EXPECT_THROW(q.get_job_info("missing"), std::runtime_error);
}

// -----------------------------------------------------------------------------
// Executor lifecycle - result propagation + exception handling
// -----------------------------------------------------------------------------

TEST(JobQueue, completed_job_status_and_result_propagate) {
    JobQueue q(instant_executor(), 1);
    auto id = q.submit(nlohmann::json::object());

    ASSERT_TRUE(
        wait_until([&]{
            return q.get_job_info(id).status == job_status::completed;
            })
    );
    auto info = q.get_job_info(id);
    EXPECT_EQ(info.status, job_status::completed);
    EXPECT_EQ(info.queue_position, -1);
    EXPECT_EQ(info.result.value("status", ""), "completed");
}

TEST(JobQueue, executor_exception_marks_job_failed_with_error) {
    JobQueue q(throwing_executor(), 1);
    auto id = q.submit(nlohmann::json::object());

    ASSERT_TRUE(
        wait_until([&]{
            return q.get_job_info(id).status == job_status::failed;
            })
    );
    EXPECT_EQ(q.get_job_info(id).error, "boom");
}

TEST(JobQueue, completion_callback_receives_result_json) {
    JobQueue q(instant_executor(), 1);

    std::promise<nlohmann::json> seen;
    auto fut = seen.get_future();
    q.submit(
        nlohmann::json::object(),
        [&](const nlohmann::json& r) {
            seen.set_value(r);
        }
    );
    ASSERT_EQ(fut.wait_for(2s), std::future_status::ready);
    auto j = fut.get();
    EXPECT_EQ(j.value("status", ""), "completed");
}

TEST(JobQueue, no_executor_set_marks_job_failed) {
    JobQueue q(nullptr, 1);
    auto id = q.submit(nlohmann::json::object());

    ASSERT_TRUE(
        wait_until([&]{
            return q.get_job_info(id).status == job_status::failed;
            })
    );
    EXPECT_NE(q.get_job_info(id).error.find("executor not set"), std::string::npos);
}

// -----------------------------------------------------------------------------
// get_status - JSON shape
// -----------------------------------------------------------------------------

TEST(JobQueue, get_status_reports_idle_with_no_jobs) {
    JobQueue q(instant_executor(), 1);
    auto s = q.get_status();
    EXPECT_EQ(s.value("status", ""), "idle");
    EXPECT_EQ(s.value("queueSize", -1), 0);
    EXPECT_EQ(s.value("activeJobs", -1), 0);
    EXPECT_FALSE(s.value("perfPhaseRunning", true));
}

TEST(JobQueue, get_status_lists_queued_jobs_in_order) {
    JobQueue q(instant_executor(), 0);  // no workers - jobs stay queued
    auto a = q.submit(nlohmann::json::object());
    auto b = q.submit(nlohmann::json::object());

    auto s = q.get_status();
    EXPECT_EQ(s.value("queueSize", -1), 2);
    ASSERT_TRUE(s["jobs"].is_array());
    ASSERT_EQ(s["jobs"].size(), 2u);
    EXPECT_EQ(s["jobs"][0]["jobId"], a);
    EXPECT_EQ(s["jobs"][0]["position"], 1);
    EXPECT_EQ(s["jobs"][1]["jobId"], b);
    EXPECT_EQ(s["jobs"][1]["position"], 2);
}

TEST(JobQueue, get_status_reports_busy_during_active_job) {
    BlockingExecutor blocker;
    JobQueue q(blocker.make(), 1);
    auto id = q.submit(nlohmann::json::object());

    ASSERT_TRUE(wait_until([&]{ return blocker.started_count.load() == 1; }));
    auto s = q.get_status();
    EXPECT_EQ(s.value("status", ""), "busy");
    EXPECT_EQ(s.value("activeJobs", 0), 1);
    EXPECT_EQ(s.value("queueSize", -1), 0);

    blocker.release();
}

// -----------------------------------------------------------------------------
// Pool / retention configuration
// -----------------------------------------------------------------------------

TEST(JobQueue, resize_pool_grows_worker_count) {
    JobQueue q(instant_executor(), 1);
    q.resize_correctness_pool(4);
    auto s = q.get_status();
    EXPECT_EQ(s.value("maxCorrectnessWorkers", 0), 4);
}

TEST(JobQueue, resize_pool_floor_at_one) {
    JobQueue q(instant_executor(), 2);
    q.resize_correctness_pool(0);   // below-minimum input
    auto s = q.get_status();
    // Resize triggers draining for downsize; the count seen by external API
    // is `correctness_workers_.size() - drain_count_`. Either way it must be >= 1.
    EXPECT_GE(s.value("maxCorrectnessWorkers", -1), 1);
}

TEST(JobQueue, retention_seconds_clamped_to_minimum) {
    JobQueue q(instant_executor(), 0);
    q.set_job_retention_seconds(0);
    EXPECT_EQ(q.job_retention_seconds(), 1)
        << "set_job_retention_seconds() must clamp non-positive input up to 1";

    q.set_job_retention_seconds(-100);
    EXPECT_EQ(q.job_retention_seconds(), 1);

    q.set_job_retention_seconds(123);
    EXPECT_EQ(q.job_retention_seconds(), 123);
}
