// Unit tests for runner_lib base runtime - argv parsing, consuming read_*
// wrappers, timing, and the finish() persistence path. We do NOT exercise
// framework variants (runner_omp / parlay / cilk / seq) - those are compiled
// per-job and tested at integration time.

#include <cstring>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <runner.h>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "test_temp_dir.h"


namespace {

    /// Build a fake argv from a vector of arg strings. Owns the storage.
    class Argv {
        public:
            explicit Argv(std::initializer_list<std::string> args) {
                storage_.reserve(args.size() + 1);
                storage_.emplace_back("runner.exe");   // argv[0]
                for(auto& a : args) storage_.emplace_back(a);
                pointers_.reserve(storage_.size());
                for(auto& s : storage_) pointers_.push_back(s.data());
            }

            int argc() { return static_cast<int>(pointers_.size()); }
            char** argv() { return pointers_.data(); }

        private:
            std::vector<std::string> storage_;
            std::vector<char*> pointers_;
    };

    /// Build a TestData payload on disk for a given key/value mix and return the
    /// containing directory's path. Used to seed runner::load_input() in tests.
    std::filesystem::path seed_input(const TempDir& dir, const TestData& data) {
        auto input_dir = dir.path() / "input";
        std::filesystem::create_directories(input_dir);
        data.save(input_dir / "input.bin");
        return input_dir;
    }

} // namespace

// -----------------------------------------------------------------------------
// init() - flag parsing
// -----------------------------------------------------------------------------

TEST(RunnerInit, defaults_when_no_flags_supplied) {
    Argv args{};
    const auto& cfg = runner::init(args.argc(), args.argv());
    EXPECT_TRUE(cfg.input_dir.empty());
    EXPECT_TRUE(cfg.output_dir.empty());
    EXPECT_EQ(cfg.thread_count, 1);
    EXPECT_EQ(cfg.monitor_mode, "normal");
}

TEST(RunnerInit, parses_all_known_flags) {
    Argv args{
        "--input",
        "in/dir",
        "--output",
        "out/dir",
        "--threads",
        "8",
        "--monitor-mode",
        "stress"
    };
    const auto& cfg = runner::init(args.argc(), args.argv());
    EXPECT_EQ(cfg.input_dir, "in/dir");
    EXPECT_EQ(cfg.output_dir, "out/dir");
    EXPECT_EQ(cfg.thread_count, 8);
    EXPECT_EQ(cfg.monitor_mode, "stress");
}

TEST(RunnerInit, invalid_threads_value_falls_back_to_one) {
    Argv args{"--threads", "not-a-number"};
    const auto& cfg = runner::init(args.argc(), args.argv());
    EXPECT_EQ(cfg.thread_count, 1) << "malformed --threads should default to 1, not crash";
}

TEST(RunnerInit, dangling_flag_without_value_is_ignored) {
    // "--input" appears as the last argv element - i+1 >= argc -> loop bails.
    // Must not read past the end.
    Argv args{"--threads", "4", "--input"};
    const auto& cfg = runner::init(args.argc(), args.argv());
    EXPECT_EQ(cfg.thread_count, 4);
    EXPECT_TRUE(cfg.input_dir.empty());
}

TEST(RunnerInit, unknown_flag_is_silently_skipped) {
    Argv args{"--made-up", "value", "--threads", "2"};
    const auto& cfg = runner::init(args.argc(), args.argv());
    EXPECT_EQ(cfg.thread_count, 2);
}

TEST(RunnerInit, output_dir_is_created_if_missing) {
    TempDir tmp;
    auto out = tmp.path() / "fresh-out";
    EXPECT_FALSE(std::filesystem::exists(out));

    Argv args{"--output", out.string()};
    runner::init(args.argc(), args.argv());

    EXPECT_TRUE(std::filesystem::is_directory(out));
}

TEST(RunnerInit, init_resets_state_between_calls) {
    // A previous test may have left input/output maps populated. init() must
    // wipe them so the next "job" starts clean.
    Argv first{"--threads", "4"};
    runner::init(first.argc(), first.argv());
    runner::output().write_value<long long>("leftover", 42);

    Argv second{};
    runner::init(second.argc(), second.argv());
    EXPECT_FALSE(runner::output().contains("leftover"))
        << "init() should clear the output map";
}

// -----------------------------------------------------------------------------
// load_input() - populates runner::input() from <input_dir>/input.bin
// -----------------------------------------------------------------------------

TEST(RunnerInput, load_input_is_noop_when_no_input_dir) {
    Argv args{};   // no --input
    runner::init(args.argc(), args.argv());
    // Pre-populate to make sure load_input() doesn't smash existing state.
    runner::input().write_value<long long>("kept", 5);
    EXPECT_NO_THROW(runner::load_input());
    EXPECT_TRUE(runner::input().contains("kept"));
}

TEST(RunnerInput, load_input_reads_bin_file_into_map) {
    TempDir tmp;
    TestData payload;
    payload.write_value<long long>("n", 99);
    payload.write_array<long long>("arr", {1, 2, 3});
    auto in_dir = seed_input(tmp, payload);

    Argv args{"--input", in_dir.string()};
    runner::init(args.argc(), args.argv());
    runner::load_input();

    EXPECT_EQ(runner::input().read_value<long long>("n"), 99);
    EXPECT_EQ(
        runner::input().read_array<long long>("arr"),
        (std::vector<long long>{1, 2, 3})
    );
}

// -----------------------------------------------------------------------------
// Consuming read_* wrappers - read then erase (frees raw bytes for big arrays)
// -----------------------------------------------------------------------------

TEST(RunnerConsumingReads, read_value_erases_key_after_parse) {
    Argv args{};
    runner::init(args.argc(), args.argv());
    runner::input().write_value<long long>("n", 7);

    EXPECT_EQ(runner::read_value<long long>("n"), 7);
    EXPECT_FALSE(runner::input().contains("n")) << "wrapper should erase after read";
}

TEST(RunnerConsumingReads, second_read_throws_after_consumption) {
    Argv args{};
    runner::init(args.argc(), args.argv());
    runner::input().write_value<long long>("n", 1);
    runner::read_value<long long>("n");
    EXPECT_THROW(runner::read_value<long long>("n"), std::runtime_error);
}

TEST(RunnerConsumingReads, read_array_erases_key) {
    Argv args{};
    runner::init(args.argc(), args.argv());
    runner::input().write_array<long long>("v", {1, 2, 3});
    EXPECT_EQ(
        runner::read_array<long long>("v"),
        (std::vector<long long>{1, 2, 3})
    );
    EXPECT_FALSE(runner::input().contains("v"));
}

TEST(RunnerConsumingReads, read_string_erases_key) {
    Argv args{};
    runner::init(args.argc(), args.argv());
    runner::input().write_string("s", "hello");
    EXPECT_EQ(runner::read_string("s"), "hello");
    EXPECT_FALSE(runner::input().contains("s"));
}

TEST(RunnerConsumingReads, read_strings_erases_key) {
    Argv args{};
    runner::init(args.argc(), args.argv());
    runner::input().write_strings("tags", {"a", "b"});
    EXPECT_EQ(runner::read_strings("tags"), (std::vector<std::string>{"a", "b"}));
    EXPECT_FALSE(runner::input().contains("tags"));
}

// -----------------------------------------------------------------------------
// Timing helpers - begin_execute / end_execute / execute_time_ms
// -----------------------------------------------------------------------------

TEST(RunnerTiming, execute_time_is_non_negative_after_begin_end) {
    Argv args{};
    runner::init(args.argc(), args.argv());
    runner::begin_execute();
    runner::end_execute();
    EXPECT_GE(runner::execute_time_ms(), 0.0);
}

TEST(RunnerTiming, execute_time_grows_with_a_measurable_gap) {
    Argv args{};
    runner::init(args.argc(), args.argv());
    runner::begin_execute();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    runner::end_execute();
    // Allow 2ms slack for scheduling jitter on Windows.
    EXPECT_GE(runner::execute_time_ms(), 3.0);
}

// -----------------------------------------------------------------------------
// finish() - output.bin + meta.bin + optional hook
// -----------------------------------------------------------------------------

TEST(RunnerFinish, no_op_when_output_dir_is_empty) {
    Argv args{};   // no --output
    runner::init(args.argc(), args.argv());
    runner::output().write_value<long long>("ignored", 1);
    EXPECT_NO_THROW(runner::finish());
    // Nothing to assert on disk - just confirm no exception escapes.
}

TEST(RunnerFinish, writes_output_bin_and_meta_bin_to_disk) {
    TempDir tmp;
    auto out_dir = tmp.path() / "out";

    Argv args{"--output", out_dir.string()};
    runner::init(args.argc(), args.argv());

    runner::output().write_value<long long>("answer", 42);
    runner::begin_execute();
    runner::end_execute();
    runner::finish();

    auto output_bin = out_dir / "output.bin";
    auto meta_bin = out_dir / "meta.bin";
    ASSERT_TRUE(std::filesystem::exists(output_bin));
    ASSERT_TRUE(std::filesystem::exists(meta_bin));

    // Round-trip output.bin through TestData::load to verify it's parseable.
    auto loaded = TestData::load(output_bin);
    EXPECT_EQ(loaded.read_value<long long>("answer"), 42);

    // meta.bin is a raw 160-byte struct dump.
    EXPECT_EQ(std::filesystem::file_size(meta_bin), sizeof(runner::Meta));
}

TEST(RunnerFinish, finish_hook_is_invoked_and_can_populate_meta) {
    TempDir tmp;
    auto out_dir = tmp.path() / "out";

    Argv args{"--output", out_dir.string()};
    runner::init(args.argc(), args.argv());

    bool hook_called = false;
    runner::set_finish_hook(
        [&](runner::Meta& meta) {
            hook_called = true;
            meta.parallel_regions = 3;
            meta.work_ns = 12345;
        }
    );

    runner::begin_execute();
    runner::end_execute();
    runner::finish();

    EXPECT_TRUE(hook_called);

    // Confirm meta.bin reflects the values the hook stamped in.
    std::ifstream f(out_dir / "meta.bin", std::ios::binary);
    ASSERT_TRUE(f.is_open());
    runner::Meta meta;
    f.read(reinterpret_cast<char*>(&meta), sizeof(meta));
    EXPECT_EQ(meta.parallel_regions, 3);
    EXPECT_EQ(meta.work_ns, 12345);
    EXPECT_GE(meta.time_ms, 0.0);
}

TEST(RunnerFinish, init_clears_previous_finish_hook) {
    // First job sets a hook that would crash if invoked.
    Argv first{};
    runner::init(first.argc(), first.argv());
    runner::set_finish_hook([&](runner::Meta&) { FAIL() << "stale hook ran"; });

    // Re-init for the next job - hook must be cleared.
    TempDir tmp;
    auto out_dir = tmp.path() / "out";
    Argv second{"--output", out_dir.string()};
    runner::init(second.argc(), second.argv());

    runner::begin_execute();
    runner::end_execute();
    EXPECT_NO_THROW(runner::finish());
}
