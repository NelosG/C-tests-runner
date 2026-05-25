// Shared helpers for maximalIndependentSet scenarios. We build random
// simple undirected graphs (no parallel edges, no self-loops) and check
// the standard MIS invariants from pbbsbench:
//   1. No two chosen vertices are adjacent (independent).
//   2. Every non-chosen vertex has a chosen neighbor (maximal).

#pragma once

#include <cstdint>
#include <dataGen.h>
#include <set>
#include <string>
#include <test_builder.h>
#include <test_data.h>
#include <utility>
#include <vector>

namespace mis_common {

    struct CSR {
        std::int64_t n = 0;
        std::vector<std::int64_t> offsets;
        std::vector<std::int64_t> neighbors;
    };

    inline CSR build_csr(std::int64_t n,
                         std::vector<std::pair<std::int64_t,std::int64_t>> edges)
    {
        // canonicalize: drop self-loops, sort+unique to enforce simple graph,
        // expand into undirected adjacency.
        std::set<std::pair<std::int64_t,std::int64_t>> set;
        for(auto [u, v] : edges) {
            if(u == v) continue;
            if(u > v) std::swap(u, v);
            set.insert({u, v});
        }
        std::vector<std::vector<std::int64_t>> adj(n);
        for(auto [u, v] : set) {
            adj[u].push_back(v);
            adj[v].push_back(u);
        }
        CSR g;
        g.n = n;
        g.offsets.resize(n + 1, 0);
        for(std::int64_t v = 0; v < n; ++v)
            g.offsets[v + 1] = g.offsets[v]
                + static_cast<std::int64_t>(adj[v].size());
        g.neighbors.reserve(static_cast<std::size_t>(g.offsets[n]));
        for(std::int64_t v = 0; v < n; ++v)
            for(auto u : adj[v]) g.neighbors.push_back(u);
        return g;
    }

    // Random Erdos-Renyi-ish graph: m random edges chosen from V*V, dedup.
    inline CSR random_graph(std::int64_t n, std::int64_t m,
                            std::uint64_t seed = 0)
    {
        std::vector<std::pair<std::int64_t,std::int64_t>> edges;
        edges.reserve(static_cast<std::size_t>(m));
        for(std::int64_t i = 0; i < m; ++i) {
            std::int64_t a = static_cast<std::int64_t>(
                dataGen::hash64(static_cast<std::uint64_t>(seed + i * 2)) % static_cast<std::uint64_t>(n));
            std::int64_t b = static_cast<std::int64_t>(
                dataGen::hash64(static_cast<std::uint64_t>(seed + i * 2 + 1)) % static_cast<std::uint64_t>(n));
            edges.push_back({a, b});
        }
        return build_csr(n, std::move(edges));
    }

    inline void write_graph(TestData& v, const CSR& g) {
        v.write_object("graph", [&](TestData& gd) {
            gd.write_value<std::int64_t>("n", g.n);
            gd.write_array<std::int64_t>("offsets", g.offsets);
            gd.write_array<std::int64_t>("neighbors", g.neighbors);
        });
    }

    inline CSR read_graph(const TestData& vars) {
        TestData gd = vars.read_object("graph");
        CSR g;
        g.n = gd.read_value<std::int64_t>("n");
        g.offsets = gd.read_array<std::int64_t>("offsets");
        g.neighbors = gd.read_array<std::int64_t>("neighbors");
        return g;
    }

    inline setup::Fn fixed_input(std::string algo, std::int64_t n,
                                 std::vector<std::pair<std::int64_t,std::int64_t>> edges) {
        return [algo = std::move(algo), n,
                edges = std::move(edges)](TestData& td) {
            CSR g = build_csr(n, edges);
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                write_graph(v, g);
            });
        };
    }

    inline setup::Fn random_input(std::string algo, std::int64_t n,
                                  std::int64_t m, std::uint64_t seed = 0) {
        return [algo = std::move(algo), n, m, seed](TestData& td) {
            CSR g = random_graph(n, m, seed);
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                write_graph(v, g);
            });
        };
    }

    inline verify::Fn check_mis() {
        return [](const TestData& in, const TestData& out)
                -> std::pair<bool, std::string> {
            TestData vars = in.read_object("vars");
            CSR g = read_graph(vars);
            auto flags = out.read_array<std::uint8_t>("in_mis");
            if(static_cast<std::int64_t>(flags.size()) != g.n)
                return {false, "flag count " + std::to_string(flags.size())
                    + " != n " + std::to_string(g.n)};

            // 1. Independent: no two adjacent chosen.
            for(std::int64_t v = 0; v < g.n; ++v) {
                if(flags[v] != 1) continue;
                for(std::int64_t j = g.offsets[v]; j < g.offsets[v + 1]; ++j) {
                    std::int64_t u = g.neighbors[j];
                    if(flags[u] == 1)
                        return {false,
                            "edge " + std::to_string(v) + "-" + std::to_string(u)
                            + " both in MIS"};
                }
            }
            // 2. Maximal: every non-chosen vertex has a chosen neighbor.
            for(std::int64_t v = 0; v < g.n; ++v) {
                if(flags[v] == 1) continue;
                bool has_chosen = false;
                for(std::int64_t j = g.offsets[v]; j < g.offsets[v + 1]; ++j) {
                    if(flags[g.neighbors[j]] == 1) { has_chosen = true; break; }
                }
                if(!has_chosen)
                    return {false,
                        "vertex " + std::to_string(v)
                        + " not in MIS and no neighbor is"};
            }
            return {true, std::string{}};
        };
    }

    inline verify::Fn has_size(std::int64_t expected_n) {
        return [expected_n](const TestData&, const TestData& out)
                -> std::pair<bool, std::string> {
            auto a = out.read_array<std::uint8_t>("in_mis");
            if(static_cast<std::int64_t>(a.size()) != expected_n)
                return {false,
                    "size: expected " + std::to_string(expected_n)
                    + ", got " + std::to_string(a.size())};
            return {true, std::string{}};
        };
    }

} // namespace mis_common
