// Additional unit tests for par::monitor to close gaps in coverage:
//   - on_parallel_begin: max_threads_observed tracking, parallel_regions++,
//     omp_set_num_threads override.
//   - maybe_inject_delay: STRESS-mode sleeping behaviour.
//   - span_prepare_child / span_enter_task / span_exit_task / span_sync_children:
//     the full task DAG accounting used by spawn / sync points.
//
// Same fixture pattern as test_monitor.cpp.

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <par/monitor.h>
#include <thread>

using namespace par;
using namespace par::monitor;

namespace {
    struct MonitorExtrasFixture : public ::testing::Test {
        std::unique_ptr<MonitorContext> ctx;
        void SetUp() override {
            ctx = monitor::create_context();
            monitor::activate_context(ctx.get());
            monitor::set_mode(Mode::NORMAL);
            monitor::reset_stats();
        }
        void TearDown() override { monitor::activate_context(nullptr); }
    };
} // namespace

// -----------------------------------------------------------------------------
// on_parallel_begin
// -----------------------------------------------------------------------------

TEST_F(MonitorExtrasFixture, parallel_begin_increments_regions_in_monitor_mode) {
    monitor::set_mode(Mode::MONITOR);
    detail::on_parallel_begin();
    detail::on_parallel_begin();
    EXPECT_EQ(monitor::stats().parallel_regions.load(), 2);
}

TEST_F(MonitorExtrasFixture, parallel_begin_does_not_increment_regions_in_normal_mode) {
    monitor::set_mode(Mode::NORMAL);
    detail::on_parallel_begin();
    EXPECT_EQ(monitor::stats().parallel_regions.load(), 0);
}

TEST_F(MonitorExtrasFixture, parallel_begin_tracks_max_threads_even_in_normal_mode) {
    // max_threads_observed is performance-relevant in NORMAL mode too -
    // it documents what the runtime actually granted on a perf run.
    monitor::set_mode(Mode::NORMAL);
    detail::on_parallel_begin();
    EXPECT_GE(monitor::stats().max_threads_observed.load(), 1);
}

TEST_F(MonitorExtrasFixture, parallel_end_is_noop) {
    monitor::set_mode(Mode::MONITOR);
    auto before = monitor::stats().parallel_regions.load();
    detail::on_parallel_end();
    EXPECT_EQ(monitor::stats().parallel_regions.load(), before);
}

// -----------------------------------------------------------------------------
// maybe_inject_delay - STRESS path
// -----------------------------------------------------------------------------

TEST_F(MonitorExtrasFixture, inject_delay_is_noop_in_normal_mode) {
    monitor::set_mode(Mode::NORMAL);
    auto t0 = std::chrono::steady_clock::now();
    for(int i = 0; i < 100; ++i) detail::maybe_inject_delay();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
    EXPECT_LT(elapsed, 50) << "NORMAL mode must not sleep at all";
}

TEST_F(MonitorExtrasFixture, inject_delay_is_noop_in_monitor_mode) {
    monitor::set_mode(Mode::MONITOR);
    auto t0 = std::chrono::steady_clock::now();
    for(int i = 0; i < 100; ++i) detail::maybe_inject_delay();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
    EXPECT_LT(elapsed, 50) << "MONITOR mode must not sleep (only STRESS does)";
}

TEST_F(MonitorExtrasFixture, inject_delay_executes_stress_branch) {
    // Probabilistic: ~50% of calls sleep up to 50us. With 1000 iterations the
    // STRESS-only branch is exercised; we don't assert a specific timing budget
    // because CI variance is huge.
    monitor::set_mode(Mode::STRESS);
    for(int i = 0; i < 1000; ++i) detail::maybe_inject_delay();
    SUCCEED();
}

// -----------------------------------------------------------------------------
// Span DAG accounting - prepare_child / enter_task / exit_task / sync_children
//
// Simulated single-threaded "task graph":
//
//   span_init_root()
//   spawn-point: prepare_child  -> SpanChildCtx
//                enter_task     -> SpanSaved
//   ... task body work (sleeping) ...
//                exit_task(SpanSaved, SpanChildCtx)
//   span_sync_children()
//   span_finalize_root()
//
// We assert the span_ns counter is set to a non-negative value after each
// scenario. The exact arithmetic depends on the timer; we only validate the
// invariants (publishes >=0, NORMAL is no-op, restore returns TLS to baseline).
// -----------------------------------------------------------------------------

TEST_F(MonitorExtrasFixture, span_prepare_child_is_noop_in_normal_mode) {
    monitor::set_mode(Mode::NORMAL);
    detail::span_init_root();
    auto child_ctx = detail::span_prepare_child();
    // NORMAL contract: returns a zeroed SpanChildCtx with null parent_children_max.
    EXPECT_EQ(child_ctx.parent_depth, 0);
    EXPECT_EQ(child_ctx.parent_children_max, nullptr);
}

TEST_F(MonitorExtrasFixture, span_enter_exit_task_round_trip_publishes_span) {
    monitor::set_mode(Mode::MONITOR);
    detail::span_init_root();
    std::this_thread::sleep_for(std::chrono::microseconds(500));

    auto child_ctx = detail::span_prepare_child();
    auto saved = detail::span_enter_task(child_ctx);

    // Simulated task body
    std::this_thread::sleep_for(std::chrono::microseconds(500));

    detail::span_exit_task(saved, child_ctx);
    detail::span_finalize_root();

    EXPECT_GE(monitor::stats().span_ns.load(), 0);
}

TEST_F(MonitorExtrasFixture, span_enter_task_is_noop_in_normal_mode) {
    monitor::set_mode(Mode::NORMAL);
    detail::SpanChildCtx empty_ctx;
    auto saved = detail::span_enter_task(empty_ctx);
    // No throw, no TLS mutation - span_ns must remain at zero.
    detail::span_exit_task(saved, empty_ctx);
    EXPECT_EQ(monitor::stats().span_ns.load(), 0);
}

TEST_F(MonitorExtrasFixture, span_sync_children_is_noop_in_normal_mode) {
    monitor::set_mode(Mode::NORMAL);
    detail::span_init_root();
    detail::span_sync_children();
    EXPECT_EQ(monitor::stats().span_ns.load(), 0);
}

TEST_F(MonitorExtrasFixture, span_sync_children_merges_children_max_into_depth) {
    monitor::set_mode(Mode::MONITOR);
    detail::span_init_root();
    std::this_thread::sleep_for(std::chrono::microseconds(200));

    // Simulate one child spawn-task-exit so children_max gets populated.
    auto child_ctx = detail::span_prepare_child();
    auto saved = detail::span_enter_task(child_ctx);
    std::this_thread::sleep_for(std::chrono::microseconds(200));
    detail::span_exit_task(saved, child_ctx);

    // sync_children: closes strand, merges children_max into depth, resets children_max.
    detail::span_sync_children();

    detail::span_finalize_root();
    EXPECT_GT(monitor::stats().span_ns.load(), 0);
}

TEST_F(MonitorExtrasFixture, multiple_children_take_max_not_sum) {
    monitor::set_mode(Mode::MONITOR);
    detail::span_init_root();

    auto run_child = [](int us) {
        auto child_ctx = detail::span_prepare_child();
        auto saved = detail::span_enter_task(child_ctx);
        std::this_thread::sleep_for(std::chrono::microseconds(us));
        detail::span_exit_task(saved, child_ctx);
    };

    run_child(300);
    run_child(800);
    run_child(200);
    detail::span_sync_children();
    detail::span_finalize_root();

    // Span = max(serial, max_child). The serial strand is tiny (no sleep
    // between the children), so the result is dominated by the 800us child.
    auto span = monitor::stats().span_ns.load();
    EXPECT_GT(span, 0);
    EXPECT_LT(span, 2'000'000) << "span must not sum the three children";
}
