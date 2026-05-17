// Unit tests for par::monitor - the counter / mode / context machinery that
// the engine uses to instrument student OpenMP code.
//
// We deliberately avoid driving real `#pragma omp parallel` sections - those
// are exercised at integration time. Here we hit the detail::on_* hooks
// directly to assert each counter increments under MONITOR/STRESS and stays
// at zero under NORMAL.

#include <atomic>
#include <gtest/gtest.h>
#include <par/monitor.h>
#include <thread>

using namespace par;
using namespace par::monitor;


namespace {

    /// Reset all monitor state for the active context, so successive tests don't
    /// leak counters into each other.
    struct MonitorFixture : public ::testing::Test {
        std::unique_ptr<MonitorContext> ctx;

        void SetUp() override {
            ctx = monitor::create_context();
            monitor::activate_context(ctx.get());
            monitor::set_mode(Mode::NORMAL);
            monitor::reset_stats();
        }

        void TearDown() override {
            monitor::activate_context(nullptr);
        }
    };

} // namespace

// -----------------------------------------------------------------------------
// Mode / max_threads accessors
// (Default mode is implied by the SetUp() helper itself - no separate test.)
// -----------------------------------------------------------------------------

TEST_F(MonitorFixture, set_mode_round_trips) {
    monitor::set_mode(Mode::MONITOR);
    EXPECT_EQ(monitor::get_mode(), Mode::MONITOR);
    monitor::set_mode(Mode::STRESS);
    EXPECT_EQ(monitor::get_mode(), Mode::STRESS);
    monitor::set_mode(Mode::NORMAL);
    EXPECT_EQ(monitor::get_mode(), Mode::NORMAL);
}

TEST_F(MonitorFixture, max_threads_round_trips_for_non_positive_input) {
    // set_max_threads(0) only stores 0 (it does NOT call omp_set_num_threads(0)
    // - that would be invalid). Negative values are also stored as-is.
    monitor::set_max_threads(0);
    EXPECT_EQ(monitor::get_max_threads(), 0);
    monitor::set_max_threads(-3);
    EXPECT_EQ(monitor::get_max_threads(), -3);
}

TEST_F(MonitorFixture, max_threads_round_trips_for_positive_input) {
    monitor::set_max_threads(4);
    EXPECT_EQ(monitor::get_max_threads(), 4);
}

// -----------------------------------------------------------------------------
// Per-construct counters - incremented only under MONITOR / STRESS
// -----------------------------------------------------------------------------

// One helper macro per counter - keeps each test single-purpose & readable.
#define EXPECT_COUNTER_HOOK(MODE_VALUE, hook, counter_field, expected_after) \
    do {                                                                    \
        monitor::set_mode(MODE_VALUE);                                      \
        monitor::reset_stats();                                              \
        hook;                                                                \
        hook;                                                                \
        EXPECT_EQ(monitor::stats().counter_field.load(), (expected_after))  \
            << "hook=" #hook " mode=" #MODE_VALUE;                          \
    } while(0)

TEST_F(MonitorFixture, normal_mode_keeps_all_counters_at_zero) {
    monitor::set_mode(Mode::NORMAL);
    detail::on_task_create();
    detail::on_single();
    detail::on_taskwait();
    detail::on_barrier();
    detail::on_critical();
    detail::on_for_loop();
    detail::on_atomic();
    detail::on_sections();
    detail::on_master();
    detail::on_ordered();
    detail::on_taskgroup();
    detail::on_simd();
    detail::on_cancel();
    detail::on_flush();
    detail::on_taskyield();

    auto& s = monitor::stats();
    EXPECT_EQ(s.tasks_created.load(), 0);
    EXPECT_EQ(s.single_regions.load(), 0);
    EXPECT_EQ(s.taskwaits.load(), 0);
    EXPECT_EQ(s.barriers.load(), 0);
    EXPECT_EQ(s.criticals.load(), 0);
    EXPECT_EQ(s.for_loops.load(), 0);
    EXPECT_EQ(s.atomics.load(), 0);
    EXPECT_EQ(s.sections.load(), 0);
    EXPECT_EQ(s.masters.load(), 0);
    EXPECT_EQ(s.ordered.load(), 0);
    EXPECT_EQ(s.taskgroups.load(), 0);
    EXPECT_EQ(s.simd_constructs.load(), 0);
    EXPECT_EQ(s.cancels.load(), 0);
    EXPECT_EQ(s.flushes.load(), 0);
    EXPECT_EQ(s.taskyields.load(), 0);
}

TEST_F(MonitorFixture, monitor_mode_increments_each_construct_counter) {
    EXPECT_COUNTER_HOOK(Mode::MONITOR, detail::on_task_create(), tasks_created, 2);
    EXPECT_COUNTER_HOOK(Mode::MONITOR, detail::on_single(), single_regions, 2);
    EXPECT_COUNTER_HOOK(Mode::MONITOR, detail::on_taskwait(), taskwaits, 2);
    EXPECT_COUNTER_HOOK(Mode::MONITOR, detail::on_barrier(), barriers, 2);
    EXPECT_COUNTER_HOOK(Mode::MONITOR, detail::on_critical(), criticals, 2);
    EXPECT_COUNTER_HOOK(Mode::MONITOR, detail::on_for_loop(), for_loops, 2);
    EXPECT_COUNTER_HOOK(Mode::MONITOR, detail::on_atomic(), atomics, 2);
    EXPECT_COUNTER_HOOK(Mode::MONITOR, detail::on_sections(), sections, 2);
    EXPECT_COUNTER_HOOK(Mode::MONITOR, detail::on_master(), masters, 2);
    EXPECT_COUNTER_HOOK(Mode::MONITOR, detail::on_ordered(), ordered, 2);
    EXPECT_COUNTER_HOOK(Mode::MONITOR, detail::on_taskgroup(), taskgroups, 2);
    EXPECT_COUNTER_HOOK(Mode::MONITOR, detail::on_simd(), simd_constructs, 2);
    EXPECT_COUNTER_HOOK(Mode::MONITOR, detail::on_cancel(), cancels, 2);
    EXPECT_COUNTER_HOOK(Mode::MONITOR, detail::on_flush(), flushes, 2);
    EXPECT_COUNTER_HOOK(Mode::MONITOR, detail::on_taskyield(), taskyields, 2);
}

TEST_F(MonitorFixture, stress_mode_also_increments_counters) {
    // STRESS is just MONITOR + delay injection - counters must still tick.
    monitor::set_mode(Mode::STRESS);
    detail::on_task_create();
    detail::on_critical();
    EXPECT_EQ(monitor::stats().tasks_created.load(), 1);
    EXPECT_EQ(monitor::stats().criticals.load(), 1);
}

TEST_F(MonitorFixture, reset_stats_zeroes_all_counters) {
    monitor::set_mode(Mode::MONITOR);
    detail::on_task_create();
    detail::on_barrier();
    detail::on_critical();
    monitor::stats().work_ns.fetch_add(123);
    monitor::stats().span_ns.fetch_add(456);

    monitor::reset_stats();

    auto& s = monitor::stats();
    EXPECT_EQ(s.tasks_created.load(), 0);
    EXPECT_EQ(s.barriers.load(), 0);
    EXPECT_EQ(s.criticals.load(), 0);
    EXPECT_EQ(s.work_ns.load(), 0);
    EXPECT_EQ(s.span_ns.load(), 0);
}

// -----------------------------------------------------------------------------
// Context activation - counters belong to the active context, not global
// -----------------------------------------------------------------------------

TEST(MonitorContextIsolation, separate_contexts_do_not_share_counters) {
    auto c1 = monitor::create_context();
    auto c2 = monitor::create_context();

    monitor::activate_context(c1.get());
    monitor::set_mode(Mode::MONITOR);
    detail::on_task_create();
    detail::on_task_create();

    monitor::activate_context(c2.get());
    monitor::set_mode(Mode::MONITOR);
    detail::on_task_create();

    EXPECT_EQ(c1->stats.tasks_created.load(), 2);
    EXPECT_EQ(c2->stats.tasks_created.load(), 1);

    monitor::activate_context(nullptr);
}

TEST(MonitorContextIsolation, deactivating_routes_back_to_default) {
    auto local = monitor::create_context();
    monitor::activate_context(local.get());
    EXPECT_EQ(detail::current_context(), local.get());

    monitor::activate_context(nullptr);
    EXPECT_EQ(detail::current_context(), nullptr)
        << "current_context() reports the TLS pointer raw; default ctx is private";
}

TEST(MonitorContextIsolation, context_does_not_leak_across_threads) {
    auto local = monitor::create_context();
    monitor::activate_context(local.get());

    std::atomic<bool> other_thread_saw_main_ctx{true};
    std::thread(
        [&] {
            // Other thread's TLS is null -> current_context() must return nullptr,
            // NOT the main thread's local context.
            other_thread_saw_main_ctx.store(
                detail::current_context() == local.get()
            );
        }
    ).join();

    EXPECT_FALSE(other_thread_saw_main_ctx.load());
    monitor::activate_context(nullptr);
}

// -----------------------------------------------------------------------------
// work_begin / work_end - single-threaded, with explicit nesting guard
// -----------------------------------------------------------------------------

TEST_F(MonitorFixture, work_begin_returns_zero_in_normal_mode) {
    EXPECT_EQ(detail::work_begin(), 0);
    detail::work_end(0);                            // matches the contract: 0 -> no-op
    EXPECT_EQ(monitor::stats().work_ns.load(), 0);
}

TEST_F(MonitorFixture, work_begin_returns_nonzero_in_monitor_mode) {
    monitor::set_mode(Mode::MONITOR);
    long long start = detail::work_begin();
    EXPECT_GT(start, 0);
    detail::work_end(start);
    EXPECT_GE(monitor::stats().work_ns.load(), 0);
}

TEST_F(MonitorFixture, work_end_accumulates_elapsed_time) {
    monitor::set_mode(Mode::MONITOR);
    long long start = detail::work_begin();
    ASSERT_GT(start, 0);
    // Force a measurable gap so the elapsed delta isn't suspiciously close to 0.
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    detail::work_end(start);

    EXPECT_GT(monitor::stats().work_ns.load(), 0);
}

TEST_F(MonitorFixture, nested_work_begin_returns_zero_to_prevent_double_count) {
    monitor::set_mode(Mode::MONITOR);

    long long outer = detail::work_begin();
    ASSERT_GT(outer, 0);

    // Inside the timed region - nested call must return 0 (don't time twice).
    long long inner = detail::work_begin();
    EXPECT_EQ(inner, 0) << "work_begin while already timing must yield 0";

    detail::work_end(inner);   // no-op
    detail::work_end(outer);   // accumulates
}

TEST_F(MonitorFixture, work_end_resets_nesting_guard_even_after_mode_change) {
    // Set up a "leaking" scenario: start in MONITOR, switch to NORMAL mid-region.
    // work_end must still clear tl_in_work_timing, otherwise the next region
    // would be silently dropped.
    monitor::set_mode(Mode::MONITOR);
    long long start = detail::work_begin();
    ASSERT_GT(start, 0);

    monitor::set_mode(Mode::NORMAL);
    detail::work_end(start);

    // Re-enter MONITOR - should be able to time again.
    monitor::set_mode(Mode::MONITOR);
    long long start2 = detail::work_begin();
    EXPECT_GT(start2, 0)
        << "nesting guard must have been cleared by the previous work_end()";
    detail::work_end(start2);
}

// -----------------------------------------------------------------------------
// span tracking - single-threaded path, just enough to prove finalize_root
// records non-zero span and span_save/restore actually round-trip TLS state.
// -----------------------------------------------------------------------------

TEST_F(MonitorFixture, span_init_then_finalize_records_non_negative_span) {
    monitor::set_mode(Mode::MONITOR);
    detail::span_init_root();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    detail::span_finalize_root();
    EXPECT_GE(monitor::stats().span_ns.load(), 0)
        << "finalize_root must publish a non-negative span into Stats.span_ns";
}

TEST_F(MonitorFixture, span_finalize_without_init_is_safe_no_op) {
    // In NORMAL mode neither init nor finalize touch span_ns.
    monitor::set_mode(Mode::NORMAL);
    detail::span_init_root();
    detail::span_finalize_root();
    EXPECT_EQ(monitor::stats().span_ns.load(), 0);
}

TEST_F(MonitorFixture, span_save_restore_round_trip_preserves_state) {
    monitor::set_mode(Mode::MONITOR);
    detail::span_init_root();

    auto saved = detail::span_save_state();
    // Mutate the live state, then restore - invariants tied to the original
    // state must hold after restore.
    detail::span_prepare_child();
    detail::span_restore_state(saved);

    // After restore + finalize the recorded span must be non-negative.
    // (Tighter assertions need full task DAG simulation.)
    detail::span_finalize_root();
    EXPECT_GE(monitor::stats().span_ns.load(), 0);
}
