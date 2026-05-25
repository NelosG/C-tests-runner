// Vendored from pbbsbench/benchmarks/spanningForest/incrementalST/ST.C
// (MIT licensed, (c) Guy Blelloch and the PBBS team).
#pragma once

#include <graph.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <speculative_for.h>
#include <union_find.h>

namespace pbbs_incremental_st {

using vertexId = int;
using edgeId = unsigned int;
using reservation = pbbs::reservation<edgeId>;

struct unionFindStep {
  vertexId u;  vertexId v;
  edgeArray<vertexId> const &E;
  parlay::sequence<reservation> &R;
  unionFind<vertexId> &UF;
  unionFindStep(edgeArray<vertexId> const &E,
                unionFind<vertexId> &UF,
                parlay::sequence<reservation> &R)
    : E(E), R(R), UF(UF) {}

  bool reserve(edgeId i) {
    u = UF.find(E[i].u);
    v = UF.find(E[i].v);
    if (u > v) std::swap(u,v);
    if (u != v) {
      R[v].reserve(i);
      return 1;
    } else return 0;
  }

  bool commit(edgeId i) {
    if (R[v].check(i)) { UF.link(v, u); return 1; }
    else return 0;
  }
};

inline parlay::sequence<edgeId> st(edgeArray<vertexId> const &G){
  size_t m = G.nonZeros;
  size_t n = G.numRows;
  unionFind<vertexId> UF(n);
  parlay::sequence<reservation> R(n);
  unionFindStep UFStep(G, UF, R);
  pbbs::speculative_for<edgeId>(UFStep, 0, m, 100);
  return parlay::internal::filter_map(R,
                  [&] (const reservation& a) -> bool {return a.reserved();},
                  [&] (const reservation& a) -> edgeId {return a.get();});
}

} // namespace pbbs_incremental_st
