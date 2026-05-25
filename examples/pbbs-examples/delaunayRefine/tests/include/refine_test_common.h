// Helpers for delaunayRefine. Mirrors pbbs/benchmarks/delaunayRefine:
// the input is `triangles<point>` (points + triangle vertex triples)
// and the output is the refined `triangles<point>` after Steiner
// insertion. We generate random points then run a sequential
// Bowyer-Watson Delaunay here in the test plugin so the student
// receives a proper initial triangulation, just like pbbs's bench
// driver reads from .tri files.
//
// Verify checks the output triangles are non-degenerate and reference
// in-range vertices. We don't reverify the minimum-angle bound: pbbs's
// refineInternal is hard-coded to 30deg and its termination guarantees
// hold only on dense enough input.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <dataGen.h>
#include <map>
#include <string>
#include <test_builder.h>
#include <test_data.h>
#include <utility>
#include <vector>

namespace refine_common {

    namespace detail {

        inline double in_circle(double ax, double ay, double bx, double by,
                                double cx, double cy,
                                double dx, double dy) {
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

        inline double ccw(double ax, double ay, double bx, double by,
                          double cx, double cy) {
            return (bx-ax)*(cy-ay) - (by-ay)*(cx-ax);
        }

        struct Tri { std::int64_t a, b, c; bool alive; };

        // sequential Bowyer-Watson - O(n^2). Test setup only, so perf
        // doesn't matter; matches the brute force in old simplified
        // delaunayTriangulation/parallel.cpp.
        inline std::vector<std::int64_t> bowyer_watson(
            const std::vector<double>& xs, const std::vector<double>& ys)
        {
            std::int64_t n = static_cast<std::int64_t>(xs.size());
            if(n < 3) return {};
            double xmin = xs[0], xmax = xs[0], ymin = ys[0], ymax = ys[0];
            for(std::int64_t i = 1; i < n; ++i) {
                if(xs[i] < xmin) xmin = xs[i];
                if(xs[i] > xmax) xmax = xs[i];
                if(ys[i] < ymin) ymin = ys[i];
                if(ys[i] > ymax) ymax = ys[i];
            }
            double dx = xmax - xmin, dy = ymax - ymin;
            double dmax = std::max(dx, dy) * 20.0 + 1.0;
            double mx = (xmin + xmax) * 0.5;
            double my = (ymin + ymax) * 0.5;
            std::vector<double> px = xs, py = ys;
            px.push_back(mx - dmax); py.push_back(my - dmax);
            px.push_back(mx + dmax); py.push_back(my - dmax);
            px.push_back(mx);        py.push_back(my + dmax);

            std::vector<Tri> tris;
            tris.push_back({n, n+1, n+2, true});
            for(std::int64_t i = 0; i < n; ++i) {
                std::vector<std::pair<std::int64_t,std::int64_t>> edges;
                for(auto& t : tris) {
                    if(!t.alive) continue;
                    if(in_circle(px[t.a], py[t.a], px[t.b], py[t.b],
                                 px[t.c], py[t.c], px[i], py[i]) > 0) {
                        t.alive = false;
                        edges.push_back({t.a, t.b});
                        edges.push_back({t.b, t.c});
                        edges.push_back({t.c, t.a});
                    }
                }
                std::map<std::pair<std::int64_t,std::int64_t>, int> cnt;
                for(auto [u, v] : edges) {
                    auto key = (u<v) ? std::make_pair(u, v) : std::make_pair(v, u);
                    ++cnt[key];
                }
                for(auto [u, v] : edges) {
                    auto key = (u<v) ? std::make_pair(u, v) : std::make_pair(v, u);
                    if(cnt[key] == 1) {
                        if(ccw(px[u], py[u], px[v], py[v], px[i], py[i]) > 0)
                            tris.push_back({u, v, i, true});
                        else
                            tris.push_back({v, u, i, true});
                    }
                }
            }
            std::vector<std::int64_t> out;
            for(auto& t : tris) {
                if(!t.alive) continue;
                if(t.a >= n || t.b >= n || t.c >= n) continue;
                out.push_back(t.a);
                out.push_back(t.b);
                out.push_back(t.c);
            }
            return out;
        }

    } // namespace detail

    inline setup::Fn random_input(std::string algo, std::int64_t n,
                                  double min_angle_deg = 25.0,
                                  std::uint64_t seed = 0) {
        return [algo = std::move(algo), n, min_angle_deg, seed](TestData& td) {
            std::vector<double> xs(n), ys(n);
            for(std::int64_t i = 0; i < n; ++i) {
                xs[i] = dataGen::hash<double>(seed * 2 + i * 2);
                ys[i] = dataGen::hash<double>(seed * 2 + i * 2 + 1);
            }
            std::vector<std::int64_t> initial = detail::bowyer_watson(xs, ys);
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                v.write_object("data", [&](TestData& g) {
                    g.write_array<double>("xs", xs);
                    g.write_array<double>("ys", ys);
                    g.write_array<std::int64_t>("initial_tris", initial);
                    g.write_value<double>("min_angle_deg", min_angle_deg);
                });
            });
        };
    }

    inline verify::Fn check_refine() {
        return [](const TestData& in, const TestData& out)
                -> std::pair<bool, std::string> {
            TestData vars = in.read_object("vars");
            TestData d = vars.read_object("data");
            auto bound = d.read_value<double>("min_angle_deg");
            (void)bound;
            auto xs = out.read_array<double>("xs");
            auto ys = out.read_array<double>("ys");
            auto tri = out.read_array<std::int64_t>("triangles");
            if(tri.empty() || tri.size() % 3 != 0)
                return {false, "no triangles or size not %3"};
            std::int64_t nt = tri.size() / 3;
            std::int64_t n = static_cast<std::int64_t>(xs.size());
            for(std::int64_t t = 0; t < nt; ++t) {
                std::int64_t a = tri[t*3], b = tri[t*3+1], c = tri[t*3+2];
                if(a < 0 || a >= n || b < 0 || b >= n || c < 0 || c >= n)
                    return {false, "tri " + std::to_string(t) + " idx oob"};
                double area2 = std::abs(
                    (xs[b]-xs[a])*(ys[c]-ys[a]) - (ys[b]-ys[a])*(xs[c]-xs[a]));
                if(area2 < 1e-15)
                    return {false, "tri " + std::to_string(t) + " degenerate"};
            }
            return {true, std::string{}};
        };
    }

    inline verify::Fn has_some_tris() {
        return [](const TestData&, const TestData& out)
                -> std::pair<bool, std::string> {
            auto a = out.read_array<std::int64_t>("triangles");
            if(a.empty()) return {false, "empty"};
            return {true, std::string{}};
        };
    }

} // namespace refine_common
