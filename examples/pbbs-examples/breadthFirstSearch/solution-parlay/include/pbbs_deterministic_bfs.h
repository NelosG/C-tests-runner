// Adapted from pbbsbench/benchmarks/breadthFirstSearch/deterministicBFS/BFS.C
// (MIT licensed, (c) Guy Blelloch and the PBBS team).
//
// The verbatim pbbs version chains parlay::block_delayed::filter +
// filter_map over parlay::sequence<std::atomic<bool>>; on our toolchain
// that combination occasionally SIGSEGVs inside the delayed pipeline at
// 2-8 worker threads. We keep the algorithm (write_max for determinism)
// but materialize edges and the filter result with plain primitives,
// avoiding the delayed::filter chain entirely.
#pragma once

#include <atomic>
#include <graph.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>

namespace pbbs_deterministic_bfs {

using vertexId = int;
using edgeId = unsigned int;
using Graph = graph<vertexId, edgeId>;

inline parlay::sequence<vertexId> BFS(vertexId start, const Graph &G) {
  size_t n = G.numVertices();
  auto parent = parlay::sequence<std::atomic<vertexId>>::from_function(n, [&] (size_t) {
      return -1;});
  parlay::sequence<char> visited(n, char{0});
  parent[start] = start;
  visited[start] = 1;

  parlay::sequence<vertexId> frontier(1, start);

  while (frontier.size() > 0) {
    // Per-frontier-vertex degree, then prefix-sum into offsets in the
    // materialized edge buffer.
    size_t fs = frontier.size();
    parlay::sequence<size_t> offsets(fs + 1, 0);
    parlay::parallel_for(0, fs, [&] (size_t i) {
        offsets[i] = G[frontier[i]].degree; });
    size_t total = parlay::scan_inplace(offsets.cut(0, fs));
    offsets[fs] = total;

    // Materialize edges (u, v) for the current frontier.
    parlay::sequence<std::pair<vertexId, vertexId>> edges(total);
    parlay::parallel_for(0, fs, [&] (size_t i) {
        vertexId u = frontier[i];
        size_t base = offsets[i];
        auto vtx = G[u];
        for(vertexId j = 0; j < vtx.degree; ++j) {
            edges[base + j] = std::make_pair(u, vtx.Neighbors[j]);
        }
    });

    // Stage 1: try to claim parent via write_max (deterministic per round).
    parlay::sequence<char> keep(total, 0);
    parlay::parallel_for(0, total, [&] (size_t i) {
        auto [u, v] = edges[i];
        if(visited[v] == 0
            && parlay::write_max(&parent[v], u, std::less<vertexId>())) {
            keep[i] = 1;
        }
    });

    // Stage 2: keep edges where our claim won (parent[v] == u).
    parlay::sequence<vertexId> winners(total, vertexId{-1});
    parlay::parallel_for(0, total, [&] (size_t i) {
        if(keep[i]) {
            auto [u, v] = edges[i];
            if(parent[v].load() == u) {
                visited[v] = 1;
                winners[i] = v;
            }
        }
    });

    frontier = parlay::filter(winners, [] (vertexId v) { return v >= 0; });
  }

  return parlay::map(parent, [] (auto const &x) -> vertexId {
      return x.load();});
}

} // namespace pbbs_deterministic_bfs
