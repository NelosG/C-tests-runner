// Shared helpers for removeDuplicates scenarios.

#pragma once

#include <algorithm>
#include <cstdint>
#include <dataGen.h>
#include <string>
#include <test_builder.h>
#include <test_data.h>
#include <vector>

namespace dedup_common {

    inline setup::Fn fixed_input(std::string algo,
                                 std::vector<std::uint32_t> in) {
        return [algo = std::move(algo), in = std::move(in)](TestData& td) {
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                v.write_array<std::uint32_t>("in", in);
            });
        };
    }

    // Random integers in a constrained range to force duplicates.
    inline setup::Fn random_input(std::string algo, std::size_t n,
                                  std::uint32_t range,
                                  std::uint64_t seed = 0) {
        return [algo = std::move(algo), n, range, seed](TestData& td) {
            std::vector<std::uint32_t> in(n);
            for(std::size_t i = 0; i < n; ++i) {
                in[i] = static_cast<std::uint32_t>(
                    dataGen::hash<unsigned int>(seed + i) % range);
            }
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                v.write_array<std::uint32_t>("in", in);
            });
        };
    }

    // Verify: sorted unique(input) == sorted output. This is the
    // structural invariant pbbsbench's check would enforce - output is
    // exactly the set of distinct input values, no element repeated, no
    // element missing.
    inline verify::Fn check_dedup() {
        return [](const TestData& in, const TestData& out)
                -> std::pair<bool, std::string> {
            TestData vars = in.read_object("vars");
            auto input  = vars.read_array<std::uint32_t>("in");
            auto output = out.read_array<std::uint32_t>("result");

            std::sort(input.begin(), input.end());
            input.erase(std::unique(input.begin(), input.end()),
                        input.end());
            std::sort(output.begin(), output.end());

            if(output.size() != input.size())
                return {false,
                    "size: expected " + std::to_string(input.size())
                    + " distinct, got " + std::to_string(output.size())};
            for(std::size_t i = 0; i < input.size(); ++i) {
                if(input[i] != output[i])
                    return {false,
                        "check failed at i=" + std::to_string(i)
                        + " expected " + std::to_string(input[i])
                        + " got " + std::to_string(output[i])};
            }
            return {true, std::string{}};
        };
    }

    inline verify::Fn has_max_size(std::size_t max_expected) {
        return [max_expected](const TestData&, const TestData& out)
                -> std::pair<bool, std::string> {
            auto a = out.read_array<std::uint32_t>("result");
            if(a.size() > max_expected)
                return {false,
                    "size " + std::to_string(a.size())
                    + " > max " + std::to_string(max_expected)};
            return {true, std::string{}};
        };
    }

} // namespace dedup_common
