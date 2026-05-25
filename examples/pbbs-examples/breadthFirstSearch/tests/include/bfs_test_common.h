// Shared helpers for BFS scenarios. Verifies the parent sequence:
//   1. parent[start] == start, parent[v] == -1 iff v unreachable.
//   2. For each reached v != start, parent[v] is one of v's neighbors,
//      and BFS-distance via parents = sequential BFS reference distance.

#pragma once

#include <cstdint>
#include <dataGen.h>
#include <queue>
#include <set>
#include <string>
#include <test_builder.h>
#include <test_data.h>
#include <utility>
#include <vector>

namespace bfs_common {

    struct CSR {
        std::int64_t n = 0;
        std::vector<std::int64_t> offsets;
        std::vector<std::int64_t> neighbors;
    };

    inline CSR build_csr_undirected(
        std::int64_t n,
        const std::vector<std::pair<std::int64_t,std::int64_t>>& edges)
    {
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
        g.offsets.assign(n + 1, 0);
        for(std::int64_t v = 0; v < n; ++v)
            g.offsets[v + 1] = g.offsets[v]
                + static_cast<std::int64_t>(adj[v].size());
        g.neighbors.reserve(static_cast<std::size_t>(g.offsets[n]));
        for(std::int64_t v = 0; v < n; ++v)
            for(auto u : adj[v]) g.neighbors.push_back(u);
        return g;
    }

    inline CSR random_graph(std::int64_t n, std::int64_t m,
                            std::uint64_t seed = 0) {
        std::vector<std::pair<std::int64_t,std::int64_t>> edges;
        edges.reserve(static_cast<std::size_t>(m));
        for(std::int64_t i = 0; i < m; ++i) {
            std::int64_t a = static_cast<std::int64_t>(
                dataGen::hash64(static_cast<std::uint64_t>(seed + i*2)) % static_cast<std::uint64_t>(n));
            std::int64_t b = static_cast<std::int64_t>(
                dataGen::hash64(static_cast<std::uint64_t>(seed + i*2 + 1)) % static_cast<std::uint64_t>(n));
            edges.push_back({a, b});
        }
        return build_csr_undirected(n, edges);
    }

    inline void write_graph(TestData& v, const CSR& g, std::int64_t start) {
        v.write_value<std::int64_t>("start", start);
        v.write_object("graph", [&](TestData& gd) {
            gd.write_value<std::int64_t>("n", g.n);
            gd.write_array<std::int64_t>("offsets", g.offsets);
            gd.write_array<std::int64_t>("neighbors", g.neighbors);
        });
    }

    inline std::pair<CSR, std::int64_t> read_input(const TestData& vars) {
        auto start = vars.read_value<std::int64_t>("start");
        TestData gd = vars.read_object("graph");
        CSR g;
        g.n = gd.read_value<std::int64_t>("n");
        g.offsets = gd.read_array<std::int64_t>("offsets");
        g.neighbors = gd.read_array<std::int64_t>("neighbors");
        return {g, start};
    }

    inline setup::Fn fixed_input(std::string algo, std::int64_t start,
                                 std::int64_t n,
                                 std::vector<std::pair<std::int64_t,std::int64_t>> edges) {
        return [algo = std::move(algo), start, n,
                edges = std::move(edges)](TestData& td) {
            CSR g = build_csr_undirected(n, edges);
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                write_graph(v, g, start);
            });
        };
    }

    inline setup::Fn random_input(std::string algo, std::int64_t start,
                                  std::int64_t n, std::int64_t m,
                                  std::uint64_t seed = 0) {
        return [algo = std::move(algo), start, n, m, seed](TestData& td) {
            CSR g = random_graph(n, m, seed);
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                write_graph(v, g, start);
            });
        };
    }

    // Sequential reference BFS to compute distances.
    inline std::vector<std::int64_t> reference_dist(const CSR& g,
                                                    std::int64_t start) {
        std::vector<std::int64_t> d(g.n, -1);
        d[start] = 0;
        std::queue<std::int64_t> q;
        q.push(start);
        while(!q.empty()) {
            std::int64_t v = q.front(); q.pop();
            for(std::int64_t j = g.offsets[v]; j < g.offsets[v + 1]; ++j) {
                std::int64_t u = g.neighbors[j];
                if(d[u] < 0) {
                    d[u] = d[v] + 1;
                    q.push(u);
                }
            }
        }
        return d;
    }

    inline verify::Fn check_bfs() {
        return [](const TestData& in, const TestData& out)
                -> std::pair<bool, std::string> {
            TestData vars = in.read_object("vars");
            auto [g, start] = read_input(vars);
            auto parent = out.read_array<std::int64_t>("parent");
            if(static_cast<std::int64_t>(parent.size()) != g.n)
                return {false, "parent size " + std::to_string(parent.size())
                    + " != n " + std::to_string(g.n)};
            if(parent[start] != start)
                return {false, "parent[start] != start"};

            auto ref = reference_dist(g, start);
            // For each vertex: reachable iff parent[v] >= 0 (treating start as reached).
            for(std::int64_t v = 0; v < g.n; ++v) {
                bool got_reached = parent[v] >= 0;
                bool ref_reached = ref[v] >= 0;
                if(got_reached != ref_reached)
                    return {false,
                        "vertex " + std::to_string(v) + " reach mismatch: got="
                        + std::to_string(got_reached) + " ref=" + std::to_string(ref_reached)};
            }
            // Walk parent[] from each reached vertex back to start; depth must
            // equal reference distance.
            std::vector<std::int64_t> dist(g.n, -1);
            dist[start] = 0;
            for(std::int64_t v = 0; v < g.n; ++v) {
                if(v == start || parent[v] < 0) continue;
                // Use known parent to compute dist (might need recursion).
                if(dist[v] >= 0) continue;
                std::vector<std::int64_t> chain;
                std::int64_t cur = v;
                while(cur != start && dist[cur] < 0) {
                    chain.push_back(cur);
                    std::int64_t p = parent[cur];
                    if(p < 0 || p >= g.n)
                        return {false, "parent[" + std::to_string(cur)
                            + "] = " + std::to_string(p) + " out of range"};
                    cur = p;
                }
                std::int64_t base = (cur == start) ? 0 : dist[cur];
                if(base < 0)
                    return {false, "unreachable chain from " + std::to_string(v)};
                for(std::int64_t i = static_cast<std::int64_t>(chain.size()) - 1; i >= 0; --i)
                    dist[chain[i]] = ++base;
            }
            for(std::int64_t v = 0; v < g.n; ++v) {
                if(ref[v] != dist[v])
                    return {false,
                        "distance for v=" + std::to_string(v)
                        + " ref=" + std::to_string(ref[v])
                        + " got=" + std::to_string(dist[v])};
            }
            // Sanity: parent[v] must be an actual neighbor of v.
            for(std::int64_t v = 0; v < g.n; ++v) {
                if(v == start || parent[v] < 0) continue;
                bool found = false;
                for(std::int64_t j = g.offsets[v]; j < g.offsets[v + 1]; ++j)
                    if(g.neighbors[j] == parent[v]) { found = true; break; }
                if(!found)
                    return {false, "parent[" + std::to_string(v) + "]="
                        + std::to_string(parent[v]) + " is not a neighbor"};
            }
            return {true, std::string{}};
        };
    }

    inline verify::Fn has_size(std::int64_t expected_n) {
        return [expected_n](const TestData&, const TestData& out)
                -> std::pair<bool, std::string> {
            auto a = out.read_array<std::int64_t>("parent");
            if(static_cast<std::int64_t>(a.size()) != expected_n)
                return {false,
                    "size " + std::to_string(a.size())
                    + " != " + std::to_string(expected_n)};
            return {true, std::string{}};
        };
    }

} // namespace bfs_common
