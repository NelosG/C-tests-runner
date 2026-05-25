// Vendored from pbbsbench/benchmarks/breadthFirstSearch/simpleBFS/BFS.C
// (MIT licensed, (c) Guy Blelloch and the PBBS team).
#pragma once

#include <atomic>
#include <graph.h>
#include <limits>
#include <parlay/internal/block_delayed.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>

namespace pbbs_simple_bfs {

namespace delayed = parlay::block_delayed;

using vertexId = int;
using edgeId = unsigned int;
using Graph = graph<vertexId, edgeId>;

inline parlay::sequence<vertexId> BFS(vertexId start, const Graph &G) {
  size_t n = G.numVertices();
  auto parent = parlay::sequence<std::atomic<vertexId>>::from_function(n, [&] (size_t i) {
      return -1;});
  parent[start] = start;
  parlay::sequence<vertexId> frontier(1,start);

  while (frontier.size() > 0) {
    auto nested_edges = parlay::map(frontier, [&] (vertexId u) {
        return parlay::delayed_tabulate(G[u].degree, [&, u] (size_t i) {
            return std::pair(u, G[u].Neighbors[i]);});});
    auto edges = delayed::flatten(nested_edges);

    auto edge_f = [&] (auto u_v) {
      vertexId expected = -1;
      auto [u, v] = u_v;
      return (parent[v] == -1) && parent[v].compare_exchange_strong(expected, u);
    };
    frontier = delayed::filter_map(edges, edge_f, [] (auto x) {return x.second;});
  }

  return parlay::map(parent, [] (auto const &x) -> vertexId {
      return x.load();});
}

} // namespace pbbs_simple_bfs
