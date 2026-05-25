#include "json_scenario_loader.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <log_utils.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <test.h>
#include <test_data.h>
#include <test_registry.h>
#include <test_scenario_extension.h>

namespace fs = std::filesystem;


namespace {

    // ============================================================================
    // JSON value classification
    // ============================================================================

    enum class JsonKind {
        UNKNOWN,
        INT,           ///< nlohmann integer (signed/unsigned)
        DOUBLE,        ///< nlohmann float
        BOOL,          ///< true / false
        STRING,        ///< "..."
        INT_ARRAY,
        DOUBLE_ARRAY,
        BOOL_ARRAY,
        STRING_ARRAY,
        EMPTY_ARRAY,   ///< [] - treat as INT_ARRAY by default (zero elements anyway)
    };

    JsonKind classify(const nlohmann::json& v) {
        if(v.is_number_integer()) return JsonKind::INT;
        if(v.is_number_float()) return JsonKind::DOUBLE;
        if(v.is_boolean()) return JsonKind::BOOL;
        if(v.is_string()) return JsonKind::STRING;
        if(v.is_array()) {
            if(v.empty()) return JsonKind::EMPTY_ARRAY;
            const auto& first = v[0];
            if(first.is_number_integer()) return JsonKind::INT_ARRAY;
            if(first.is_number_float()) return JsonKind::DOUBLE_ARRAY;
            if(first.is_boolean()) return JsonKind::BOOL_ARRAY;
            if(first.is_string()) return JsonKind::STRING_ARRAY;
        }
        return JsonKind::UNKNOWN;
    }

    // ============================================================================
    // Setup: copy JSON value into TestData under `key`
    // ============================================================================

    void write_json_to_test_data(TestData& td, const std::string& key, const nlohmann::json& v) {
        switch(classify(v)) {
            case JsonKind::INT: td.write_value<long long>(key, v.get<long long>());
                break;
            case JsonKind::DOUBLE: td.write_value<double>(key, v.get<double>());
                break;
            case JsonKind::BOOL: td.write_value<bool>(key, v.get<bool>());
                break;
            case JsonKind::STRING: td.write_string(key, v.get<std::string>());
                break;
            case JsonKind::INT_ARRAY:
            case JsonKind::EMPTY_ARRAY:
                td.write_array<long long>(key, v.get<std::vector<long long>>());
                break;
            case JsonKind::DOUBLE_ARRAY:
                td.write_array<double>(key, v.get<std::vector<double>>());
                break;
            case JsonKind::BOOL_ARRAY: {
                std::vector<bool> bvec = v.get<std::vector<bool>>();
                // std::vector<bool> is bit-packed; convert to writable storage
                std::vector<unsigned char> u8(bvec.size());
                for(std::size_t i = 0; i < bvec.size(); ++i) u8[i] = bvec[i] ? 1 : 0;
                td.write_array<unsigned char>(key, u8);
                break;
            }
            case JsonKind::STRING_ARRAY:
                td.write_strings(key, v.get<std::vector<std::string>>());
                break;
            case JsonKind::UNKNOWN:
                throw std::runtime_error("JSON scenario: unsupported type for key '" + key + "'");
        }
    }

    // ============================================================================
    // Verify: compare expected JSON value against actual TestData entry
    // ============================================================================

    template<typename T>
    std::string mismatch_message(const std::string& key, std::size_t i, T exp, T got) {
        if constexpr(std::is_same_v<T, std::string>) {
            return "Field '" + key + "': mismatch at index " + std::to_string(i)
                + ": expected \"" + exp + "\", got \"" + got + "\"";
        } else {
            return "Field '" + key + "': mismatch at index " + std::to_string(i)
                + ": expected " + std::to_string(exp) + ", got " + std::to_string(got);
        }
    }

    bool doubles_equal(double a, double b, double epsilon) {
        if(a == b) return true;
        double diff = std::fabs(a - b);
        if(diff <= epsilon) return true;
        double scale = std::max(std::fabs(a), std::fabs(b));
        return diff <= epsilon * scale;
    }

    std::pair<bool, std::string> compare_double_array(
        const std::string& key,
        const std::vector<double>& expected,
        const std::vector<double>& actual,
        double eps
    ) {
        if(actual.size() != expected.size())
            return {
                false,
                "Field '" + key + "': size mismatch (expected "
                + std::to_string(expected.size()) + ", got "
                + std::to_string(actual.size()) + ")"
            };
        for(std::size_t i = 0; i < expected.size(); ++i) {
            if(!doubles_equal(expected[i], actual[i], eps))
                return {false, mismatch_message<double>(key, i, expected[i], actual[i])};
        }
        return {true, std::string{}};
    }

    std::pair<bool, std::string> verify_json_against_output(
        const std::string& key,
        const nlohmann::json& expected,
        const TestData& output,
        double eps
    ) {
        if(!output.contains(key))
            return {false, "Output is missing key '" + key + "'"};

        try {
            switch(classify(expected)) {
                case JsonKind::INT: {
                    long long e = expected.get<long long>();
                    long long a = output.read_value<long long>(key);
                    if(a != e)
                        return {
                            false,
                            "Field '" + key + "': expected "
                            + std::to_string(e) + ", got " + std::to_string(a)
                        };
                    return {true, {}};
                }
                case JsonKind::DOUBLE: {
                    double e = expected.get<double>();
                    double a = output.read_value<double>(key);
                    if(!doubles_equal(e, a, eps))
                        return {
                            false,
                            "Field '" + key + "': expected "
                            + std::to_string(e) + ", got " + std::to_string(a)
                        };
                    return {true, {}};
                }
                case JsonKind::BOOL: {
                    bool e = expected.get<bool>();
                    bool a = output.read_value<bool>(key);
                    if(a != e)
                        return {
                            false,
                            "Field '" + key + "': expected "
                            + (e ? "true" : "false") + ", got " + (a ? "true" : "false")
                        };
                    return {true, {}};
                }
                case JsonKind::STRING: {
                    std::string e = expected.get<std::string>();
                    std::string a = output.read_string(key);
                    if(a != e)
                        return {
                            false,
                            "Field '" + key + "': expected \""
                            + e + "\", got \"" + a + "\""
                        };
                    return {true, {}};
                }
                case JsonKind::INT_ARRAY:
                case JsonKind::EMPTY_ARRAY: {
                    auto e = expected.get<std::vector<long long>>();
                    auto a = output.read_array<long long>(key);
                    if(a == e) return {true, {}};
                    if(a.size() != e.size())
                        return {
                            false,
                            "Field '" + key + "': size mismatch (expected "
                            + std::to_string(e.size()) + ", got " + std::to_string(a.size()) + ")"
                        };
                    for(std::size_t i = 0; i < a.size(); ++i)
                        if(a[i] != e[i]) return {false, mismatch_message<long long>(key, i, e[i], a[i])};
                    return {true, {}};
                }
                case JsonKind::DOUBLE_ARRAY: {
                    auto e = expected.get<std::vector<double>>();
                    auto a = output.read_array<double>(key);
                    return compare_double_array(key, e, a, eps);
                }
                case JsonKind::BOOL_ARRAY: {
                    auto e_raw = expected.get<std::vector<bool>>();
                    auto a_raw = output.read_array<unsigned char>(key);
                    if(a_raw.size() != e_raw.size())
                        return {
                            false,
                            "Field '" + key + "': size mismatch (expected "
                            + std::to_string(e_raw.size()) + ", got " + std::to_string(a_raw.size()) + ")"
                        };
                    for(std::size_t i = 0; i < e_raw.size(); ++i) {
                        bool actual_bit = a_raw[i] != 0;
                        bool expected_bit = e_raw[i];
                        if(actual_bit != expected_bit)
                            return {
                                false,
                                "Field '" + key + "': mismatch at index "
                                + std::to_string(i) + ": expected "
                                + (expected_bit ? "true" : "false") + ", got "
                                + (actual_bit ? "true" : "false")
                            };
                    }
                    return {true, {}};
                }
                case JsonKind::STRING_ARRAY: {
                    auto e = expected.get<std::vector<std::string>>();
                    auto a = output.read_strings(key);
                    if(a.size() != e.size())
                        return {
                            false,
                            "Field '" + key + "': size mismatch (expected "
                            + std::to_string(e.size()) + ", got " + std::to_string(a.size()) + ")"
                        };
                    for(std::size_t i = 0; i < a.size(); ++i)
                        if(a[i] != e[i]) return {false, mismatch_message<std::string>(key, i, e[i], a[i])};
                    return {true, {}};
                }
                case JsonKind::UNKNOWN:
                    return {false, "Field '" + key + "': unsupported expected type"};
            }
        } catch(const std::exception& ex) {
            return {false, "Field '" + key + "': " + ex.what()};
        }
        return {false, "Field '" + key + "': unreachable"};
    }

    // ============================================================================
    // Per-test definition extracted from a JSON scenario file
    // ============================================================================

    struct TestDef {
        std::string name;
        nlohmann::json input;
        nlohmann::json output;   ///< object {key: expected_value, ...}
        double epsilon = 1e-9;
        // Optional: absolute path of a ready-made TLV blob. When set,
        // `input` is ignored and the file is fed directly into the
        // sandbox as input.bin. Resolved against the JSON file's
        // directory in load().
        std::string input_file;
        // Optional: absolute path of an expected TLV output blob. When
        // set the output is byte-compared with this file and `output`
        // is ignored.
        std::string expected_output_file;
    };

    // ============================================================================
    // JsonScenario - TestScenarioExtension that materialises Tests from TestDefs
    // ============================================================================

    class JsonScenario : public TestScenarioExtension {
        public:
            JsonScenario(std::string scenario_name, ScenarioType type, std::vector<TestDef> defs)
                : name_(std::move(scenario_name)), type_(type), defs_(std::move(defs)) {}

            std::string name() const override { return name_; }
            ScenarioType scenario_type() const override { return type_; }

            std::vector<Test> get_tests() const override {
                std::vector<Test> tests;
                tests.reserve(defs_.size());

                for(const auto& def : defs_) {
                    const auto input_obj = def.input;
                    const auto output_obj = def.output;
                    const double eps = def.epsilon;
                    const bool has_input_file = !def.input_file.empty();

                    Test::SetupFn setup_fn;
                    if(!has_input_file) {
                        setup_fn = [input_obj](TestData& in) {
                            if(!input_obj.is_object())
                                throw std::runtime_error("JSON scenario: 'input' must be an object");
                            for(auto it = input_obj.begin(); it != input_obj.end(); ++it) {
                                write_json_to_test_data(in, it.key(), it.value());
                            }
                        };
                    }

                    Test::VerifyFn verify_fn;
                    if(def.expected_output_file.empty()) {
                        verify_fn = [output_obj, eps](
                            const TestData& /*in*/,
                            const TestData& out
                        ) -> std::pair<bool, std::string> {
                            if(!output_obj.is_object())
                                return {true, std::string{}};
                            for(auto it = output_obj.begin(); it != output_obj.end(); ++it) {
                                auto [ok, msg] = verify_json_against_output(it.key(), it.value(), out, eps);
                                if(!ok) return {false, msg};
                            }
                            return {true, std::string{}};
                        };
                    }

                    Test t{def.name, std::move(setup_fn), std::move(verify_fn)};
                    t.raw_input_path = def.input_file;
                    t.expected_output_path = def.expected_output_file;
                    tests.push_back(std::move(t));
                }

                return tests;
            }

        private:
            std::string name_;
            ScenarioType type_;
            std::vector<TestDef> defs_;
    };

} // anonymous namespace

void JsonScenarioLoader::load(const std::string& test_dir, TestRegistry& registry) {
    fs::path cases_dir = fs::path(test_dir) / "cases";
    if(!fs::exists(cases_dir) || !fs::is_directory(cases_dir)) return;

    std::error_code iter_ec;
    for(auto it = fs::directory_iterator(cases_dir, iter_ec);
        !iter_ec && it != fs::directory_iterator();
        it.increment(iter_ec)) {
        std::error_code op_ec;
        if(!it->is_regular_file(op_ec) || op_ec) continue;
        if(it->path().extension() != ".json") continue;

        try {
            std::ifstream f(it->path());
            auto j = nlohmann::json::parse(f);

            std::string scenario_name = j.at("name").get<std::string>();
            ScenarioType type = ScenarioType::CORRECTNESS;
            if(j.contains("type") && j["type"].get<std::string>() == "performance")
                type = ScenarioType::PERFORMANCE;

            const auto& tests_arr = j.at("tests");
            // Relative input_file / expected_output_file paths resolve
            // against the json file's directory (e.g. `<test_dir>/cases/`).
            fs::path json_dir = it->path().parent_path();
            std::vector<TestDef> defs;
            for(const auto& t : tests_arr) {
                TestDef d;
                d.name = t.at("name").get<std::string>();
                d.input = t.value("input", nlohmann::json::object());
                d.output = t.value("output", nlohmann::json::object());
                d.epsilon = t.value("epsilon", 1e-9);
                if(t.contains("input_file")) {
                    fs::path p = t["input_file"].get<std::string>();
                    if(p.is_relative()) p = json_dir / p;
                    d.input_file = fs::weakly_canonical(p).string();
                }
                if(t.contains("expected_output_file")) {
                    fs::path p = t["expected_output_file"].get<std::string>();
                    if(p.is_relative()) p = json_dir / p;
                    d.expected_output_file = fs::weakly_canonical(p).string();
                }
                defs.push_back(std::move(d));
            }

            std::size_t test_count = defs.size();
            std::string display_name = scenario_name;
            registry.register_test(
                std::make_unique<JsonScenario>(
                    std::move(scenario_name),
                    type,
                    std::move(defs)
                )
            );

            LOG("JsonScenarioLoader") << "Loaded " << it->path().filename().generic_string()
                << " (" << display_name << ", " << test_count << " tests)\n";
        } catch(const std::exception& e) {
            LOG_ERR("JsonScenarioLoader") << "Skipping "
                << it->path().filename().generic_string() << ": " << e.what() << "\n";
        }
    }
}
