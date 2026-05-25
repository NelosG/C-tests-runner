// Helpers for rayCast scenarios. Verify checks against a sequential
// brute-force reference.

#pragma once

#include <cmath>
#include <cstdint>
#include <dataGen.h>
#include <limits>
#include <string>
#include <test_builder.h>
#include <test_data.h>
#include <utility>
#include <vector>

namespace raycast_common {

    inline setup::Fn random_input(std::string algo,
                                  std::int64_t num_tris,
                                  std::int64_t num_rays,
                                  std::uint64_t seed = 0) {
        return [algo = std::move(algo), num_tris, num_rays, seed](TestData& td) {
            std::int64_t num_verts = num_tris * 3;
            std::vector<double> verts(num_verts * 3);
            std::vector<std::int64_t> idx(num_tris * 3);
            // Each triangle is 3 new vertices, deterministic random.
            for(std::int64_t i = 0; i < num_verts * 3; ++i)
                verts[i] = dataGen::hash<double>(seed + i);
            for(std::int64_t i = 0; i < num_tris; ++i) {
                idx[i*3]   = i*3;
                idx[i*3+1] = i*3+1;
                idx[i*3+2] = i*3+2;
            }
            std::vector<double> ro(num_rays * 3), rd(num_rays * 3);
            for(std::int64_t i = 0; i < num_rays * 3; ++i) {
                ro[i] = dataGen::hash<double>(seed + 1'000'000 + i);
                rd[i] = dataGen::hash<double>(seed + 2'000'000 + i) + 0.5;
            }
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                v.write_object("data", [&](TestData& g) {
                    g.write_value<std::int64_t>("num_verts", num_verts);
                    g.write_array<double>("verts", verts);
                    g.write_value<std::int64_t>("num_tris", num_tris);
                    g.write_array<std::int64_t>("idx", idx);
                    g.write_value<std::int64_t>("num_rays", num_rays);
                    g.write_array<double>("ray_origin", ro);
                    g.write_array<double>("ray_dir", rd);
                });
            });
        };
    }

    struct Vec3 { double x, y, z; };
    inline Vec3 sub(Vec3 a, Vec3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
    inline Vec3 cross(Vec3 a, Vec3 b) {
        return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
    }
    inline double dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
    inline double ray_tri(Vec3 o, Vec3 d, Vec3 a, Vec3 b, Vec3 c) {
        constexpr double eps = 1e-9;
        Vec3 e1 = sub(b, a), e2 = sub(c, a);
        Vec3 h = cross(d, e2);
        double det = dot(e1, h);
        if(std::abs(det) < eps) return -1.0;
        double inv = 1.0 / det;
        Vec3 s = sub(o, a);
        double u = dot(s, h) * inv;
        if(u < 0.0 || u > 1.0) return -1.0;
        Vec3 q = cross(s, e1);
        double v = dot(d, q) * inv;
        if(v < 0.0 || u + v > 1.0) return -1.0;
        double t = dot(e2, q) * inv;
        return (t > eps) ? t : -1.0;
    }

    inline verify::Fn check_raycast() {
        return [](const TestData& in, const TestData& out)
                -> std::pair<bool, std::string> {
            TestData vars = in.read_object("vars");
            TestData g = vars.read_object("data");
            auto verts = g.read_array<double>("verts");
            auto num_tris = g.read_value<std::int64_t>("num_tris");
            auto idx = g.read_array<std::int64_t>("idx");
            auto num_rays = g.read_value<std::int64_t>("num_rays");
            auto ro = g.read_array<double>("ray_origin");
            auto rd = g.read_array<double>("ray_dir");
            auto hits = out.read_array<std::int64_t>("hits");
            if(static_cast<std::int64_t>(hits.size()) != num_rays)
                return {false, "size mismatch"};
            for(std::int64_t r = 0; r < num_rays; ++r) {
                Vec3 o = {ro[r*3], ro[r*3+1], ro[r*3+2]};
                Vec3 d = {rd[r*3], rd[r*3+1], rd[r*3+2]};
                double best_t = std::numeric_limits<double>::infinity();
                std::int64_t best_i = -1;
                for(std::int64_t i = 0; i < num_tris; ++i) {
                    std::int64_t ia = idx[i*3], ib = idx[i*3+1], ic = idx[i*3+2];
                    Vec3 a = {verts[ia*3], verts[ia*3+1], verts[ia*3+2]};
                    Vec3 b = {verts[ib*3], verts[ib*3+1], verts[ib*3+2]};
                    Vec3 c = {verts[ic*3], verts[ic*3+1], verts[ic*3+2]};
                    double t = ray_tri(o, d, a, b, c);
                    if(t > 0 && t < best_t) { best_t = t; best_i = i; }
                }
                if(hits[r] != best_i)
                    return {false, "ray " + std::to_string(r)
                        + " expected " + std::to_string(best_i)
                        + " got " + std::to_string(hits[r])};
            }
            return {true, std::string{}};
        };
    }

    inline verify::Fn has_size(std::int64_t expected) {
        return [expected](const TestData&, const TestData& out)
                -> std::pair<bool, std::string> {
            auto a = out.read_array<std::int64_t>("hits");
            if(static_cast<std::int64_t>(a.size()) != expected)
                return {false, "size " + std::to_string(a.size())
                    + " != " + std::to_string(expected)};
            return {true, std::string{}};
        };
    }

} // namespace raycast_common
