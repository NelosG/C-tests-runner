// Shared setup generators and verify functions for all four sort variants.
// Every per-variant scenario class reuses these helpers - the only thing
// that changes between scenarios is the "algo" dispatch key.

#pragma once

#include <algorithm>
#include <cstdint>
#include <dataGen.h>                  // vendored from pbbsbench/common/dataGen.h
#include <string>
#include <test_builder.h>
#include <test_data.h>
#include <vector>

namespace sort_common {

    // Build an input object with the dispatch key + a fixed array.
    // Elements are int (4-byte), matching pbbs's comparisonSort which sorts
    // `int` with std::less<int> for a `randomSeq -t int` input.
    inline setup::Fn fixed_input(std::string algo, std::vector<int> arr) {
        return [algo = std::move(algo), arr = std::move(arr)](TestData& in) {
            in.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                v.write_array<int>("arr", arr);
            });
        };
    }

    // Build an input object with the dispatch key + a random array
    // generated through pbbsbench's `dataGen::hash<int>` (deterministic
    // per index, the same generator pbbs's randomSeq uses for `-t int`).
    inline setup::Fn random_input(std::string algo, std::size_t n,
                                  std::uint64_t seed = 0) {
        return [algo = std::move(algo), n, seed](TestData& in) {
            std::vector<int> arr(n);
            for(std::size_t i = 0; i < n; ++i) {
                arr[i] = dataGen::hash<int>(seed + i);
            }
            in.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                v.write_array<int>("arr", arr);
            });
        };
    }

    // Verify the output is a sorted permutation of the input. This mirrors
    // pbbsbench/benchmarks/comparisonSort/bench/sortCheck.C's check_sort -
    // it sorts the input independently and compares element-wise with the
    // submitted output.
    inline verify::Fn sorted_permutation_of_input() {
        return [](const TestData& in, const TestData& out)
                -> std::pair<bool, std::string> {
            TestData vars = in.read_object("vars");
            auto input = vars.read_array<int>("arr");
            auto output = out.read_array<int>("result");
            if(input.size() != output.size())
                return {
                    false,
                    "size mismatch: input " + std::to_string(input.size())
                    + ", output " + std::to_string(output.size())
                };
            std::sort(input.begin(), input.end());
            for(std::size_t i = 0; i < input.size(); ++i) {
                if(input[i] != output[i])
                    return {
                        false,
                        "comparison sort: check failed at location i="
                        + std::to_string(i)
                        + " expected " + std::to_string(input[i])
                        + " got " + std::to_string(output[i])
                    };
            }
            return {true, std::string{}};
        };
    }

    // Output has the expected size (for performance scenarios that don't
    // need to re-verify correctness on huge inputs).
    inline verify::Fn has_size(std::size_t expected) {
        return [expected](const TestData&, const TestData& out)
                -> std::pair<bool, std::string> {
            auto a = out.read_array<int>("result");
            if(a.size() != expected)
                return {
                    false,
                    "size: expected " + std::to_string(expected)
                    + ", got " + std::to_string(a.size())
                };
            return {true, std::string{}};
        };
    }

} // namespace sort_common
