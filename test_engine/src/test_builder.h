#pragma once

/**
 * @file test_builder.h
 * @brief Helpers for writing test plugins - input generators and output verifiers.
 *
 * Both setup and verify operate on TestData objects (typed key-value maps).
 *
 * Example:
 *   #include <test_builder.h>
 *
 *   class MySortTest final : public TestScenarioExtension {
 *   public:
 *       std::vector<Test> get_tests() const override {
 *           return {
 *               {"small",  setup::array<long long>("array", {5, 3, 1}),
 *                          verify::equals<long long>("result", {1, 3, 5})},
 *               {"random", setup::random_array<long long>("array", 1e6),
 *                          verify::same_elements<long long>("array", "result")},
 *           };
 *       }
 *       std::string name() const override { return "Correctness.Sort"; }
 *   };
 *   REGISTER_TEST(MySortTest)
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <random>
#include <register_test.h>
#include <string>
#include <test_data.h>
#include <test_scenario_extension.h>
#include <vector>

// ============================================================================
// Setup generators - populate the input TestData
// ============================================================================

namespace setup {

    using Fn = std::function<void(TestData&)>;

    /// Write a fixed array under `key`.
    template<typename T>
    Fn array(const std::string& key, std::vector<T> data) {
        return [key, data = std::move(data)](TestData& in) {
            in.write_array<T>(key, data);
        };
    }

    /// Write a random integer array (uniform distribution).
    template<typename T = long long>
    Fn random_array(const std::string& key, std::size_t count, std::uint64_t seed = 42) {
        return [key, count, seed](TestData& in) {
            std::mt19937_64 gen(seed);
            std::vector<T> data(count);
            for(auto& v : data) {
                if constexpr(std::is_floating_point_v<T>) {
                    v = static_cast<T>(static_cast<double>(gen()) / static_cast<double>(gen.max()));
                } else {
                    v = static_cast<T>(gen());
                }
            }
            in.write_array<T>(key, data);
        };
    }

    /// Write a single arithmetic value under `key`.
    template<typename T>
    Fn value(const std::string& key, T val) {
        return [key, val](TestData& in) {
            in.write_value<T>(key, val);
        };
    }

    /// Write a single string value under `key`.
    inline Fn string(const std::string& key, std::string s) {
        return [key, s = std::move(s)](TestData& in) {
            in.write_string(key, s);
        };
    }

    /// Write an array of strings under `key`.
    inline Fn strings(const std::string& key, std::vector<std::string> v) {
        return [key, v = std::move(v)](TestData& in) {
            in.write_strings(key, v);
        };
    }

    /// Combine multiple setup actions into one (e.g. write multiple keys).
    inline Fn combine(std::initializer_list<Fn> fns) {
        std::vector<Fn> v(fns);
        return [v = std::move(v)](TestData& in) {
            for(auto& fn : v) fn(in);
        };
    }

} // namespace setup

// ============================================================================
// Verify checkers - compare output against expected
// ============================================================================

namespace verify {

    using Fn = std::function<std::pair<bool, std::string>(
        const TestData& input,
        const TestData& output
    )>;

    /// Output array under `key` matches `expected` exactly.
    template<typename T = long long>
    Fn equals(const std::string& key, std::vector<T> expected) {
        return [key, expected = std::move(expected)](
            const TestData&,
            const TestData& out
        ) -> std::pair<bool, std::string> {
            auto actual = out.read_array<T>(key);
            if(actual == expected) return {true, std::string{}};
            if(actual.size() != expected.size())
                return {
                    false,
                    "Size: expected " + std::to_string(expected.size())
                    + ", got " + std::to_string(actual.size())
                };
            for(std::size_t i = 0; i < actual.size(); ++i) {
                if(actual[i] != expected[i])
                    return {false, "Mismatch at " + std::to_string(i)};
            }
            return {true, std::string{}};
        };
    }

    /// Output array under `output_key` matches the reference array under
    /// `reference_key` from the input map (teacher pre-computed expected result).
    template<typename T = long long>
    Fn matches_reference(const std::string& reference_key, const std::string& output_key) {
        return [reference_key, output_key](
            const TestData& in,
            const TestData& out
        )
            -> std::pair<bool, std::string> {
            auto ref = in.read_array<T>(reference_key);
            auto actual = out.read_array<T>(output_key);
            if(actual == ref) return {true, std::string{}};
            if(actual.size() != ref.size())
                return {
                    false,
                    "Size: expected " + std::to_string(ref.size())
                    + ", got " + std::to_string(actual.size())
                };
            for(std::size_t i = 0; i < actual.size(); ++i) {
                if(actual[i] != ref[i])
                    return {false, "Mismatch at " + std::to_string(i)};
            }
            return {true, std::string{}};
        };
    }

    /// Output array under `output_key` is a permutation of input array under `input_key`.
    template<typename T = long long>
    Fn same_elements(const std::string& input_key, const std::string& output_key) {
        return [input_key, output_key](
            const TestData& in,
            const TestData& out
        )
            -> std::pair<bool, std::string> {
            auto a = in.read_array<T>(input_key);
            auto b = out.read_array<T>(output_key);
            if(a.size() != b.size())
                return {
                    false,
                    "Size mismatch: expected " + std::to_string(a.size())
                    + ", got " + std::to_string(b.size())
                };
            std::sort(a.begin(), a.end());
            std::sort(b.begin(), b.end());
            return a == b
                ? std::make_pair(true, std::string{})
                : std::make_pair(false, std::string("Elements differ"));
        };
    }

    /// Output array has expected size.
    template<typename T = long long>
    Fn has_size(const std::string& key, std::size_t expected_size) {
        return [key, expected_size](
            const TestData&,
            const TestData& out
        )
            -> std::pair<bool, std::string> {
            auto data = out.read_array<T>(key);
            return data.size() == expected_size
                ? std::make_pair(true, std::string{})
                : std::make_pair(
                    false,
                    "Size: expected " + std::to_string(expected_size)
                    + ", got " + std::to_string(data.size())
                );
        };
    }

    /// Scalar value under `key` matches `expected`.
    template<typename T = long long>
    Fn value_equals(const std::string& key, T expected) {
        return [key, expected](
            const TestData&,
            const TestData& out
        )
            -> std::pair<bool, std::string> {
            try {
                T actual = out.read_value<T>(key);
                return actual == expected
                    ? std::make_pair(true, std::string{})
                    : std::make_pair(
                        false,
                        "Value: expected " + std::to_string(expected)
                        + ", got " + std::to_string(actual)
                    );
            } catch(const std::exception& e) {
                return {false, e.what()};
            }
        };
    }

} // namespace verify
