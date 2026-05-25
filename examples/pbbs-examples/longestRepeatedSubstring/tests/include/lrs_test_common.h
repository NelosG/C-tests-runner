// Shared helpers for LRS scenarios. Verify checks:
//   1. (length, pos1, pos2) point to two valid positions in s.
//   2. The substrings at those positions match for `length` chars.
//   3. Length is at least the brute-force LRS for small inputs, or at
//      least the planted repeated substring length when we control the
//      data.

#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <dataGen.h>
#include <string>
#include <test_builder.h>
#include <test_data.h>
#include <utility>
#include <vector>

namespace lrs_common {

    inline setup::Fn fixed_input(std::string algo, std::string s) {
        return [algo = std::move(algo), s = std::move(s)](TestData& td) {
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                v.write_string("s", s);
                v.write_value<std::int64_t>("min_expected", -1);
            });
        };
    }

    // Constructed input with a planted repeat of known length.
    inline setup::Fn planted_input(std::string algo, std::string base,
                                   std::string repeat) {
        std::string s = base + repeat + "X" + repeat;
        std::int64_t min_expected = static_cast<std::int64_t>(repeat.size());
        return [algo = std::move(algo), s = std::move(s),
                min_expected](TestData& td) {
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                v.write_string("s", s);
                v.write_value<std::int64_t>("min_expected", min_expected);
            });
        };
    }

    inline setup::Fn random_input(std::string algo, std::size_t n,
                                  std::int64_t alphabet = 26,
                                  std::uint64_t seed = 0) {
        return [algo = std::move(algo), n, alphabet, seed](TestData& td) {
            std::string s(n, '\0');
            for(std::size_t i = 0; i < n; ++i) {
                s[i] = static_cast<char>(
                    'a' + (dataGen::hash<unsigned int>(seed + i) % alphabet));
            }
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                v.write_string("s", s);
                v.write_value<std::int64_t>("min_expected", -1);
            });
        };
    }

    inline verify::Fn check_lrs() {
        return [](const TestData& in, const TestData& out)
                -> std::pair<bool, std::string> {
            TestData vars = in.read_object("vars");
            auto s = vars.read_string("s");
            auto min_expected = vars.read_value<std::int64_t>("min_expected");
            auto length = out.read_value<std::int64_t>("length");
            auto pos1 = out.read_value<std::int64_t>("pos1");
            auto pos2 = out.read_value<std::int64_t>("pos2");
            std::int64_t n = static_cast<std::int64_t>(s.size());
            if(pos1 < 0 || pos2 < 0 || pos1 >= n || pos2 >= n)
                return {false, "positions out of range"};
            if(pos1 == pos2)
                return {false, "pos1 == pos2"};
            if(length < 0 || pos1 + length > n || pos2 + length > n)
                return {false, "length goes past end"};
            // substring match
            if(std::memcmp(s.data() + pos1, s.data() + pos2, length) != 0)
                return {false, "substrings at pos1 and pos2 don't match"};
            if(min_expected >= 0 && length < min_expected)
                return {false,
                    "length " + std::to_string(length)
                    + " < planted minimum " + std::to_string(min_expected)};
            return {true, std::string{}};
        };
    }

    inline verify::Fn well_formed() {
        return [](const TestData& in, const TestData& out)
                -> std::pair<bool, std::string> {
            TestData vars = in.read_object("vars");
            auto s = vars.read_string("s");
            auto length = out.read_value<std::int64_t>("length");
            auto pos1 = out.read_value<std::int64_t>("pos1");
            auto pos2 = out.read_value<std::int64_t>("pos2");
            std::int64_t n = static_cast<std::int64_t>(s.size());
            if(pos1 < 0 || pos2 < 0 || pos1 >= n || pos2 >= n) return {false, "oob"};
            if(length < 0 || pos1 + length > n || pos2 + length > n)
                return {false, "length past end"};
            return {true, std::string{}};
        };
    }

} // namespace lrs_common
