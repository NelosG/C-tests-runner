// Closes the dll_plugin_helpers gap: load_and_resolve() paths.
// test_dll_plugin_helpers.cpp covers NamingScheme / ScanPluginDir / EffectiveConfig
// already; load_and_resolve() requires a real .so handle which we get from the
// dummy fixture (same one used by test_plugin_loader.cpp).

#include <gtest/gtest.h>

#include <filesystem>

#include <dll_plugin_helpers.h>

namespace fs = std::filesystem;

#ifndef CTR_DUMMY_PLUGIN_PATH
#  error "CTR_DUMMY_PLUGIN_PATH must be defined by CMake"
#endif

namespace {
    fs::path fixture_path() { return fs::path(CTR_DUMMY_PLUGIN_PATH); }
}

TEST(LoadAndResolve, missing_file_yields_empty_handle_with_error) {
    auto missing = fixture_path().parent_path() / "no_such_lib.so";
    auto out = dll_plugin_helpers::load_and_resolve(missing.string(),
        "anything", "anything_else");
    EXPECT_EQ(out.handle, nullptr);
    EXPECT_NE(out.error.find("Failed to load"), std::string::npos);
    EXPECT_EQ(out.create_sym, nullptr);
    EXPECT_EQ(out.destroy_sym, nullptr);
}

TEST(LoadAndResolve, missing_symbols_release_handle_and_report_names) {
    // Fixture .so exists but lacks the requested symbols ("create_provider" etc.).
    auto out = dll_plugin_helpers::load_and_resolve(
        fixture_path().string(),
        "create_xxx",
        "destroy_xxx");
    EXPECT_EQ(out.handle, nullptr) << "missing symbols must release the handle";
    EXPECT_NE(out.error.find("create_xxx"), std::string::npos);
    EXPECT_NE(out.error.find("destroy_xxx"), std::string::npos);
}

TEST(LoadAndResolve, found_symbol_returns_non_null) {
    // The dummy fixture exports dummy_plugin_marker - request it by name.
    // We only need ONE symbol resolved to verify the success path; we ask
    // for the same symbol as both create / destroy because the fixture
    // doesn't export distinct ones.
    auto out = dll_plugin_helpers::load_and_resolve(
        fixture_path().string(),
        "dummy_plugin_marker",
        "dummy_plugin_marker");
    EXPECT_NE(out.handle, nullptr);
    EXPECT_NE(out.create_sym, nullptr);
    EXPECT_NE(out.destroy_sym, nullptr);
    // Clean up the handle so we don't leak.
    if(out.handle) dll::free(out.handle);
}
