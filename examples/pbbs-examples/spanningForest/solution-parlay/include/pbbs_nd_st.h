// Vendored from pbbsbench/benchmarks/spanningForest/ndST/ST.C
// (MIT licensed, (c) Guy Blelloch and the PBBS team).
#pragma once

#include <atomics.h>
#include <graph.h>
#include <limits.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <union_find.h>

namespace pbbs_nd_st {

using vertexId = int;
using edgeId = unsigned int;

inline parlay::sequence<edgeId> st(edgeArray<vertexId> const &E){
  edgeId m = E.nonZeros;
  vertexId n = E.numRows;
  unionFind<vertexId> UF(n);
  parlay::sequence<edgeId> hooks(n, (edgeId) m);

  parlay::parallel_for (0, m, [&] (edgeId i) {
      vertexId u = E[i].u;
      vertexId v = E[i].v;
      while(1) {
        u = UF.find(u);
        v = UF.find(v);
        if (u == v) break;
        if (u > v) std::swap(u,v);
        if (hooks[u] == m &&
            pbbs::atomic_compare_and_swap(&hooks[u], m, i)){
          UF.link(u, v);
          break;
        }
      }
    }, 1000);

  parlay::sequence<edgeId> stIdx =  parlay::filter(hooks, [&] (size_t a) {
      return a != m;});
  return stIdx;
}

} // namespace pbbs_nd_st
