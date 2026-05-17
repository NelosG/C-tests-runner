// Unit tests for JsonScenarioLoader - turns JSON files under <test_dir>/cases/
// into virtual TestScenarioExtension entries in a TestRegistry.

#include <gtest/gtest.h>
#include <json_scenario_loader.h>
#include <test_data.h>
#include <test_registry.h>
#include <test_scenario_extension.h>

#include "test_temp_dir.h"


namespace {

    /// Find a registered scenario by exact name. Returns nullptr if not found.
    TestScenarioExtension* find_by_name(TestRegistry& reg, const std::string& name) {
        for(const auto& s : reg.all()) {
            if(s->name() == name) return s.get();
        }
        return nullptr;
    }

} // namespace

// -----------------------------------------------------------------------------
// Loader basics
// -----------------------------------------------------------------------------

TEST(JsonScenarioLoader, no_cases_dir_loads_nothing_silently) {
    TempDir dir;
    TestRegistry reg;
    JsonScenarioLoader::load(dir.path().string(), reg);
    EXPECT_EQ(reg.size(), 0u);
}

TEST(JsonScenarioLoader, empty_cases_dir_loads_nothing) {
    TempDir dir;
    std::filesystem::create_directories(dir.path() / "cases");
    TestRegistry reg;
    JsonScenarioLoader::load(dir.path().string(), reg);
    EXPECT_EQ(reg.size(), 0u);
}

TEST(JsonScenarioLoader, non_json_files_are_skipped) {
    TempDir dir;
    dir.write_file("cases/readme.txt", "not a scenario");
    TestRegistry reg;
    JsonScenarioLoader::load(dir.path().string(), reg);
    EXPECT_EQ(reg.size(), 0u);
}

TEST(JsonScenarioLoader, scenario_is_registered_with_its_name) {
    TempDir dir;
    dir.write_file(
        "cases/sum.json",
        R"({
        "name": "Correctness.Sum",
        "tests": [
            { "name": "trivial", "input": {}, "output": {} }
        ]
    })"
    );
    TestRegistry reg;
    JsonScenarioLoader::load(dir.path().string(), reg);
    ASSERT_EQ(reg.size(), 1u);
    EXPECT_EQ(reg.all().front()->name(), "Correctness.Sum");
}

TEST(JsonScenarioLoader, default_type_is_correctness) {
    TempDir dir;
    dir.write_file(
        "cases/x.json",
        R"({
        "name": "S",
        "tests": []
    })"
    );
    TestRegistry reg;
    JsonScenarioLoader::load(dir.path().string(), reg);
    ASSERT_EQ(reg.size(), 1u);
    EXPECT_EQ(reg.all().front()->scenario_type(), ScenarioType::CORRECTNESS);
}

TEST(JsonScenarioLoader, explicit_performance_type_is_picked_up) {
    TempDir dir;
    dir.write_file(
        "cases/x.json",
        R"({
        "name": "S",
        "type": "performance",
        "tests": []
    })"
    );
    TestRegistry reg;
    JsonScenarioLoader::load(dir.path().string(), reg);
    ASSERT_EQ(reg.size(), 1u);
    EXPECT_EQ(reg.all().front()->scenario_type(), ScenarioType::PERFORMANCE);
}

TEST(JsonScenarioLoader, malformed_scenario_file_does_not_crash_or_register) {
    TempDir dir;
    dir.write_file("cases/bad.json", "{this isn't valid json");
    dir.write_file(
        "cases/good.json",
        R"({
        "name": "Good", "tests": []
    })"
    );
    TestRegistry reg;
    EXPECT_NO_THROW(JsonScenarioLoader::load(dir.path().string(), reg));
    EXPECT_EQ(reg.size(), 1u);
    EXPECT_NE(find_by_name(reg, "Good"), nullptr);
}

TEST(JsonScenarioLoader, scenario_missing_name_field_is_skipped) {
    TempDir dir;
    // No "name" key - must be skipped, not registered with empty name.
    dir.write_file("cases/x.json", R"({ "tests": [] })");
    TestRegistry reg;
    JsonScenarioLoader::load(dir.path().string(), reg);
    EXPECT_EQ(reg.size(), 0u);
}

// -----------------------------------------------------------------------------
// Test materialisation - setup writes typed values into TestData
// -----------------------------------------------------------------------------

class JsonScenarioRoundTrip : public ::testing::Test {
    protected:
        TestRegistry reg_;

        TestScenarioExtension* load_single(const std::string& json) {
            TempDir dir;
            dir.write_file("cases/x.json", json);
            JsonScenarioLoader::load(dir.path().string(), reg_);
            if(reg_.size() != 1) return nullptr;
            // Owned by the registry; safe to hold the pointer for the test lifetime.
            return reg_.all().front().get();
        }
};

TEST_F(JsonScenarioRoundTrip, setup_writes_int_scalar) {
    auto* ext = load_single(
        R"({
        "name": "S",
        "tests": [
            { "name": "t", "input": { "n": 42 }, "output": {} }
        ]
    })"
    );
    ASSERT_NE(ext, nullptr);
    auto tests = ext->get_tests();
    ASSERT_EQ(tests.size(), 1u);

    TestData in;
    tests.front().setup(in);
    EXPECT_EQ(in.read_value<long long>("n"), 42);
}

TEST_F(JsonScenarioRoundTrip, setup_writes_double_scalar) {
    auto* ext = load_single(
        R"({
        "name": "S",
        "tests": [
            { "name": "t", "input": { "x": 3.14 }, "output": {} }
        ]
    })"
    );
    ASSERT_NE(ext, nullptr);
    auto tests = ext->get_tests();
    TestData in;
    tests.front().setup(in);
    EXPECT_DOUBLE_EQ(in.read_value<double>("x"), 3.14);
}

TEST_F(JsonScenarioRoundTrip, setup_writes_bool_scalar) {
    auto* ext = load_single(
        R"({
        "name": "S",
        "tests": [
            { "name": "t", "input": { "flag": true }, "output": {} }
        ]
    })"
    );
    ASSERT_NE(ext, nullptr);
    auto tests = ext->get_tests();
    TestData in;
    tests.front().setup(in);
    EXPECT_TRUE(in.read_value<bool>("flag"));
}

TEST_F(JsonScenarioRoundTrip, setup_writes_string_scalar) {
    auto* ext = load_single(
        R"({
        "name": "S",
        "tests": [
            { "name": "t", "input": { "label": "hello" }, "output": {} }
        ]
    })"
    );
    ASSERT_NE(ext, nullptr);
    auto tests = ext->get_tests();
    TestData in;
    tests.front().setup(in);
    EXPECT_EQ(in.read_string("label"), "hello");
}

TEST_F(JsonScenarioRoundTrip, setup_writes_int_array) {
    auto* ext = load_single(
        R"({
        "name": "S",
        "tests": [
            { "name": "t", "input": { "arr": [1, 2, 3] }, "output": {} }
        ]
    })"
    );
    ASSERT_NE(ext, nullptr);
    auto tests = ext->get_tests();
    TestData in;
    tests.front().setup(in);
    EXPECT_EQ(in.read_array<long long>("arr"), (std::vector<long long>{1, 2, 3}));
}

TEST_F(JsonScenarioRoundTrip, setup_writes_double_array) {
    auto* ext = load_single(
        R"({
        "name": "S",
        "tests": [
            { "name": "t", "input": { "v": [1.5, 2.5] }, "output": {} }
        ]
    })"
    );
    ASSERT_NE(ext, nullptr);
    auto tests = ext->get_tests();
    TestData in;
    tests.front().setup(in);
    EXPECT_EQ(in.read_array<double>("v"), (std::vector<double>{1.5, 2.5}));
}

TEST_F(JsonScenarioRoundTrip, setup_writes_string_array) {
    auto* ext = load_single(
        R"({
        "name": "S",
        "tests": [
            { "name": "t", "input": { "tags": ["a", "bb", "ccc"] }, "output": {} }
        ]
    })"
    );
    ASSERT_NE(ext, nullptr);
    auto tests = ext->get_tests();
    TestData in;
    tests.front().setup(in);
    EXPECT_EQ(
        in.read_strings("tags"),
        (std::vector<std::string>{"a", "bb", "ccc"})
    );
}

// -----------------------------------------------------------------------------
// verify() - JSON expected vs actual TestData
// -----------------------------------------------------------------------------

TEST_F(JsonScenarioRoundTrip, verify_passes_when_int_matches) {
    auto* ext = load_single(
        R"({
        "name": "S",
        "tests": [{ "name": "t", "input": {}, "output": { "sum": 6 } }]
    })"
    );
    ASSERT_NE(ext, nullptr);
    auto tests = ext->get_tests();

    TestData out;
    out.write_value<long long>("sum", 6);
    auto [ok, msg] = tests.front().verify({}, out);
    EXPECT_TRUE(ok) << msg;
}

TEST_F(JsonScenarioRoundTrip, verify_fails_when_int_differs) {
    auto* ext = load_single(
        R"({
        "name": "S",
        "tests": [{ "name": "t", "input": {}, "output": { "sum": 6 } }]
    })"
    );
    ASSERT_NE(ext, nullptr);
    auto tests = ext->get_tests();

    TestData out;
    out.write_value<long long>("sum", 5);
    auto [ok, msg] = tests.front().verify({}, out);
    EXPECT_FALSE(ok);
    EXPECT_NE(msg.find("sum"), std::string::npos);
}

TEST_F(JsonScenarioRoundTrip, verify_reports_missing_output_key) {
    auto* ext = load_single(
        R"({
        "name": "S",
        "tests": [{ "name": "t", "input": {}, "output": { "required": 1 } }]
    })"
    );
    ASSERT_NE(ext, nullptr);
    auto tests = ext->get_tests();

    TestData out;  // empty - does not contain "required"
    auto [ok, msg] = tests.front().verify({}, out);
    EXPECT_FALSE(ok);
    EXPECT_NE(msg.find("missing"), std::string::npos);
    EXPECT_NE(msg.find("required"), std::string::npos);
}

TEST_F(JsonScenarioRoundTrip, verify_uses_default_epsilon_for_doubles) {
    auto* ext = load_single(
        R"({
        "name": "S",
        "tests": [{ "name": "t", "input": {}, "output": { "x": 1.0 } }]
    })"
    );
    ASSERT_NE(ext, nullptr);
    auto tests = ext->get_tests();

    TestData out;
    out.write_value<double>("x", 1.0 + 1e-12);  // well within default 1e-9
    auto [ok, msg] = tests.front().verify({}, out);
    EXPECT_TRUE(ok) << msg;
}

TEST_F(JsonScenarioRoundTrip, verify_honours_custom_epsilon) {
    // doubles_equal() inside the loader uses BOTH an absolute and a
    // scale-relative tolerance (diff <= eps OR diff <= eps * scale).
    // Pick a value pair where both bounds disagree on the verdict.
    auto* ext = load_single(
        R"({
        "name": "S",
        "tests": [{
            "name": "t", "input": {}, "output": { "x": 100.0 }, "epsilon": 1e-6
        }]
    })"
    );
    ASSERT_NE(ext, nullptr);
    auto tests = ext->get_tests();

    // Within both abs (1e-6) and scaled (1e-6 * 100 = 1e-4) tolerance -> pass.
    TestData out;
    out.write_value<double>("x", 100.0 + 5e-5);
    auto [ok, msg] = tests.front().verify({}, out);
    EXPECT_TRUE(ok) << msg;

    // Difference of 0.01 - bigger than both abs and scaled tolerance -> fail.
    TestData far;
    far.write_value<double>("x", 100.01);
    auto [ok2, msg2] = tests.front().verify({}, far);
    EXPECT_FALSE(ok2);
}

TEST_F(JsonScenarioRoundTrip, verify_compares_int_arrays_elementwise) {
    auto* ext = load_single(
        R"({
        "name": "S",
        "tests": [{ "name": "t", "input": {}, "output": { "v": [1, 2, 3] } }]
    })"
    );
    ASSERT_NE(ext, nullptr);
    auto tests = ext->get_tests();

    TestData good;
    good.write_array<long long>("v", {1, 2, 3});
    EXPECT_TRUE(tests.front().verify({}, good).first);

    TestData wrong_size;
    wrong_size.write_array<long long>("v", {1, 2});
    auto [ok, msg] = tests.front().verify({}, wrong_size);
    EXPECT_FALSE(ok);
    EXPECT_NE(msg.find("size mismatch"), std::string::npos);

    TestData wrong_elem;
    wrong_elem.write_array<long long>("v", {1, 2, 4});
    auto [ok2, msg2] = tests.front().verify({}, wrong_elem);
    EXPECT_FALSE(ok2);
}

TEST_F(JsonScenarioRoundTrip, verify_handles_bool_arrays) {
    auto* ext = load_single(
        R"({
        "name": "S",
        "tests": [{
            "name": "t", "input": {}, "output": { "flags": [true, false, true] }
        }]
    })"
    );
    ASSERT_NE(ext, nullptr);
    auto tests = ext->get_tests();

    TestData out;
    // bool arrays are stored as unsigned char on the runner side (matches setup).
    out.write_array<unsigned char>("flags", {1u, 0u, 1u});
    auto [ok, msg] = tests.front().verify({}, out);
    EXPECT_TRUE(ok) << msg;
}
