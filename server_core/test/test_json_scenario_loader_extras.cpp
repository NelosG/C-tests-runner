// Additional unit tests for JsonScenarioLoader to close verify-side gaps:
// size mismatches per type, string array mismatches, exception caught from
// read_*, custom epsilon edge cases. Existing test_json_scenario_loader.cpp
// already covers happy paths + int mismatch + missing key + bool array;
// here we exercise the other failure messages so every error string in the
// loader has a test.

#include <gtest/gtest.h>

#include <json_scenario_loader.h>
#include <test_registry.h>
#include <test_scenario_extension.h>

#include "test_temp_dir.h"

namespace {

struct JsonScenarioExtras : ::testing::Test {
    TestRegistry reg_;

    TestScenarioExtension* load_single(const std::string& json) {
        TempDir dir;
        dir.write_file("cases/x.json", json);
        JsonScenarioLoader::load(dir.path().string(), reg_);
        if(reg_.size() != 1) return nullptr;
        return reg_.all().front().get();
    }
};

} // namespace

// -----------------------------------------------------------------------------
// Numeric mismatch reporting for double scalar
// -----------------------------------------------------------------------------

TEST_F(JsonScenarioExtras, verify_double_scalar_outside_epsilon_fails_with_message) {
    auto* ext = load_single(
        R"({"name":"S","tests":[{"name":"t","input":{},"output":{"x":1.0},"epsilon":1e-9}]})");
    ASSERT_NE(ext, nullptr);
    auto tests = ext->get_tests();
    TestData out;
    out.write_value<double>("x", 1.0 + 1e-3);  // well outside epsilon
    auto [ok, msg] = tests.front().verify({}, out);
    EXPECT_FALSE(ok);
    EXPECT_NE(msg.find("x"), std::string::npos);
}

TEST_F(JsonScenarioExtras, verify_double_scalar_scale_relative_epsilon_passes) {
    // For large values the comparator scales epsilon by max(|a|,|b|), so a
    // 1e-4 absolute diff against 1e6 at eps=1e-9 passes (1e-4 << 1e-3 = eps*scale).
    auto* ext = load_single(
        R"({"name":"S","tests":[{"name":"t","input":{},"output":{"x":1.0e6},"epsilon":1e-9}]})");
    ASSERT_NE(ext, nullptr);
    auto tests = ext->get_tests();
    TestData out;
    out.write_value<double>("x", 1.0e6 + 1e-4);
    auto [ok, msg] = tests.front().verify({}, out);
    EXPECT_TRUE(ok) << msg;
}

// -----------------------------------------------------------------------------
// Bool scalar mismatch
// -----------------------------------------------------------------------------

TEST_F(JsonScenarioExtras, verify_bool_scalar_mismatch_emits_typed_message) {
    auto* ext = load_single(
        R"({"name":"S","tests":[{"name":"t","input":{},"output":{"flag":true}}]})");
    ASSERT_NE(ext, nullptr);
    auto tests = ext->get_tests();
    TestData out;
    out.write_value<bool>("flag", false);
    auto [ok, msg] = tests.front().verify({}, out);
    EXPECT_FALSE(ok);
    EXPECT_NE(msg.find("expected true"), std::string::npos);
    EXPECT_NE(msg.find("got false"), std::string::npos);
}

// -----------------------------------------------------------------------------
// String scalar mismatch
// -----------------------------------------------------------------------------

TEST_F(JsonScenarioExtras, verify_string_scalar_mismatch_shows_both_values) {
    auto* ext = load_single(
        R"({"name":"S","tests":[{"name":"t","input":{},"output":{"label":"alpha"}}]})");
    ASSERT_NE(ext, nullptr);
    auto tests = ext->get_tests();
    TestData out;
    out.write_string("label", "beta");
    auto [ok, msg] = tests.front().verify({}, out);
    EXPECT_FALSE(ok);
    EXPECT_NE(msg.find("alpha"), std::string::npos);
    EXPECT_NE(msg.find("beta"), std::string::npos);
}

// -----------------------------------------------------------------------------
// Array size mismatches per type
// -----------------------------------------------------------------------------

TEST_F(JsonScenarioExtras, verify_int_array_size_mismatch_reports_sizes) {
    auto* ext = load_single(
        R"({"name":"S","tests":[{"name":"t","input":{},"output":{"a":[1,2,3]}}]})");
    ASSERT_NE(ext, nullptr);
    auto tests = ext->get_tests();
    TestData out;
    out.write_array<long long>("a", {1, 2});
    auto [ok, msg] = tests.front().verify({}, out);
    EXPECT_FALSE(ok);
    EXPECT_NE(msg.find("size mismatch"), std::string::npos);
    EXPECT_NE(msg.find("3"), std::string::npos);
    EXPECT_NE(msg.find("2"), std::string::npos);
}

TEST_F(JsonScenarioExtras, verify_int_array_element_mismatch_reports_index) {
    auto* ext = load_single(
        R"({"name":"S","tests":[{"name":"t","input":{},"output":{"a":[1,2,3]}}]})");
    ASSERT_NE(ext, nullptr);
    auto tests = ext->get_tests();
    TestData out;
    out.write_array<long long>("a", {1, 5, 3});
    auto [ok, msg] = tests.front().verify({}, out);
    EXPECT_FALSE(ok);
    EXPECT_NE(msg.find("index 1"), std::string::npos);
}

TEST_F(JsonScenarioExtras, verify_double_array_size_mismatch_reports_sizes) {
    auto* ext = load_single(
        R"({"name":"S","tests":[{"name":"t","input":{},"output":{"v":[1.0,2.0]}}]})");
    ASSERT_NE(ext, nullptr);
    auto tests = ext->get_tests();
    TestData out;
    out.write_array<double>("v", {1.0});
    auto [ok, msg] = tests.front().verify({}, out);
    EXPECT_FALSE(ok);
    EXPECT_NE(msg.find("size mismatch"), std::string::npos);
}

TEST_F(JsonScenarioExtras, verify_double_array_element_mismatch_reports_index) {
    auto* ext = load_single(
        R"({"name":"S","tests":[{"name":"t","input":{},"output":{"v":[1.0,2.0]},"epsilon":1e-12}]})");
    ASSERT_NE(ext, nullptr);
    auto tests = ext->get_tests();
    TestData out;
    out.write_array<double>("v", {1.0, 99.0});
    auto [ok, msg] = tests.front().verify({}, out);
    EXPECT_FALSE(ok);
    EXPECT_NE(msg.find("index 1"), std::string::npos);
}

TEST_F(JsonScenarioExtras, verify_bool_array_size_mismatch_reports_sizes) {
    auto* ext = load_single(
        R"({"name":"S","tests":[{"name":"t","input":{},"output":{"b":[true,false,true]}}]})");
    ASSERT_NE(ext, nullptr);
    auto tests = ext->get_tests();
    TestData out;
    out.write_array<unsigned char>("b", {1, 0});
    auto [ok, msg] = tests.front().verify({}, out);
    EXPECT_FALSE(ok);
    EXPECT_NE(msg.find("size mismatch"), std::string::npos);
}

TEST_F(JsonScenarioExtras, verify_bool_array_element_mismatch_reports_index_and_bool_text) {
    auto* ext = load_single(
        R"({"name":"S","tests":[{"name":"t","input":{},"output":{"b":[true,true]}}]})");
    ASSERT_NE(ext, nullptr);
    auto tests = ext->get_tests();
    TestData out;
    out.write_array<unsigned char>("b", {1, 0});
    auto [ok, msg] = tests.front().verify({}, out);
    EXPECT_FALSE(ok);
    EXPECT_NE(msg.find("index 1"), std::string::npos);
    EXPECT_NE(msg.find("expected true"), std::string::npos);
    EXPECT_NE(msg.find("got false"), std::string::npos);
}

TEST_F(JsonScenarioExtras, verify_string_array_size_mismatch_reports_sizes) {
    auto* ext = load_single(
        R"({"name":"S","tests":[{"name":"t","input":{},"output":{"s":["a","b","c"]}}]})");
    ASSERT_NE(ext, nullptr);
    auto tests = ext->get_tests();
    TestData out;
    out.write_strings("s", {"a", "b"});
    auto [ok, msg] = tests.front().verify({}, out);
    EXPECT_FALSE(ok);
    EXPECT_NE(msg.find("size mismatch"), std::string::npos);
}

TEST_F(JsonScenarioExtras, verify_string_array_element_mismatch_quotes_both_values) {
    auto* ext = load_single(
        R"({"name":"S","tests":[{"name":"t","input":{},"output":{"s":["alpha","beta"]}}]})");
    ASSERT_NE(ext, nullptr);
    auto tests = ext->get_tests();
    TestData out;
    out.write_strings("s", {"alpha", "wrong"});
    auto [ok, msg] = tests.front().verify({}, out);
    EXPECT_FALSE(ok);
    EXPECT_NE(msg.find("beta"), std::string::npos);
    EXPECT_NE(msg.find("wrong"), std::string::npos);
}

// -----------------------------------------------------------------------------
// Type errors in TestData (read_* throws) are reported through the catch path
// -----------------------------------------------------------------------------

TEST_F(JsonScenarioExtras, verify_catches_exception_from_test_data_typed_read) {
    // Expected is "int (8 bytes)" but actual TestData stores 1 byte under "n"
    // (a bool). read_value<long long> throws a size-mismatch exception, which
    // the loader must catch and surface as a typed verify failure.
    auto* ext = load_single(
        R"({"name":"S","tests":[{"name":"t","input":{},"output":{"n":1}}]})");
    ASSERT_NE(ext, nullptr);
    auto tests = ext->get_tests();
    TestData out;
    out.write_value<bool>("n", true);       // 1 byte payload under key "n"
    auto [ok, msg] = tests.front().verify({}, out);
    EXPECT_FALSE(ok);
    EXPECT_NE(msg.find("'n'"), std::string::npos)
        << "error message must reference the failing field name";
}

// -----------------------------------------------------------------------------
// Setup-side: unsupported JSON type raises during setup (heterogeneous arrays
// fall through to UNKNOWN, e.g. nested object inside array).
// -----------------------------------------------------------------------------

TEST_F(JsonScenarioExtras, setup_throws_on_unsupported_json_type) {
    auto* ext = load_single(
        R"({"name":"S","tests":[{"name":"t","input":{"o":{"k":1}},"output":{}}]})");
    ASSERT_NE(ext, nullptr);
    auto tests = ext->get_tests();
    TestData in;
    EXPECT_THROW(tests.front().setup(in), std::runtime_error);
}

// -----------------------------------------------------------------------------
// Empty-array tagging - both expected and actual end up empty -> pass
// -----------------------------------------------------------------------------

TEST_F(JsonScenarioExtras, empty_int_array_round_trips) {
    auto* ext = load_single(
        R"({"name":"S","tests":[{"name":"t","input":{},"output":{"a":[]}}]})");
    ASSERT_NE(ext, nullptr);
    auto tests = ext->get_tests();
    TestData out;
    out.write_array<long long>("a", {});
    auto [ok, msg] = tests.front().verify({}, out);
    EXPECT_TRUE(ok) << msg;
}
