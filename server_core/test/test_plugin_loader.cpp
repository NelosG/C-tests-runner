// Unit tests for PluginLoader.
//
// Uses a tiny dummy_test_plugin .so/.dll fixture (built alongside the test
// executable) to exercise the load/unload paths. The fixture has no symbols
// the engine queries - we only verify that dlopen/LoadLibrary returns a
// non-null handle and that the loader stores it.

#include <gtest/gtest.h>

#include <filesystem>

#include <plugin_loader.h>

namespace fs = std::filesystem;

// Path to the fixture shared library, injected at compile time via target_compile_definitions.
#ifndef CTR_DUMMY_PLUGIN_PATH
#  error "CTR_DUMMY_PLUGIN_PATH must be defined by CMake"
#endif

namespace {
    fs::path fixture_path() { return fs::path(CTR_DUMMY_PLUGIN_PATH); }
}

TEST(PluginLoader, default_constructed_loader_owns_nothing) {
    PluginLoader loader;
    // Nothing to load -> unload_all is a no-op and must not crash.
    EXPECT_NO_THROW(loader.unload_all());
}

TEST(PluginLoader, load_plugin_returns_true_for_valid_so) {
    PluginLoader loader;
    ASSERT_TRUE(fs::exists(fixture_path())) << "fixture missing at " << fixture_path();
    EXPECT_TRUE(loader.load_plugin(fixture_path().string()));
}

TEST(PluginLoader, load_plugin_returns_false_for_missing_file) {
    PluginLoader loader;
    auto missing = fixture_path().parent_path() / "this_file_does_not_exist.so";
    EXPECT_FALSE(loader.load_plugin(missing.string()));
}

TEST(PluginLoader, load_plugin_returns_false_for_non_library_file) {
    PluginLoader loader;
    // The fixture .cpp source is not a shared library - dlopen must reject it.
    auto src = fixture_path().parent_path() / "dummy_plugin.cpp";
    if(!fs::exists(src)) GTEST_SKIP() << "source not present at install location";
    EXPECT_FALSE(loader.load_plugin(src.string()));
}

TEST(PluginLoader, load_same_plugin_twice_succeeds_both_times) {
    PluginLoader loader;
    EXPECT_TRUE(loader.load_plugin(fixture_path().string()));
    // dlopen returns a refcounted handle on POSIX; LoadLibrary refs on Win32.
    // The loader stores both handles, and both should free cleanly.
    EXPECT_TRUE(loader.load_plugin(fixture_path().string()));
}

TEST(PluginLoader, unload_all_clears_handles_and_is_idempotent) {
    PluginLoader loader;
    ASSERT_TRUE(loader.load_plugin(fixture_path().string()));
    EXPECT_NO_THROW(loader.unload_all());
    // Second call is a no-op now that handles_ is empty.
    EXPECT_NO_THROW(loader.unload_all());
}

TEST(PluginLoader, destructor_unloads_remaining_handles) {
    // Construct + load + destruct - destructor calls unload_all. We only
    // verify "no crash" since we can't observe handle counts directly.
    {
        PluginLoader loader;
        ASSERT_TRUE(loader.load_plugin(fixture_path().string()));
    }
    SUCCEED();
}

TEST(PluginLoader, move_construction_transfers_handles) {
    PluginLoader src;
    ASSERT_TRUE(src.load_plugin(fixture_path().string()));
    PluginLoader dst(std::move(src));
    // src is now empty; dst owns the handle. Both unload_all calls must
    // be safe and free no-or-one handle each respectively.
    EXPECT_NO_THROW(src.unload_all());
    EXPECT_NO_THROW(dst.unload_all());
}

TEST(PluginLoader, move_assignment_releases_existing_handles_first) {
    PluginLoader a;
    PluginLoader b;
    ASSERT_TRUE(a.load_plugin(fixture_path().string()));
    ASSERT_TRUE(b.load_plugin(fixture_path().string()));
    // Assigning b -> a: a's existing handle must be freed, then b's moved in.
    a = std::move(b);
    // Both should now be safe to destroy.
    SUCCEED();
}
