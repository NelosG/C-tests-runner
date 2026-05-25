// Shared helpers for minSpanningForest. Verifies:
//   1. Output edges form a forest (no cycles).
//   2. Forest reaches the same connectivity as the full input
//      (size = n - #components).
//   3. Total weight equals the reference Kruskal MST weight on the same
//      input (i.e., minimal).

#pragma once

#include <algorithm>
#include <cstdint>
#include <dataGen.h>
#include <numeric>
#include <set>
#include <string>
#include <test_builder.h>
#include <test_data.h>
#include <utility>
#include <vector>

namespace mst_common {

    struct WEdgeList {
        std::int64_t n = 0;
        std::vector<std::int64_t> us;
        std::vector<std::int64_t> vs;
        std::vector<float> weights;
    };

    inline WEdgeList canonicalize(
        std::int64_t n,
        std::vector<std::tuple<std::int64_t,std::int64_t,float>> edges)
    {
        std::set<std::pair<std::int64_t,std::int64_t>> seen;
        WEdgeList out;
        out.n = n;
        for(auto [u, v, w] : edges) {
            if(u == v) continue;
            if(u > v) std::swap(u, v);
            if(!seen.insert({u, v}).second) continue;
            out.us.push_back(u);
            out.vs.push_back(v);
            out.weights.push_back(w);
        }
        return out;
    }

    inline WEdgeList random_edges(std::int64_t n, std::int64_t m,
                                  std::uint64_t seed = 0) {
        std::vector<std::tuple<std::int64_t,std::int64_t,float>> raw;
        raw.reserve(static_cast<std::size_t>(m));
        for(std::int64_t i = 0; i < m; ++i) {
            std::int64_t a = static_cast<std::int64_t>(
                dataGen::hash64(static_cast<std::uint64_t>(seed + i*3)) % static_cast<std::uint64_t>(n));
            std::int64_t b = static_cast<std::int64_t>(
                dataGen::hash64(static_cast<std::uint64_t>(seed + i*3 + 1)) % static_cast<std::uint64_t>(n));
            float w = dataGen::hash<float>(static_cast<std::size_t>(seed + i*3 + 2));
            raw.push_back({a, b, w});
        }
        return canonicalize(n, std::move(raw));
    }

    inline void write_edges(TestData& v, const WEdgeList& e) {
        v.write_object("wedges", [&](TestData& ed) {
            ed.write_value<std::int64_t>("n", e.n);
            ed.write_array<std::int64_t>("us", e.us);
            ed.write_array<std::int64_t>("vs", e.vs);
            ed.write_array<float>("weights", e.weights);
        });
    }

    inline WEdgeList read_edges(const TestData& vars) {
        TestData ed = vars.read_object("wedges");
        WEdgeList e;
        e.n = ed.read_value<std::int64_t>("n");
        e.us = ed.read_array<std::int64_t>("us");
        e.vs = ed.read_array<std::int64_t>("vs");
        e.weights = ed.read_array<float>("weights");
        return e;
    }

    inline setup::Fn fixed_input(std::string algo, std::int64_t n,
                                 std::vector<std::tuple<std::int64_t,std::int64_t,float>> edges) {
        return [algo = std::move(algo), n, edges = std::move(edges)](TestData& td) {
            WEdgeList e = canonicalize(n, edges);
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                write_edges(v, e);
            });
        };
    }

    inline setup::Fn random_input(std::string algo, std::int64_t n,
                                  std::int64_t m, std::uint64_t seed = 0) {
        return [algo = std::move(algo), n, m, seed](TestData& td) {
            WEdgeList e = random_edges(n, m, seed);
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                write_edges(v, e);
            });
        };
    }

    struct DSU {
        std::vector<std::int64_t> p;
        explicit DSU(std::int64_t n) : p(n) { std::iota(p.begin(), p.end(), 0); }
        std::int64_t find(std::int64_t x) {
            while(p[x] != x) { p[x] = p[p[x]]; x = p[x]; }
            return x;
        }
        bool unite(std::int64_t a, std::int64_t b) {
            a = find(a); b = find(b);
            if(a == b) return false;
            p[a] = b;
            return true;
        }
    };

    // Reference total weight via sequential Kruskal with (weight, index) tiebreak.
    inline double reference_mst_weight(const WEdgeList& e) {
        std::vector<std::int64_t> order(e.us.size());
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(),
                  [&](std::int64_t a, std::int64_t b) {
                      if(e.weights[a] != e.weights[b]) return e.weights[a] < e.weights[b];
                      return a < b;
                  });
        DSU d(e.n);
        double total = 0.0;
        for(auto i : order) {
            if(d.unite(e.us[i], e.vs[i])) total += e.weights[i];
        }
        return total;
    }

    inline verify::Fn check_mst() {
        return [](const TestData& in, const TestData& out)
                -> std::pair<bool, std::string> {
            TestData vars = in.read_object("vars");
            WEdgeList e = read_edges(vars);
            auto idx = out.read_array<std::int64_t>("mst_edges");

            DSU d(e.n);
            double got_weight = 0.0;
            for(auto i : idx) {
                if(i < 0 || i >= static_cast<std::int64_t>(e.us.size()))
                    return {false, "edge index " + std::to_string(i) + " out of range"};
                if(!d.unite(e.us[i], e.vs[i]))
                    return {false, "edge " + std::to_string(i) + " creates cycle"};
                got_weight += e.weights[i];
            }
            // size = n - components
            DSU full(e.n);
            for(std::size_t i = 0; i < e.us.size(); ++i)
                full.unite(e.us[i], e.vs[i]);
            std::set<std::int64_t> roots;
            for(std::int64_t v = 0; v < e.n; ++v) roots.insert(full.find(v));
            std::int64_t exp = e.n - static_cast<std::int64_t>(roots.size());
            if(static_cast<std::int64_t>(idx.size()) != exp)
                return {false,
                    "edge count: expected " + std::to_string(exp)
                    + ", got " + std::to_string(idx.size())};
            double expected = reference_mst_weight(e);
            double tol = 1e-3 * std::max(1.0, std::abs(expected));
            if(std::abs(got_weight - expected) > tol)
                return {false,
                    "total weight: expected " + std::to_string(expected)
                    + ", got " + std::to_string(got_weight)};
            return {true, std::string{}};
        };
    }

    inline verify::Fn has_some_mst() {
        return [](const TestData&, const TestData& out)
                -> std::pair<bool, std::string> {
            auto a = out.read_array<std::int64_t>("mst_edges");
            (void)a;
            return {true, std::string{}};
        };
    }

} // namespace mst_common
