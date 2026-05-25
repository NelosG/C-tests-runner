// Shared helpers for suffixArray scenarios. Verify checks:
//   1. SA is a permutation of [0, n).
//   2. The suffixes are in increasing lexicographic order.

#pragma once

#include <algorithm>
#include <cstdint>
#include <dataGen.h>
#include <string>
#include <test_builder.h>
#include <test_data.h>
#include <utility>
#include <vector>

namespace sa_common {

    inline setup::Fn fixed_input(std::string algo, std::string s) {
        return [algo = std::move(algo), s = std::move(s)](TestData& td) {
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                v.write_string("s", s);
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
            });
        };
    }

    inline verify::Fn check_sa() {
        return [](const TestData& in, const TestData& out)
                -> std::pair<bool, std::string> {
            TestData vars = in.read_object("vars");
            auto s = vars.read_string("s");
            auto sa = out.read_array<std::int64_t>("sa");
            std::size_t n = s.size();
            if(sa.size() != n)
                return {false, "sa size " + std::to_string(sa.size())
                    + " != n " + std::to_string(n)};
            // Permutation of [0,n).
            std::vector<bool> seen(n, false);
            for(auto x : sa) {
                if(x < 0 || x >= static_cast<std::int64_t>(n))
                    return {false, "SA contains " + std::to_string(x)
                        + " out of [0," + std::to_string(n) + ")"};
                if(seen[x])
                    return {false, "SA contains duplicate " + std::to_string(x)};
                seen[x] = true;
            }
            // Lex order.
            for(std::size_t i = 0; i + 1 < n; ++i) {
                std::int64_t a = sa[i], b = sa[i+1];
                // Compare suffix a with suffix b.
                std::size_t la = n - a, lb = n - b;
                std::size_t mn = std::min(la, lb);
                int cmp = std::memcmp(s.data() + a, s.data() + b, mn);
                if(cmp > 0 || (cmp == 0 && la > lb))
                    return {false,
                        "SA not sorted at i=" + std::to_string(i)
                        + " (sa[i]=" + std::to_string(a)
                        + ", sa[i+1]=" + std::to_string(b) + ")"};
            }
            return {true, std::string{}};
        };
    }

    inline verify::Fn has_size(std::size_t expected) {
        return [expected](const TestData&, const TestData& out)
                -> std::pair<bool, std::string> {
            auto a = out.read_array<std::int64_t>("sa");
            if(a.size() != expected)
                return {false, "size " + std::to_string(a.size())
                    + " != " + std::to_string(expected)};
            return {true, std::string{}};
        };
    }

} // namespace sa_common
