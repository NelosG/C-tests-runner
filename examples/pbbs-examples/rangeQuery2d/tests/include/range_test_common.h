// rangeQuery2d test contract mirrors pbbs/benchmarks/rangeQuery2d/bench/rangeTime.C:
// the input is a single sequence of 2D points; the first 2*num_queries
// of them are interpreted as pairs (a, b) defining query rectangles
// (one corner each); the rest are the data points. The algorithm
// returns one long: the total count over all queries.

#pragma once

#include <algorithm>
#include <cstdint>
#include <dataGen.h>
#include <string>
#include <test_builder.h>
#include <test_data.h>
#include <utility>
#include <vector>

namespace range_common {

    // pbbs reads `n` 2D points and splits them: first `2*num_q` define
    // queries (paired up as opposite corners), remaining n-2*num_q are
    // the data points. `num_q = n/3` in pbbs's driver; we accept the
    // ratio as a parameter so tests can dial it.
    inline setup::Fn random_input(std::string algo,
                                  std::int64_t n_total,
                                  std::int64_t n_queries,
                                  std::uint64_t seed = 0) {
        return [algo = std::move(algo), n_total, n_queries, seed](TestData& td) {
            std::vector<double> all_x(n_total), all_y(n_total);
            for(std::int64_t i = 0; i < n_total; ++i) {
                all_x[i] = dataGen::hash<double>(seed * 2 + i * 2);
                all_y[i] = dataGen::hash<double>(seed * 2 + i * 2 + 1);
            }
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                v.write_object("data", [&](TestData& g) {
                    g.write_array<double>("all_x", all_x);
                    g.write_array<double>("all_y", all_y);
                    g.write_value<std::int64_t>("n_queries", n_queries);
                });
            });
        };
    }

    inline verify::Fn check_total() {
        return [](const TestData& in, const TestData& out)
                -> std::pair<bool, std::string> {
            TestData vars = in.read_object("vars");
            TestData g = vars.read_object("data");
            auto all_x = g.read_array<double>("all_x");
            auto all_y = g.read_array<double>("all_y");
            auto n_q = g.read_value<std::int64_t>("n_queries");
            auto total = out.read_value<std::int64_t>("total");

            std::int64_t n_total = static_cast<std::int64_t>(all_x.size());
            std::int64_t n_pts = n_total - 2 * n_q;

            std::int64_t ref = 0;
            for(std::int64_t i = 0; i < n_q; ++i) {
                double x1 = std::min(all_x[2*i], all_x[2*i+1]);
                double x2 = std::max(all_x[2*i], all_x[2*i+1]);
                double y1 = std::min(all_y[2*i], all_y[2*i+1]);
                double y2 = std::max(all_y[2*i], all_y[2*i+1]);
                for(std::int64_t j = 2 * n_q; j < n_total; ++j) {
                    if(all_x[j] >= x1 && all_x[j] <= x2
                        && all_y[j] >= y1 && all_y[j] <= y2)
                        ++ref;
                }
                (void)n_pts;
            }
            if(total != ref)
                return {false, "total expected " + std::to_string(ref)
                    + " got " + std::to_string(total)};
            return {true, std::string{}};
        };
    }

    inline verify::Fn is_nonneg() {
        return [](const TestData&, const TestData& out)
                -> std::pair<bool, std::string> {
            auto t = out.read_value<std::int64_t>("total");
            if(t < 0) return {false, "negative total"};
            return {true, std::string{}};
        };
    }

} // namespace range_common
