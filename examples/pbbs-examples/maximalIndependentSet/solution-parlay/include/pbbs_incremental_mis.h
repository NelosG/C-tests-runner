// Vendored from pbbsbench/benchmarks/maximalIndependentSet/incrementalMIS/MIS.C
// (MIT licensed, (c) Guy Blelloch and the PBBS team).
#pragma once

#include <graph.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <speculative_for.h>

namespace pbbs_incremental_mis {

using vertexId = unsigned int;
using edgeId = unsigned int;
using Graph = graph<vertexId, edgeId>;

// For each vertex:
//   Flags = 0 indicates undecided
//   Flags = 1 indicates chosen
//   Flags = 2 indicates a neighbor is chosen
struct MISstep {
  char flag;
  parlay::sequence<char> &Flags;
  Graph const &G;
  MISstep(parlay::sequence<char> & F, const Graph &G) : Flags(F), G(G) {}

  bool reserve(size_t i) {
    size_t d = G[i].degree;
    flag = 1;
    for (size_t j = 0; j < d; j++) {
      size_t ngh = G[i].Neighbors[j];
      if (ngh < i) {
        if (Flags[ngh] == 1) { flag = 2; return 1;}
        // need to wait for higher priority neighbor to decide
        else if (Flags[ngh] == 0) flag = 0;
      }
    }
    return 1;
  }

  bool commit(size_t i) { return (Flags[i] = flag) > 0;}
};

inline parlay::sequence<char> maximalIndependentSet(Graph const &GS) {
  size_t n = GS.n;
  parlay::sequence<char> Flags(n, (char) 0);
  MISstep mis(Flags, GS);
  pbbs::speculative_for<vertexId>(mis, 0, n, 20);
  return Flags;
}

} // namespace pbbs_incremental_mis
