// Helpers for delaunayTriangulation scenarios. Verify checks the
// Delaunay empty-circle property: no circumscribed circle of an output
// triangle contains any input point.

#pragma once

#include <cmath>
#include <cstdint>
#include <dataGen.h>
#include <set>
#include <string>
#include <test_builder.h>
#include <test_data.h>
#include <utility>
#include <vector>

namespace delaunay_common {

    inline setup::Fn random_input(std::string algo, std::int64_t n,
                                  std::uint64_t seed = 0) {
        return [algo = std::move(algo), n, seed](TestData& td) {
            std::vector<double> xs(n), ys(n);
            for(std::int64_t i = 0; i < n; ++i) {
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

    inline double in_circle(double ax, double ay, double bx, double by,
                            double cx, double cy, double dx, double dy) {
        double adx = ax - dx, ady = ay - dy;
        double bdx = bx - dx, bdy = by - dy;
        double cdx = cx - dx, cdy = cy - dy;
        double abdet = adx*bdy - bdx*ady;
        double bcdet = bdx*cdy - cdx*bdy;
        double cadet = cdx*ady - adx*cdy;
        double alift = adx*adx + ady*ady;
        double blift = bdx*bdx + bdy*bdy;
        double clift = cdx*cdx + cdy*cdy;
        return alift*bcdet + blift*cadet + clift*abdet;
    }

    inline verify::Fn check_delaunay() {
        return [](const TestData& in, const TestData& out)
                -> std::pair<bool, std::string> {
            TestData vars = in.read_object("vars");
            TestData pd = vars.read_object("points");
            auto xs = pd.read_array<double>("xs");
            auto ys = pd.read_array<double>("ys");
            auto tri = out.read_array<std::int64_t>("triangles");
            std::int64_t n = static_cast<std::int64_t>(xs.size());
            std::int64_t nt = tri.size() / 3;
            if(nt == 0)
                return {false, "no triangles"};
            // For each triangle, empty-circle: no other input point lies
            // strictly inside the circumcircle.
            constexpr double tol = 1e-6;
            for(std::int64_t t = 0; t < nt; ++t) {
                std::int64_t a = tri[t*3], b = tri[t*3+1], c = tri[t*3+2];
                if(a < 0 || a >= n || b < 0 || b >= n || c < 0 || c >= n)
                    return {false, "tri " + std::to_string(t) + " idx oob"};
                // Ensure ccw orientation for the circumcircle test.
                double cross = (xs[b]-xs[a])*(ys[c]-ys[a])
                             - (ys[b]-ys[a])*(xs[c]-xs[a]);
                if(cross == 0) continue; // degenerate, skip
                double ax = xs[a], ay = ys[a];
                double bx = xs[b], by = ys[b];
                double cx = xs[c], cy = ys[c];
                if(cross < 0) { std::swap(bx, cx); std::swap(by, cy); }
                for(std::int64_t p = 0; p < n; ++p) {
                    if(p == a || p == b || p == c) continue;
                    if(in_circle(ax, ay, bx, by, cx, cy, xs[p], ys[p]) > tol)
                        return {false,
                            "point " + std::to_string(p)
                            + " inside circumcircle of tri "
                            + std::to_string(t)};
                }
            }
            return {true, std::string{}};
        };
    }

    inline verify::Fn has_some_tris() {
        return [](const TestData&, const TestData& out)
                -> std::pair<bool, std::string> {
            auto a = out.read_array<std::int64_t>("triangles");
            if(a.empty() || a.size() % 3 != 0)
                return {false, "bad size " + std::to_string(a.size())};
            return {true, std::string{}};
        };
    }

} // namespace delaunay_common
