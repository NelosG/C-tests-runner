// Shared helpers for convexHull scenarios. Verify checks pbbs's invariants:
//   * Output is a CCW polygon of points that are all from the input.
//   * Every input point lies inside or on the boundary of the polygon
//     (i.e., on the left or on the line of every CCW edge).

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <dataGen.h>
#include <set>
#include <string>
#include <test_builder.h>
#include <test_data.h>
#include <utility>
#include <vector>

namespace hull_common {

    inline setup::Fn fixed_input(std::string algo,
                                 std::vector<double> xs,
                                 std::vector<double> ys) {
        return [algo = std::move(algo),
                xs = std::move(xs), ys = std::move(ys)](TestData& td) {
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                v.write_object("points", [&](TestData& p) {
                    p.write_array<double>("xs", xs);
                    p.write_array<double>("ys", ys);
                });
            });
        };
    }

    // Random 2d points in a unit square.
    inline setup::Fn random_input(std::string algo, std::size_t n,
                                  std::uint64_t seed = 0) {
        return [algo = std::move(algo), n, seed](TestData& td) {
            std::vector<double> xs(n), ys(n);
            for(std::size_t i = 0; i < n; ++i) {
                xs[i] = dataGen::hash<double>(seed * 2 + i * 2);
                ys[i] = dataGen::hash<double>(seed * 2 + i * 2 + 1);
            }
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                v.write_object("points", [&](TestData& p) {
                    p.write_array<double>("xs", xs);
                    p.write_array<double>("ys", ys);
                });
            });
        };
    }

    inline verify::Fn check_hull() {
        return [](const TestData& in, const TestData& out)
                -> std::pair<bool, std::string> {
            TestData vars = in.read_object("vars");
            TestData pd = vars.read_object("points");
            auto xs = pd.read_array<double>("xs");
            auto ys = pd.read_array<double>("ys");
            auto idx = out.read_array<std::int64_t>("hull");
            if(idx.size() < 1)
                return {false, "empty hull"};
            // indices in range
            for(auto i : idx)
                if(i < 0 || i >= static_cast<std::int64_t>(xs.size()))
                    return {false, "index " + std::to_string(i) + " out of range"};
            // CCW: every triple of consecutive hull vertices makes a left
            // turn (cross >= 0). Strict > 0 may fail on colinear edges so
            // we allow == 0.
            auto cross = [&](std::int64_t a, std::int64_t b, std::int64_t c) {
                return (xs[b]-xs[a])*(ys[c]-ys[a]) - (ys[b]-ys[a])*(xs[c]-xs[a]);
            };
            std::size_t k = idx.size();
            // pbbs's hull() returns [leftmost, ...upper L->R, rightmost,
            // ...lower R->L], which is clockwise. So every consecutive
            // triple makes a right turn (cross <= 0) and every input point
            // lies on the right side of every edge.
            for(std::size_t i = 0; i < k; ++i) {
                double c = cross(idx[i], idx[(i+1)%k], idx[(i+2)%k]);
                if(c > 1e-9)
                    return {false, "not CW at hull vertex " + std::to_string(i)};
            }
            for(std::size_t p = 0; p < xs.size(); ++p) {
                for(std::size_t e = 0; e < k; ++e) {
                    std::int64_t a = idx[e], b = idx[(e+1)%k];
                    double c = (xs[b]-xs[a])*(ys[p]-ys[a])
                             - (ys[b]-ys[a])*(xs[p]-xs[a]);
                    if(c > 1e-7) {
                        return {false,
                            "point " + std::to_string(p)
                            + " outside hull (edge "
                            + std::to_string(a) + "->" + std::to_string(b) + ")"};
                    }
                }
            }
            return {true, std::string{}};
        };
    }

    inline verify::Fn has_some_hull() {
        return [](const TestData&, const TestData& out)
                -> std::pair<bool, std::string> {
            auto a = out.read_array<std::int64_t>("hull");
            if(a.empty()) return {false, "empty"};
            return {true, std::string{}};
        };
    }

} // namespace hull_common
