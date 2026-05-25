// Unit tests for ResourceManager.
//
// ResourceManager is the runtime DLL container for resource providers (git,
// local). It scans a directory for resource_*.so/.dll files, maintains a
// name->path index, and loads/unloads providers on demand. The lifecycle
// inside load_locked() drives the full provider contract: create -> validate
// -> start -> resolve -> stop -> destroy.
//
// Tests use a fake resource_fake.so fixture installed alongside the test
// executable (see CMakeLists `fake_resource_provider` target). The fake's
// behaviour is driven by config flags so we can exercise the rejection /
// start-failure paths without inventing additional fixtures.

#include <gtest/gtest.h>

#include <filesystem>

#include <resource_manager.h>

#include "test_temp_dir.h"

namespace fs = std::filesystem;

#ifndef CTR_FAKE_PROVIDER_DIR
#  error "CTR_FAKE_PROVIDER_DIR must be defined by CMake"
#endif

namespace {
    fs::path providers_dir() { return fs::path(CTR_FAKE_PROVIDER_DIR); }
}

// -----------------------------------------------------------------------------
// Construction / scan
// -----------------------------------------------------------------------------

TEST(ResourceManager, construct_with_missing_dir_yields_empty_list) {
    ResourceManager rm("/nonexistent/path/for/sure_xyz");
    auto list = rm.list();
    EXPECT_TRUE(list.is_array());
    EXPECT_EQ(list.size(), 0u);
}

TEST(ResourceManager, scan_finds_fake_provider_fixture) {
    ResourceManager rm(providers_dir());
    auto list = rm.list();
    ASSERT_GE(list.size(), 1u);
    bool seen = false;
    for(const auto& entry : list) {
        if(entry["name"].get<std::string>() == "fake") seen = true;
    }
    EXPECT_TRUE(seen) << "fake provider must appear in scan list";
}

TEST(ResourceManager, fresh_manager_reports_available_status) {
    ResourceManager rm(providers_dir());
    auto list = rm.list();
    for(const auto& entry : list) {
        if(entry["name"] == "fake") {
            EXPECT_EQ(entry["status"].get<std::string>(), "available");
            EXPECT_FALSE(entry.contains("config")) << "config only present when running";
        }
    }
}

// -----------------------------------------------------------------------------
// resolve() failure - no provider loaded
// -----------------------------------------------------------------------------

TEST(ResourceManager, resolve_throws_when_provider_not_loaded) {
    ResourceManager rm(providers_dir());
    EXPECT_THROW(rm.resolve("nope", {{"id", "x"}}), std::runtime_error);
}

TEST(ResourceManager, resolve_error_message_lists_loaded_providers) {
    ResourceManager rm(providers_dir());
    try {
        rm.resolve("git", {});
        FAIL() << "expected throw";
    } catch(const std::runtime_error& e) {
        // Message includes "(none)" when no providers are loaded.
        EXPECT_NE(std::string(e.what()).find("(none)"), std::string::npos);
    }
}

// -----------------------------------------------------------------------------
// load() failure paths
// -----------------------------------------------------------------------------

TEST(ResourceManager, load_returns_false_for_unknown_provider) {
    ResourceManager rm(providers_dir());
    std::string err;
    EXPECT_FALSE(rm.load("not-a-provider", {}, &err));
    EXPECT_NE(err.find("not-a-provider"), std::string::npos);
}

TEST(ResourceManager, load_rejected_by_validate_config_returns_false) {
    ResourceManager rm(providers_dir());
    std::string err;
    EXPECT_FALSE(rm.load("fake", {{"reject", true}}, &err));
    EXPECT_NE(err.find("rejected-by-fake"), std::string::npos);
}

TEST(ResourceManager, load_with_create_returning_null_returns_false) {
    ResourceManager rm(providers_dir());
    std::string err;
    EXPECT_FALSE(rm.load("fake", {{"ctor_null", true}}, &err));
    EXPECT_NE(err.find("nullptr"), std::string::npos);
}

TEST(ResourceManager, load_with_start_throwing_returns_false) {
    ResourceManager rm(providers_dir());
    std::string err;
    EXPECT_FALSE(rm.load("fake", {{"throw_start", true}}, &err));
    EXPECT_NE(err.find("fake-provider-start-explode"), std::string::npos);
}

// -----------------------------------------------------------------------------
// Successful load -> resolve -> unload lifecycle
// -----------------------------------------------------------------------------

TEST(ResourceManager, load_then_resolve_round_trip) {
    ResourceManager rm(providers_dir());
    ASSERT_TRUE(rm.load("fake", {}));
    auto path = rm.resolve("fake", {{"id", "alpha"}});
    EXPECT_NE(path.string().find("alpha"), std::string::npos);
}

TEST(ResourceManager, double_load_returns_false_with_already_loaded_error) {
    ResourceManager rm(providers_dir());
    ASSERT_TRUE(rm.load("fake", {}));
    std::string err;
    EXPECT_FALSE(rm.load("fake", {}, &err));
    EXPECT_NE(err.find("already loaded"), std::string::npos);
}

TEST(ResourceManager, list_reports_running_status_after_load) {
    ResourceManager rm(providers_dir());
    ASSERT_TRUE(rm.load("fake", {{"flavour", "x"}}));
    auto list = rm.list();
    for(const auto& entry : list) {
        if(entry["name"] == "fake") {
            EXPECT_EQ(entry["status"].get<std::string>(), "running");
            ASSERT_TRUE(entry.contains("config"));
            EXPECT_EQ(entry["config"].value("flavour", ""), "x");
        }
    }
}

TEST(ResourceManager, unload_running_provider_returns_true) {
    ResourceManager rm(providers_dir());
    ASSERT_TRUE(rm.load("fake", {}));
    EXPECT_TRUE(rm.unload("fake"));
    // After unload, resolve() must throw again.
    EXPECT_THROW(rm.resolve("fake", {}), std::runtime_error);
}

TEST(ResourceManager, unload_unknown_returns_false) {
    ResourceManager rm(providers_dir());
    EXPECT_FALSE(rm.unload("nothing-loaded"));
}

TEST(ResourceManager, rescan_picks_up_changes_in_provider_dir) {
    // We can't easily mutate the fixture install dir, so this is a smoke test:
    // rescan must not throw and the list must remain non-empty.
    ResourceManager rm(providers_dir());
    auto before = rm.list().size();
    rm.rescan();
    auto after = rm.list().size();
    EXPECT_EQ(before, after);
}

// -----------------------------------------------------------------------------
// JSON-string accessors (list_providers_alloc / list_available_providers_alloc)
// -----------------------------------------------------------------------------

TEST(ResourceManager, list_providers_alloc_returns_parseable_json_with_caller_free) {
    ResourceManager rm(providers_dir());
    const char* raw = rm.list_providers_alloc();
    ASSERT_NE(raw, nullptr);
    auto j = nlohmann::json::parse(raw);
    EXPECT_TRUE(j.is_array());
    delete[] raw;
}

TEST(ResourceManager, list_available_providers_alloc_filters_to_available_only) {
    ResourceManager rm(providers_dir());
    ASSERT_TRUE(rm.load("fake", {}));

    const char* raw_all = rm.list_providers_alloc();
    const char* raw_avail = rm.list_available_providers_alloc();
    auto all = nlohmann::json::parse(raw_all);
    auto avail = nlohmann::json::parse(raw_avail);
    delete[] raw_all;
    delete[] raw_avail;

    // The just-loaded "fake" must NOT appear in available; it's running now.
    for(const auto& entry : avail) {
        EXPECT_NE(entry["name"].get<std::string>(), "fake");
    }
    // It MUST still appear in the full list (as running).
    bool seen = false;
    for(const auto& entry : all) {
        if(entry["name"] == "fake") seen = true;
    }
    EXPECT_TRUE(seen);
}

// -----------------------------------------------------------------------------
// stop_all - shutdown sweep
// -----------------------------------------------------------------------------

TEST(ResourceManager, stop_all_releases_loaded_providers) {
    ResourceManager rm(providers_dir());
    ASSERT_TRUE(rm.load("fake", {}));
    rm.stop_all();
    EXPECT_THROW(rm.resolve("fake", {}), std::runtime_error);
}

TEST(ResourceManager, destructor_calls_stop_all) {
    {
        ResourceManager rm(providers_dir());
        ASSERT_TRUE(rm.load("fake", {}));
    }   // destructor must not throw or leak; we only assert no-crash here.
    SUCCEED();
}
