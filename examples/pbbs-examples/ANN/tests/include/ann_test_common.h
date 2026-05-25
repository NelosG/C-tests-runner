// Helpers for ANN scenarios. Single point set (matches pbbs ANN bench
// shape: build the graph from v, then for each v[i] find its k-ANN
// among the others). Verify uses recall >= threshold vs brute-force
// ground truth k-NN.
#pragma once

#include <algorithm>
#include <cstdint>
#include <dataGen.h>
#include <limits>
#include <set>
#include <string>
#include <test_builder.h>
#include <test_data.h>
#include <utility>
#include <vector>

namespace ann_common {

    inline setup::Fn random_input(std::string algo, std::int64_t k,
                                  std::int64_t dim,
                                  std::int64_t n,
                                  std::uint64_t seed = 0) {
        return [algo = std::move(algo), k, dim, n, seed](TestData& td) {
            std::vector<float> points(n * dim);
            for(std::int64_t i = 0; i < n * dim; ++i)
                points[i] = (float)dataGen::hash<double>(seed * 2 + i);
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                v.write_value<std::int64_t>("k", k);
                v.write_value<std::int64_t>("dim", dim);
                v.write_array<float>("points", points);
            });
        };
    }

    inline double sqdist(const float* a, const float* b, std::int64_t dim) {
        double s = 0;
        for(std::int64_t d = 0; d < dim; ++d) { double t = a[d]-b[d]; s += t*t; }
        return s;
    }

    inline verify::Fn recall_at_least(double min_recall) {
        return [min_recall](const TestData& in, const TestData& out)
                -> std::pair<bool, std::string> {
            TestData vars = in.read_object("vars");
            auto k = vars.read_value<std::int64_t>("k");
            auto dim = vars.read_value<std::int64_t>("dim");
            auto points = vars.read_array<float>("points");
            auto result = out.read_array<std::int64_t>("ann");
            std::int64_t n = points.size() / dim;

            std::int64_t total_hits = 0;
            for(std::int64_t q = 0; q < n; ++q) {
                std::vector<std::pair<double, std::int64_t>> d(n);
                for(std::int64_t i = 0; i < n; ++i) {
                    if(i == q)
                        d[i] = {std::numeric_limits<double>::infinity(), i};
                    else
                        d[i] = {sqdist(points.data() + i*dim,
                                        points.data() + q*dim, dim), i};
                }
                std::partial_sort(d.begin(), d.begin() + k, d.end());
                std::set<std::int64_t> truth;
                for(std::int64_t j = 0; j < k; ++j) truth.insert(d[j].second);
                for(std::int64_t j = 0; j < k; ++j)
                    if(truth.count(result[q*k + j])) ++total_hits;
            }
            double recall = double(total_hits) / (n * k);
            if(recall < min_recall)
                return {false, "recall " + std::to_string(recall)
                    + " < " + std::to_string(min_recall)};
            return {true, std::string{}};
        };
    }

    inline verify::Fn has_size(std::int64_t expected) {
        return [expected](const TestData&, const TestData& out)
                -> std::pair<bool, std::string> {
            auto a = out.read_array<std::int64_t>("ann");
            if(static_cast<std::int64_t>(a.size()) != expected)
                return {false, "size " + std::to_string(a.size())};
            return {true, std::string{}};
        };
    }

} // namespace ann_common
