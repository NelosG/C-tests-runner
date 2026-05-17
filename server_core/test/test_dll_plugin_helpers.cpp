// Unit tests for dll_plugin_helpers - the pure-logic parts shared between
// AdapterManager and ResourceManager. We don't try to load real DLLs here:
// only the path/name/config primitives (which run without any DLL handle).

#include <dll_plugin_helpers.h>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "test_temp_dir.h"

using namespace dll_plugin_helpers;

// -----------------------------------------------------------------------------
// NamingScheme::name_from_stem - strip platform-specific prefix/suffix
// -----------------------------------------------------------------------------

TEST(NamingScheme, suffix_only_strips_suffix) {
    NamingScheme s{"", "_adapter"};
    EXPECT_EQ(s.name_from_stem("http_adapter"), "http");
    EXPECT_EQ(s.name_from_stem("rabbit_adapter"), "rabbit");
}

TEST(NamingScheme, prefix_only_strips_prefix) {
    NamingScheme s{"resource_", ""};
    EXPECT_EQ(s.name_from_stem("resource_git"), "git");
    EXPECT_EQ(s.name_from_stem("resource_local"), "local");
}

TEST(NamingScheme, no_prefix_suffix_match_returns_stem_verbatim) {
    NamingScheme s{"resource_", ""};
    EXPECT_EQ(s.name_from_stem("legacy_plugin"), "legacy_plugin")
        << "stems that don't match the scheme should be returned unchanged "
           "(older plugin layouts)";
}

TEST(NamingScheme, prefix_equal_to_stem_does_not_strip) {
    // Implementation requires `stem.size() > prefix.size()`, otherwise we'd
    // strip the whole name and end up with "".
    NamingScheme s{"resource_", ""};
    EXPECT_EQ(s.name_from_stem("resource_"), "resource_");
}

TEST(NamingScheme, empty_scheme_returns_stem_verbatim) {
    NamingScheme s{"", ""};
    EXPECT_EQ(s.name_from_stem("whatever_name"), "whatever_name");
}

TEST(NamingScheme, prefix_match_wins_over_suffix_when_both_apply) {
    // Both prefix and suffix patterns could match - code branches on prefix
    // first, so it should win.
    NamingScheme s{"plug_", "_xyz"};
    EXPECT_EQ(s.name_from_stem("plug_thing_xyz"), "thing_xyz");
}

// -----------------------------------------------------------------------------
// scan_plugin_dir - pick up .dll/.so files and map logical name -> absolute path
// -----------------------------------------------------------------------------

namespace {

    /// Platform-correct shared-lib filename for the test's fake DLLs.
    std::string with_ext(const std::string& base) {
        #ifdef _WIN32
        return base + ".dll";
        #else
        return base + ".so";
        #endif
    }

    /// Write an empty file at `dir/name` - scan_plugin_dir only inspects
    /// extension and stem, never opens the file.
    void touch(const std::filesystem::path& dir, const std::string& name) {
        std::ofstream(dir / name).close();
    }

} // namespace

TEST(ScanPluginDir, missing_dir_returns_empty_map_without_throwing) {
    auto found = scan_plugin_dir(
        "definitely/missing/dir",
        {"", "_adapter"},
        "TestLogger"
    );
    EXPECT_TRUE(found.empty());
}

TEST(ScanPluginDir, finds_only_matching_extension) {
    TempDir dir;
    touch(dir.path(), with_ext("http_adapter"));
    touch(dir.path(), with_ext("rabbit_adapter"));
    touch(dir.path(), "README.md");
    touch(dir.path(), "http_adapter.lib");   // import lib - not a shared lib
    touch(dir.path(), "junk.txt");

    auto found = scan_plugin_dir(dir.path(), {"", "_adapter"}, "TestLogger");
    EXPECT_EQ(found.size(), 2u);
    EXPECT_TRUE(found.count("http"));
    EXPECT_TRUE(found.count("rabbit"));
}

TEST(ScanPluginDir, mapped_paths_are_usable_for_dll_load) {
    TempDir dir;
    auto file = dir.path() / with_ext("http_adapter");
    std::ofstream(file).close();

    auto found = scan_plugin_dir(dir.path(), {"", "_adapter"}, "TestLogger");
    ASSERT_EQ(found.size(), 1u);
    // Map value must be an actual filesystem path pointing at the touched file.
    EXPECT_EQ(
        std::filesystem::absolute(found.at("http")),
        std::filesystem::absolute(file)
    );
}

TEST(ScanPluginDir, applies_prefix_scheme_for_resource_providers) {
    TempDir dir;
    touch(dir.path(), with_ext("resource_git"));
    touch(dir.path(), with_ext("resource_local"));
    touch(dir.path(), with_ext("not_a_provider"));    // doesn't match prefix

    auto found = scan_plugin_dir(dir.path(), {"resource_", ""}, "ResourceManager");
    ASSERT_EQ(found.size(), 3u) << "scan should keep ALL .dlls - stems without "
                                   "the prefix fall back to verbatim";
    EXPECT_TRUE(found.count("git"));
    EXPECT_TRUE(found.count("local"));
    EXPECT_TRUE(found.count("not_a_provider"));
}

// -----------------------------------------------------------------------------
// effective_config - prefer caller-supplied JSON, else read fallback file
// -----------------------------------------------------------------------------

TEST(EffectiveConfig, non_empty_provided_wins_over_file) {
    TempDir cfg_dir;
    cfg_dir.write_file("resource-git.json", R"({"from": "file"})");

    nlohmann::json supplied = {{"from", "caller"}};
    auto out = effective_config(supplied, cfg_dir.path(), "resource-git.json");
    EXPECT_EQ(out.value("from", ""), "caller");
}

TEST(EffectiveConfig, empty_provided_falls_through_to_file) {
    TempDir cfg_dir;
    cfg_dir.write_file("resource-git.json", R"({"baseDir": "/cache"})");

    auto out = effective_config(
        nlohmann::json{},
        cfg_dir.path(),
        "resource-git.json"
    );
    EXPECT_EQ(out.value("baseDir", std::string{}), "/cache");
}

TEST(EffectiveConfig, empty_provided_and_missing_file_yields_empty) {
    TempDir cfg_dir;   // no file written
    auto out = effective_config(
        nlohmann::json{},
        cfg_dir.path(),
        "resource-git.json"
    );
    EXPECT_TRUE(out.empty());
}

TEST(EffectiveConfig, empty_config_dir_skips_file_lookup) {
    // When config_dir is empty there's nowhere to look - even empty `provided`
    // is returned as-is, not silently replaced.
    auto out = effective_config(
        nlohmann::json{},
        std::filesystem::path{},
        "anything.json"
    );
    EXPECT_TRUE(out.empty());
}
