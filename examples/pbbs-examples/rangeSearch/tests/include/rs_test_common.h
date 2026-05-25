// Shared helpers for rangeSearch scenarios. Matches pbbs's HCNNG
// variant input shape: high-dim float feature vectors (corpus +
// queries), Euclidean radius. Output is pbbs's flat per-query layout:
//   out[0]      = q
//   out[1..q+1] = counts[i]
//   out[q+1..]  = ids ordered by query.
// Verification samples queries and checks recall vs brute-force ground
// truth (HCNNG is approximate; perfect recall is not expected).

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <set>
#include <string>
#include <test_builder.h>
#include <test_data.h>
#include <utility>
#include <vector>

namespace rs_common {

    inline setup::Fn random_input(std::size_t n, std::size_t q,
                                  std::int64_t dim, double rad,
                                  std::uint64_t seed = 42) {
        return [n, q, dim, rad, seed](TestData& in) {
            std::mt19937_64 gen(seed);
            std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
            std::vector<float> corpus(n * dim), queries(q * dim);
            for(auto& x : corpus) x = dist(gen);
            for(auto& x : queries) x = dist(gen);
            in.write_array<float>("corpus", corpus);
            in.write_array<float>("queries", queries);
            in.write_value<std::int64_t>("dim", dim);
            in.write_value<double>("rad", rad);
        };
    }

    inline double sqdist(const float* a, const float* b, std::int64_t d) {
        double s = 0;
        for(std::int64_t i = 0; i < d; ++i) {
            double t = (double)a[i] - (double)b[i];
            s += t * t;
        }
        return s;
    }

    // HCNNG range search is approximate; we check that at least
    // `recall_floor` of the brute-force in-range points were returned,
    // averaged over a sample of queries.
    inline verify::Fn recall_at_least(double recall_floor,
                                      int sample_queries = 16,
                                      std::uint64_t check_seed = 9001) {
        return [recall_floor, sample_queries, check_seed]
                (const TestData& in, const TestData& out)
                -> std::pair<bool, std::string> {
            auto corpus = in.read_array<float>("corpus");
            auto queries = in.read_array<float>("queries");
            auto dim = in.read_value<std::int64_t>("dim");
            auto rad = in.read_value<double>("rad");
            auto flat = out.read_array<long long>("neighbors");
            if(flat.empty()) return {false, "empty result"};
            long long q = flat[0];
            long long expected_q = (long long)queries.size() / dim;
            if(q != expected_q)
                return {false, "q header " + std::to_string(q)
                    + " != " + std::to_string(expected_q)};
            if((long long)flat.size() < 1 + q)
                return {false, "missing counts"};

            std::vector<long long> counts(q);
            for(long long i = 0; i < q; ++i) counts[i] = flat[1 + i];
            std::vector<long long> off(q + 1);
            off[0] = 0;
            for(long long i = 0; i < q; ++i) off[i + 1] = off[i] + counts[i];
            long long ids_start = 1 + q;

            long long n = (long long)corpus.size() / dim;
            double r2 = rad * rad;

            std::mt19937_64 g(check_seed);
            int samples = std::min<int>(sample_queries, (int)q);
            double total_recall = 0;
            int counted = 0;
            for(int s = 0; s < samples; ++s) {
                long long qi = (long long)(g() % (std::uint64_t)q);
                std::set<long long> truth;
                for(long long j = 0; j < n; ++j) {
                    if(sqdist(corpus.data() + j * dim,
                              queries.data() + qi * dim, dim) <= r2)
                        truth.insert(j);
                }
                if(truth.empty()) continue;
                std::set<long long> reported;
                for(long long k = 0; k < counts[qi]; ++k)
                    reported.insert(flat[ids_start + off[qi] + k]);
                long long hits = 0;
                for(auto id : truth) if(reported.count(id)) ++hits;
                double r = double(hits) / double(truth.size());
                total_recall += r;
                ++counted;
            }
            if(counted == 0) {
                // every sampled query had empty truth; treat as
                // trivially OK (algorithm not exercised).
                return {true, {}};
            }
            double avg = total_recall / counted;
            if(avg < recall_floor)
                return {false, "recall " + std::to_string(avg)
                    + " < " + std::to_string(recall_floor)};
            return {true, {}};
        };
    }

    inline verify::Fn nonempty_header() {
        return [](const TestData& in, const TestData& out)
                -> std::pair<bool, std::string> {
            auto queries = in.read_array<float>("queries");
            auto dim = in.read_value<std::int64_t>("dim");
            auto flat = out.read_array<long long>("neighbors");
            long long expected_q = (long long)queries.size() / dim;
            if(flat.empty() || flat[0] != expected_q)
                return {false, "bad header"};
            return {true, {}};
        };
    }

} // namespace rs_common
