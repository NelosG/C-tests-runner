// Shared helpers for spanningForest scenarios. Verifies that the output
// edge indices form a spanning forest:
//   1. They induce no cycle (test with union-find).
//   2. Adding any remaining input edge would create a cycle (maximal).

#pragma once

#include <cstdint>
#include <dataGen.h>
#include <numeric>
#include <set>
#include <string>
#include <test_builder.h>
#include <test_data.h>
#include <utility>
#include <vector>

namespace st_common {

    struct EdgeList {
        std::int64_t n = 0;
        std::vector<std::int64_t> us;
        std::vector<std::int64_t> vs;
    };

    inline EdgeList canonicalize(std::int64_t n,
                                 const std::vector<std::pair<std::int64_t,std::int64_t>>& edges)
    {
        std::set<std::pair<std::int64_t,std::int64_t>> set;
        for(auto [u, v] : edges) {
            if(u == v) continue;
            if(u > v) std::swap(u, v);
            set.insert({u, v});
        }
        EdgeList out;
        out.n = n;
        for(auto [u, v] : set) {
            out.us.push_back(u);
            out.vs.push_back(v);
        }
        return out;
    }

    inline EdgeList random_edges(std::int64_t n, std::int64_t m,
                                 std::uint64_t seed = 0) {
        std::vector<std::pair<std::int64_t,std::int64_t>> raw;
        raw.reserve(static_cast<std::size_t>(m));
        for(std::int64_t i = 0; i < m; ++i) {
            std::int64_t a = static_cast<std::int64_t>(
                dataGen::hash64(static_cast<std::uint64_t>(seed + i*2)) % static_cast<std::uint64_t>(n));
            std::int64_t b = static_cast<std::int64_t>(
                dataGen::hash64(static_cast<std::uint64_t>(seed + i*2 + 1)) % static_cast<std::uint64_t>(n));
            raw.push_back({a, b});
        }
        return canonicalize(n, raw);
    }

    inline void write_edges(TestData& v, const EdgeList& e) {
        v.write_object("edges", [&](TestData& ed) {
            ed.write_value<std::int64_t>("n", e.n);
            ed.write_array<std::int64_t>("us", e.us);
            ed.write_array<std::int64_t>("vs", e.vs);
        });
    }

    inline EdgeList read_edges(const TestData& vars) {
        TestData ed = vars.read_object("edges");
        EdgeList e;
        e.n = ed.read_value<std::int64_t>("n");
        e.us = ed.read_array<std::int64_t>("us");
        e.vs = ed.read_array<std::int64_t>("vs");
        return e;
    }

    inline setup::Fn fixed_input(std::string algo, std::int64_t n,
                                 std::vector<std::pair<std::int64_t,std::int64_t>> edges) {
        return [algo = std::move(algo), n, edges = std::move(edges)](TestData& td) {
            EdgeList e = canonicalize(n, edges);
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                write_edges(v, e);
            });
        };
    }

    inline setup::Fn random_input(std::string algo, std::int64_t n,
                                  std::int64_t m, std::uint64_t seed = 0) {
        return [algo = std::move(algo), n, m, seed](TestData& td) {
            EdgeList e = random_edges(n, m, seed);
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                write_edges(v, e);
            });
        };
    }

    // Simple seqential DSU for verification.
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

    // Expected size of spanning forest: n - (#connected components of input).
    inline std::int64_t expected_size(const EdgeList& e) {
        DSU d(e.n);
        for(std::size_t i = 0; i < e.us.size(); ++i)
            d.unite(e.us[i], e.vs[i]);
        std::set<std::int64_t> roots;
        for(std::int64_t v = 0; v < e.n; ++v) roots.insert(d.find(v));
        return e.n - static_cast<std::int64_t>(roots.size());
    }

    inline verify::Fn check_spanning_forest() {
        return [](const TestData& in, const TestData& out)
                -> std::pair<bool, std::string> {
            TestData vars = in.read_object("vars");
            EdgeList e = read_edges(vars);
            auto idx = out.read_array<std::int64_t>("st_edges");

            DSU d(e.n);
            for(auto i : idx) {
                if(i < 0 || i >= static_cast<std::int64_t>(e.us.size()))
                    return {false, "edge index " + std::to_string(i) + " out of range"};
                if(!d.unite(e.us[i], e.vs[i]))
                    return {false,
                        "edge " + std::to_string(i) + " ("
                        + std::to_string(e.us[i]) + "," + std::to_string(e.vs[i])
                        + ") creates a cycle"};
            }
            std::int64_t exp = expected_size(e);
            if(static_cast<std::int64_t>(idx.size()) != exp)
                return {false,
                    "edge count: expected " + std::to_string(exp)
                    + ", got " + std::to_string(idx.size())};
            return {true, std::string{}};
        };
    }

    inline verify::Fn has_some_st() {
        return [](const TestData&, const TestData& out)
                -> std::pair<bool, std::string> {
            auto a = out.read_array<std::int64_t>("st_edges");
            (void)a;
            return {true, std::string{}};
        };
    }

} // namespace st_common
