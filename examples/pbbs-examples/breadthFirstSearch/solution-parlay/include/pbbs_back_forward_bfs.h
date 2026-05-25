// Vendored from pbbsbench/benchmarks/breadthFirstSearch/backForwardBFS/BFS.C
// (MIT licensed, (c) Guy Blelloch and the PBBS team).
#pragma once

#include <atomic>
#include <graph.h>
#include <ligraLight.h>
#include <limits>
#include <parlay/parallel.h>
#include <parlay/primitives.h>

namespace pbbs_back_forward_bfs {

using vertexId = int;
using edgeId = unsigned int;
using Graph = graph<vertexId, edgeId>;

inline parlay::sequence<vertexId> BFS(vertexId start, const Graph &G) {
  size_t n = G.numVertices();
  auto parent = parlay::sequence<std::atomic<vertexId>>::from_function(n, [&] (size_t i) {
      return -1;});
  parent[start] = start;

  auto edge_fa = [iparent=parent.begin()] (vertexId u, vertexId v) -> bool {
    vertexId expected = -1;
    return iparent[v].compare_exchange_strong(expected, u);
  };
  auto cond_f = [iparent=parent.begin()] (vertexId v) { return iparent[v] == -1;};
  auto frontier_map = ligra::edge_map(G, edge_fa, cond_f, false, false);

  auto frontier = ligra::vertex_subset<vertexId>(start);

  while (frontier.size() > 0) {
    frontier = frontier_map(frontier);
  }
  return parlay::map(parent, [] (auto const &x) -> vertexId {
      return x.load();});
}

} // namespace pbbs_back_forward_bfs
