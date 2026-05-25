// Shared helpers for integerSort scenarios.

#pragma once

#include <algorithm>
#include <cstdint>
#include <dataGen.h>
#include <string>
#include <test_builder.h>
#include <test_data.h>
#include <vector>

namespace isort_common {

    inline setup::Fn fixed_input(std::string algo,
                                 std::vector<std::uint32_t> in,
                                 std::size_t bits) {
        return [algo = std::move(algo),
                in = std::move(in), bits](TestData& td) {
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                v.write_array<std::uint32_t>("in", in);
                v.write_value<std::size_t>("bits", bits);
            });
        };
    }

    inline setup::Fn random_input(std::string algo, std::size_t n,
                                  std::size_t bits, std::uint64_t seed = 0) {
        return [algo = std::move(algo), n, bits, seed](TestData& td) {
            std::vector<std::uint32_t> in(n);
            std::uint32_t mask = (bits >= 32)
                ? 0xFFFFFFFFu
                : ((std::uint32_t{1} << bits) - 1);
            for(std::size_t i = 0; i < n; ++i) {
                in[i] = static_cast<std::uint32_t>(
                    dataGen::hash<unsigned int>(seed + i) & mask);
            }
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                v.write_array<std::uint32_t>("in", in);
                v.write_value<std::size_t>("bits", bits);
            });
        };
    }

    // Mirror of pbbsbench/common/checkSort: sort input independently and
    // compare elementwise with the runner's output.
    inline verify::Fn check_sort() {
        return [](const TestData& in, const TestData& out)
                -> std::pair<bool, std::string> {
            TestData vars = in.read_object("vars");
            auto input  = vars.read_array<std::uint32_t>("in");
            auto output = out.read_array<std::uint32_t>("result");
            if(input.size() != output.size())
                return {false,
                    "size mismatch: input " + std::to_string(input.size())
                    + ", output " + std::to_string(output.size())};
            std::sort(input.begin(), input.end());
            for(std::size_t i = 0; i < input.size(); ++i) {
                if(input[i] != output[i])
                    return {false,
                        "integerSort: check failed at i=" + std::to_string(i)
                        + " expected " + std::to_string(input[i])
                        + " got " + std::to_string(output[i])};
            }
            return {true, std::string{}};
        };
    }

    inline verify::Fn has_size(std::size_t expected) {
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

} // namespace isort_common
