// Helpers for nBody scenarios. Verify checks force components against
// sequential reference (with relative tolerance).

#pragma once

#include <cmath>
#include <cstdint>
#include <dataGen.h>
#include <string>
#include <test_builder.h>
#include <test_data.h>
#include <utility>
#include <vector>

namespace nbody_common {

    inline setup::Fn random_input(std::string algo, std::int64_t n,
                                  std::uint64_t seed = 0) {
        return [algo = std::move(algo), n, seed](TestData& td) {
            std::vector<double> pos(n * 3);
            std::vector<double> mass(n);
            for(std::int64_t i = 0; i < n * 3; ++i)
                pos[i] = dataGen::hash<double>(seed + i);
            for(std::int64_t i = 0; i < n; ++i)
                mass[i] = 0.5 + dataGen::hash<double>(seed + n*3 + i);
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                v.write_object("data", [&](TestData& g) {
                    g.write_value<std::int64_t>("n", n);
                    g.write_array<double>("pos", pos);
                    g.write_array<double>("mass", mass);
                });
            });
        };
    }

    inline verify::Fn check_forces() {
        return [](const TestData& in, const TestData& out)
                -> std::pair<bool, std::string> {
            TestData vars = in.read_object("vars");
            TestData g = vars.read_object("data");
            auto n = g.read_value<std::int64_t>("n");
            auto pos = g.read_array<double>("pos");
            auto mass = g.read_array<double>("mass");
            auto forces = out.read_array<double>("forces");
            if(static_cast<std::int64_t>(forces.size()) != n * 3)
                return {false, "forces size mismatch"};
            // pbbs CK uses pure inverse-cube gravity (no softening); the
            // multipole approximation in stepBH is documented at ~1e-6
            // accuracy with terms=12, so we accept 1e-5 relative.
            for(std::int64_t i = 0; i < n; ++i) {
                double fx = 0, fy = 0, fz = 0;
                for(std::int64_t j = 0; j < n; ++j) {
                    if(j == i) continue;
                    double dx = pos[j*3] - pos[i*3];
                    double dy = pos[j*3+1] - pos[i*3+1];
                    double dz = pos[j*3+2] - pos[i*3+2];
                    double r2 = dx*dx + dy*dy + dz*dz;
                    double inv_r3 = 1.0 / (r2 * std::sqrt(r2));
                    double s = mass[i] * mass[j] * inv_r3;
                    fx += s * dx; fy += s * dy; fz += s * dz;
                }
                auto rel_err = [](double a, double b) {
                    double m = std::max(std::abs(a), std::abs(b));
                    if(m < 1e-12) return 0.0;
                    return std::abs(a - b) / m;
                };
                if(rel_err(forces[i*3], fx) > 1e-5
                    || rel_err(forces[i*3+1], fy) > 1e-5
                    || rel_err(forces[i*3+2], fz) > 1e-5)
                    return {false, "force mismatch at i=" + std::to_string(i)};
            }
            return {true, std::string{}};
        };
    }

    inline verify::Fn has_size(std::int64_t expected) {
        return [expected](const TestData&, const TestData& out)
                -> std::pair<bool, std::string> {
            auto a = out.read_array<double>("forces");
            if(static_cast<std::int64_t>(a.size()) != expected)
                return {false, "size " + std::to_string(a.size())
                    + " != " + std::to_string(expected)};
            return {true, std::string{}};
        };
    }

} // namespace nbody_common
