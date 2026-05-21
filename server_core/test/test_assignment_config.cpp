// Unit tests for assignment_config::load - parses <test_dir>/config.json into
// an AssignmentConfig struct with sensible defaults.

#include <assignment_config.h>
#include <gtest/gtest.h>

#include "test_temp_dir.h"


namespace {

    /// Helper - locate "allowedPackages" default for a framework set.
    bool contains(const std::vector<std::string>& v, const std::string& s) {
        for(const auto& x : v) if(x == s) return true;
        return false;
    }

} // namespace

// -----------------------------------------------------------------------------
// Missing / unreadable file - produces an "empty" config the caller can reject
// -----------------------------------------------------------------------------

TEST(AssignmentConfig, missing_config_returns_empty_defaults) {
    TempDir dir;  // no config.json written
    auto cfg = assignment_config::load(dir.path());

    EXPECT_TRUE(cfg.name.empty());
    EXPECT_EQ(cfg.mode, "correctness");
    EXPECT_EQ(cfg.correctness_mode, "stress");
    EXPECT_TRUE(cfg.allowed_frameworks.empty());
    EXPECT_TRUE(cfg.allowed_packages.empty());
}

TEST(AssignmentConfig, nonexistent_dir_returns_empty_defaults) {
    auto cfg = assignment_config::load("path/that/does/not/exist");
    EXPECT_TRUE(cfg.allowed_frameworks.empty());
    EXPECT_EQ(cfg.mode, "correctness");
}

// -----------------------------------------------------------------------------
// Happy path
// -----------------------------------------------------------------------------

TEST(AssignmentConfig, parses_explicit_fields) {
    TempDir dir;
    dir.write_file(
        "config.json",
        R"({
        "name": "Quicksort",
        "mode": "performance",
        "correctnessMode": "monitor",
        "allowedFrameworks": ["openmp", "cilk"],
        "allowedPackages":  ["OpenMP", "Cilk", "MyHelper"]
    })"
    );

    auto cfg = assignment_config::load(dir.path());
    EXPECT_EQ(cfg.name, "Quicksort");
    EXPECT_EQ(cfg.mode, "performance");
    EXPECT_EQ(cfg.correctness_mode, "monitor");
    EXPECT_EQ(cfg.allowed_frameworks, (std::vector<std::string>{"openmp", "cilk"}));
    EXPECT_EQ(cfg.allowed_packages, (std::vector<std::string>{"OpenMP", "Cilk", "MyHelper"}));
}

// -----------------------------------------------------------------------------
// Default packages - derived from allowed_frameworks when absent
// -----------------------------------------------------------------------------

TEST(AssignmentConfig, defaults_packages_from_openmp_framework) {
    TempDir dir;
    dir.write_file(
        "config.json",
        R"({
        "allowedFrameworks": ["openmp"]
    })"
    );

    auto cfg = assignment_config::load(dir.path());
    EXPECT_TRUE(contains(cfg.allowed_packages, "OpenMP"));
    EXPECT_TRUE(contains(cfg.allowed_packages, "parallel_lib"));
    EXPECT_EQ(cfg.allowed_packages.size(), 2u);
}

TEST(AssignmentConfig, defaults_packages_from_parlay_framework) {
    TempDir dir;
    dir.write_file(
        "config.json",
        R"({
        "allowedFrameworks": ["parlay"]
    })"
    );

    auto cfg = assignment_config::load(dir.path());
    EXPECT_TRUE(contains(cfg.allowed_packages, "parlay"));
    EXPECT_TRUE(contains(cfg.allowed_packages, "parlaylib"));
    EXPECT_EQ(cfg.allowed_packages.size(), 2u);
}

TEST(AssignmentConfig, defaults_packages_from_cilk_framework) {
    TempDir dir;
    dir.write_file(
        "config.json",
        R"({
        "allowedFrameworks": ["cilk"]
    })"
    );

    auto cfg = assignment_config::load(dir.path());
    EXPECT_EQ(cfg.allowed_packages, std::vector<std::string>{"Cilk"});
}

TEST(AssignmentConfig, defaults_packages_union_across_frameworks) {
    TempDir dir;
    dir.write_file(
        "config.json",
        R"({
        "allowedFrameworks": ["openmp", "parlay", "cilk"]
    })"
    );

    auto cfg = assignment_config::load(dir.path());
    EXPECT_TRUE(contains(cfg.allowed_packages, "OpenMP"));
    EXPECT_TRUE(contains(cfg.allowed_packages, "parallel_lib"));
    EXPECT_TRUE(contains(cfg.allowed_packages, "parlay"));
    EXPECT_TRUE(contains(cfg.allowed_packages, "parlaylib"));
    EXPECT_TRUE(contains(cfg.allowed_packages, "Cilk"));
}

TEST(AssignmentConfig, defaults_packages_unique_when_frameworks_overlap) {
    TempDir dir;
    dir.write_file(
        "config.json",
        R"({
        "allowedFrameworks": ["openmp", "openmp", "openmp"]
    })"
    );

    auto cfg = assignment_config::load(dir.path());
    // append_unique() in the source guarantees no duplicates.
    EXPECT_EQ(cfg.allowed_packages.size(), 2u);
}

TEST(AssignmentConfig, defaults_packages_empty_when_frameworks_empty) {
    TempDir dir;
    dir.write_file(
        "config.json",
        R"({
        "allowedFrameworks": []
    })"
    );

    auto cfg = assignment_config::load(dir.path());
    EXPECT_TRUE(cfg.allowed_packages.empty())
        << "Empty allowedFrameworks must yield an empty default package list "
           "(student is forced into sequential code with no extras).";
}

// -----------------------------------------------------------------------------
// Mode / correctness_mode defaults
// -----------------------------------------------------------------------------

TEST(AssignmentConfig, mode_default_correctness_when_unspecified) {
    TempDir dir;
    dir.write_file("config.json", R"({"name": "X"})");

    auto cfg = assignment_config::load(dir.path());
    EXPECT_EQ(cfg.mode, "correctness");
    EXPECT_EQ(cfg.correctness_mode, "stress");
}

// -----------------------------------------------------------------------------
// Malformed / unreadable file - graceful fallback, no throw
// -----------------------------------------------------------------------------

TEST(AssignmentConfig, malformed_json_returns_defaults_without_throwing) {
    TempDir dir;
    dir.write_file("config.json", "{not valid json");

    AssignmentConfig cfg;
    EXPECT_NO_THROW(cfg = assignment_config::load(dir.path()));
    EXPECT_TRUE(cfg.allowed_frameworks.empty());
    EXPECT_EQ(cfg.mode, "correctness");
}

// -----------------------------------------------------------------------------
// Optional resource caps
// -----------------------------------------------------------------------------

TEST(AssignmentConfig, resource_caps_absent_yield_nullopts) {
    TempDir dir;
    dir.write_file("config.json", R"({"name": "X"})");

    auto cfg = assignment_config::load(dir.path());
    EXPECT_FALSE(cfg.threads.has_value());
    EXPECT_FALSE(cfg.memory_limit_mb.has_value());
    EXPECT_FALSE(cfg.wall_time_sec.has_value());
    EXPECT_FALSE(cfg.cpu_time_sec.has_value());
    EXPECT_FALSE(cfg.max_processes.has_value());
}

TEST(AssignmentConfig, resource_caps_parse_when_present) {
    TempDir dir;
    dir.write_file(
        "config.json",
        R"({
        "threads": 8,
        "memoryLimitMb": 4096,
        "wallTimeSec": 120,
        "cpuTimeSec": 90,
        "maxProcesses": 32
    })"
    );

    auto cfg = assignment_config::load(dir.path());
    ASSERT_TRUE(cfg.threads.has_value());          EXPECT_EQ(*cfg.threads, 8);
    ASSERT_TRUE(cfg.memory_limit_mb.has_value());  EXPECT_EQ(*cfg.memory_limit_mb, 4096);
    ASSERT_TRUE(cfg.wall_time_sec.has_value());    EXPECT_EQ(*cfg.wall_time_sec, 120);
    ASSERT_TRUE(cfg.cpu_time_sec.has_value());     EXPECT_EQ(*cfg.cpu_time_sec, 90);
    ASSERT_TRUE(cfg.max_processes.has_value());    EXPECT_EQ(*cfg.max_processes, 32);
}

TEST(AssignmentConfig, resource_caps_with_wrong_type_are_silently_ignored) {
    // A typo'd resource field should not crash assignment parsing - the other
    // fields (name/allowedFrameworks/etc.) must still survive.
    TempDir dir;
    dir.write_file(
        "config.json",
        R"({
        "name": "X",
        "allowedFrameworks": ["openmp"],
        "threads": "eight",
        "memoryLimitMb": 4.5,
        "wallTimeSec": true,
        "cpuTimeSec": null,
        "maxProcesses": [16]
    })"
    );

    auto cfg = assignment_config::load(dir.path());
    EXPECT_EQ(cfg.name, "X");
    EXPECT_EQ(cfg.allowed_frameworks, std::vector<std::string>{"openmp"});
    EXPECT_FALSE(cfg.threads.has_value());
    EXPECT_FALSE(cfg.memory_limit_mb.has_value());
    EXPECT_FALSE(cfg.wall_time_sec.has_value());
    EXPECT_FALSE(cfg.cpu_time_sec.has_value());
    EXPECT_FALSE(cfg.max_processes.has_value());
}

TEST(AssignmentConfig, allowed_frameworks_wrong_type_does_not_throw) {
    TempDir dir;
    // allowedFrameworks should be an array; passing a string must be tolerated.
    dir.write_file(
        "config.json",
        R"({
        "allowedFrameworks": "openmp"
    })"
    );

    AssignmentConfig cfg;
    EXPECT_NO_THROW(cfg = assignment_config::load(dir.path()));
    EXPECT_TRUE(cfg.allowed_frameworks.empty());
}
