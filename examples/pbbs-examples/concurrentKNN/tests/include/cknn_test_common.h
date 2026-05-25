// Same shape as nn_test_common: single point set, k-NN of each point.
#pragma once

#include <algorithm>
#include <cstdint>
#include <dataGen.h>
#include <limits>
#include <string>
#include <test_builder.h>
#include <test_data.h>
#include <utility>
#include <vector>

namespace cknn_common {

    inline setup::Fn random_input(std::string algo, std::int64_t k,
                                  std::int64_t dim,
                                  std::int64_t n,
                                  std::uint64_t seed = 0) {
        return [algo = std::move(algo), k, dim, n, seed](TestData& td) {
            std::vector<double> points(n * dim);
            for(std::int64_t i = 0; i < n * dim; ++i)
                points[i] = dataGen::hash<double>(seed * 2 + i);
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                v.write_value<std::int64_t>("k", k);
                v.write_value<std::int64_t>("dim", dim);
                v.write_array<double>("points", points);
            });
        };
    }

    inline double sqdist(const double* a, const double* b, std::int64_t dim) {
        double s = 0;
        for(std::int64_t d = 0; d < dim; ++d) { double t = a[d]-b[d]; s += t*t; }
        return s;
    }

    inline verify::Fn check_knn() {
        return [](const TestData& in, const TestData& out)
                -> std::pair<bool, std::string> {
            TestData vars = in.read_object("vars");
            auto k = vars.read_value<std::int64_t>("k");
            auto dim = vars.read_value<std::int64_t>("dim");
            auto points = vars.read_array<double>("points");
            auto result = out.read_array<std::int64_t>("knn");
            std::int64_t n = points.size() / dim;
            for(std::int64_t q = 0; q < n; ++q) {
                std::vector<double> dists(n);
                for(std::int64_t i = 0; i < n; ++i) {
                    dists[i] = (i == q) ? std::numeric_limits<double>::infinity()
                        : sqdist(points.data()+i*dim, points.data()+q*dim, dim);
                }
                std::vector<double> sorted_d = dists;
                std::partial_sort(sorted_d.begin(), sorted_d.begin()+k, sorted_d.end());
                double k_th = sorted_d[k-1];
                double tol = 1e-9 * std::max(1.0, k_th);
                for(std::int64_t j = 0; j < k; ++j) {
                    std::int64_t idx = result[q*k + j];
                    if(idx < 0 || idx >= n)
                        return {false, "q=" + std::to_string(q) + " idx oob"};
                    if(idx == q)
                        return {false, "q=" + std::to_string(q) + " returned self"};
                    double d = sqdist(points.data()+idx*dim,
                                      points.data()+q*dim, dim);
                    if(d > k_th + tol)
                        return {false, "q=" + std::to_string(q)
                            + " dist " + std::to_string(d)
                            + " > k_th " + std::to_string(k_th)};
                }
            }
            return {true, std::string{}};
        };
    }

    inline verify::Fn has_size(std::int64_t expected) {
        return [expected](const TestData&, const TestData& out)
                -> std::pair<bool, std::string> {
            auto a = out.read_array<std::int64_t>("knn");
            if(static_cast<std::int64_t>(a.size()) != expected)
                return {false, "size " + std::to_string(a.size())};
            return {true, std::string{}};
        };
    }

} // namespace cknn_common
