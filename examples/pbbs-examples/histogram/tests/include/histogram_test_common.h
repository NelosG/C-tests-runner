// Shared setup and verify helpers for histogram scenarios.

#pragma once

#include <cstdint>
#include <dataGen.h>
#include <string>
#include <test_builder.h>
#include <test_data.h>
#include <vector>

namespace histogram_common {

    // Build a `vars` object with the dispatch key, the input array, and
    // the bucket count.
    inline setup::Fn fixed_input(std::string algo,
                                 std::vector<std::uint32_t> in,
                                 std::uint32_t buckets) {
        return [algo = std::move(algo),
                in = std::move(in), buckets](TestData& td) {
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                v.write_array<std::uint32_t>("in", in);
                v.write_value<std::uint32_t>("buckets", buckets);
            });
        };
    }

    // Random input via dataGen::hash (deterministic per index).
    inline setup::Fn random_input(std::string algo, std::size_t n,
                                  std::uint32_t buckets,
                                  std::uint64_t seed = 0) {
        return [algo = std::move(algo), n, buckets, seed](TestData& td) {
            std::vector<std::uint32_t> in(n);
            for(std::size_t i = 0; i < n; ++i) {
                in[i] = static_cast<std::uint32_t>(
                    dataGen::hash<unsigned int>(seed + i) % buckets);
            }
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                v.write_array<std::uint32_t>("in", in);
                v.write_value<std::uint32_t>("buckets", buckets);
            });
        };
    }

    // Verify: output[b] equals the number of occurrences of `b` in input.
    // This is the invariant the pbbsbench check would enforce - we compute
    // the reference histogram sequentially and compare.
    inline verify::Fn matches_input_counts() {
        return [](const TestData& in, const TestData& out)
                -> std::pair<bool, std::string> {
            TestData vars = in.read_object("vars");
            auto input = vars.read_array<std::uint32_t>("in");
            auto buckets = vars.read_value<std::uint32_t>("buckets");
            auto output = out.read_array<std::uint32_t>("result");
            if(output.size() != buckets)
                return {false,
                    "size: expected " + std::to_string(buckets)
                    + ", got " + std::to_string(output.size())};
            std::vector<std::uint32_t> expected(buckets, 0);
            for(auto v: input) {
                if(v >= buckets)
                    return {false, "input value " + std::to_string(v)
                        + " out of [0, " + std::to_string(buckets) + ")"};
                ++expected[v];
            }
            for(std::uint32_t b = 0; b < buckets; ++b) {
                if(output[b] != expected[b])
                    return {false,
                        "bucket " + std::to_string(b)
                        + ": expected " + std::to_string(expected[b])
                        + ", got " + std::to_string(output[b])};
            }
            return {true, std::string{}};
        };
    }

    inline verify::Fn has_size(std::uint32_t expected) {
        return [expected](const TestData&, const TestData& out)
                -> std::pair<bool, std::string> {
            auto a = out.read_array<std::uint32_t>("result");
            if(a.size() != expected)
                return {false,
                    "size: expected " + std::to_string(expected)
                    + ", got " + std::to_string(a.size())};
            return {true, std::string{}};
        };
    }

} // namespace histogram_common
