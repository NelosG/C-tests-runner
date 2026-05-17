// Unit tests for config_utils - read_json_file, get_env, ServerConfig::load.
//
// ServerConfig::load is the interesting one: each field is parsed only if the
// JSON value has the right type; mistyped values fall back to defaults. That
// type-validation logic is what we want to nail down here.

#include <config_utils.h>
#include <cstdlib>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <string>

#include "test_temp_dir.h"

using config::ServerConfig;
using config::read_json_file;
using config::get_env;

// -----------------------------------------------------------------------------
// read_json_file
// -----------------------------------------------------------------------------

TEST(ReadJsonFile, missing_file_returns_empty_json) {
    auto j = read_json_file("no-such-file.json");
    EXPECT_TRUE(j.empty());
}

TEST(ReadJsonFile, malformed_json_returns_empty_without_throwing) {
    TempDir dir;
    auto path = dir.write_file("bad.json", "{not valid");
    nlohmann::json j;
    EXPECT_NO_THROW(j = read_json_file(path));
    EXPECT_TRUE(j.empty());
}

TEST(ReadJsonFile, valid_file_round_trips) {
    TempDir dir;
    auto path = dir.write_file("ok.json", R"({"a": 1, "b": "hi"})");
    auto j = read_json_file(path);
    EXPECT_EQ(j.value("a", 0), 1);
    EXPECT_EQ(j.value("b", std::string{}), "hi");
}

// -----------------------------------------------------------------------------
// get_env
// -----------------------------------------------------------------------------

TEST(GetEnv, returns_default_when_var_is_unset) {
    // Use a name that's extremely unlikely to be set in any CI environment.
    EXPECT_EQ(get_env("CTR_DEFINITELY_UNSET_VAR_12345", "fallback"), "fallback");
}

TEST(GetEnv, returns_value_when_var_is_set) {
    #ifdef _WIN32
    _putenv("CTR_TEST_GETENV=fromenv");
    #else
    setenv("CTR_TEST_GETENV", "fromenv", 1);
    #endif
    EXPECT_EQ(get_env("CTR_TEST_GETENV", "fallback"), "fromenv");
    #ifdef _WIN32
    _putenv("CTR_TEST_GETENV=");
    #else
    unsetenv("CTR_TEST_GETENV");
    #endif
}

TEST(GetEnv, empty_env_value_falls_through_to_default) {
    // `(val && val[0])` - empty string env var is treated as unset.
    #ifdef _WIN32
    _putenv("CTR_TEST_GETENV_EMPTY=");
    #else
    setenv("CTR_TEST_GETENV_EMPTY", "", 1);
    #endif
    EXPECT_EQ(get_env("CTR_TEST_GETENV_EMPTY", "fallback"), "fallback");
    #ifdef _WIN32
    _putenv("CTR_TEST_GETENV_EMPTY=");
    #else
    unsetenv("CTR_TEST_GETENV_EMPTY");
    #endif
}

// -----------------------------------------------------------------------------
// ServerConfig::load - defaults + selective parsing
// -----------------------------------------------------------------------------

TEST(ServerConfigLoad, defaults_when_file_missing) {
    auto cfg = ServerConfig::load("definitely/missing.json");
    EXPECT_TRUE(cfg.defaultAdapters.empty());
    EXPECT_TRUE(cfg.defaultResourceProviders.empty());
    EXPECT_FALSE(cfg.correctnessWorkers.has_value());
    EXPECT_FALSE(cfg.nodeId.has_value());
    EXPECT_EQ(cfg.defaultMemoryLimitMb, 1024);
    EXPECT_EQ(cfg.defaultThreads, 4);
    EXPECT_EQ(cfg.defaultWallTimeSec, 60);
    EXPECT_EQ(cfg.defaultCpuTimeSec, 30);
    EXPECT_EQ(cfg.sandboxProcessMultiplier, 2);
}

TEST(ServerConfigLoad, parses_complete_well_typed_file) {
    TempDir dir;
    auto path = dir.write_file(
        "server.json",
        R"({
        "defaultAdapters":          ["http", "rabbit"],
        "defaultResourceProviders": ["git", "local"],
        "correctnessWorkers": 8,
        "nodeId": "runner-7",
        "defaultMemoryLimitMb": 2048,
        "defaultThreads": 16,
        "defaultWallTimeSec": 120,
        "defaultCpuTimeSec": 60,
        "sandbox": { "processMultiplier": 3 }
    })"
    );
    auto cfg = ServerConfig::load(path);
    EXPECT_EQ(cfg.defaultAdapters, (std::vector<std::string>{"http", "rabbit"}));
    EXPECT_EQ(cfg.defaultResourceProviders, (std::vector<std::string>{"git", "local"}));
    ASSERT_TRUE(cfg.correctnessWorkers.has_value());
    EXPECT_EQ(*cfg.correctnessWorkers, 8);
    ASSERT_TRUE(cfg.nodeId.has_value());
    EXPECT_EQ(*cfg.nodeId, "runner-7");
    EXPECT_EQ(cfg.defaultMemoryLimitMb, 2048);
    EXPECT_EQ(cfg.defaultThreads, 16);
    EXPECT_EQ(cfg.defaultWallTimeSec, 120);
    EXPECT_EQ(cfg.defaultCpuTimeSec, 60);
    EXPECT_EQ(cfg.sandboxProcessMultiplier, 3);
}

TEST(ServerConfigLoad, non_array_default_adapters_is_ignored) {
    TempDir dir;
    auto path = dir.write_file("server.json", R"({"defaultAdapters": "http"})");
    auto cfg = ServerConfig::load(path);
    EXPECT_TRUE(cfg.defaultAdapters.empty())
        << "scalar where an array is expected must NOT propagate";
}

TEST(ServerConfigLoad, non_string_adapter_entries_are_filtered) {
    TempDir dir;
    auto path = dir.write_file(
        "server.json",
        R"({
        "defaultAdapters": ["http", 42, true, "rabbit"]
    })"
    );
    auto cfg = ServerConfig::load(path);
    EXPECT_EQ(cfg.defaultAdapters, (std::vector<std::string>{"http", "rabbit"}));
}

TEST(ServerConfigLoad, mistyped_int_field_falls_back_to_default) {
    TempDir dir;
    auto path = dir.write_file(
        "server.json",
        R"({
        "defaultThreads": "many"
    })"
    );
    auto cfg = ServerConfig::load(path);
    EXPECT_EQ(cfg.defaultThreads, 4) << "string where int expected must use default";
}

TEST(ServerConfigLoad, sandbox_subobject_can_be_missing) {
    TempDir dir;
    auto path = dir.write_file("server.json", R"({"defaultThreads": 8})");
    auto cfg = ServerConfig::load(path);
    EXPECT_EQ(cfg.defaultThreads, 8);
    EXPECT_EQ(cfg.sandboxProcessMultiplier, 2)  // default
        << "absence of `sandbox` block must leave multiplier at default";
}

TEST(ServerConfigLoad, malformed_file_yields_defaults) {
    TempDir dir;
    auto path = dir.write_file("server.json", "{this isn't valid");
    auto cfg = ServerConfig::load(path);
    EXPECT_EQ(cfg.defaultThreads, 4);
    EXPECT_EQ(cfg.defaultMemoryLimitMb, 1024);
    EXPECT_TRUE(cfg.defaultAdapters.empty());
}
