// Vendored from pbbsbench/benchmarks/minSpanningForest/parallelKruskal/MST.C
// (MIT licensed, (c) Guy Blelloch and the PBBS team).
#pragma once

#include <graph.h>
#include <limits.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <speculative_for.h>
#include <union_find.h>

namespace pbbs_parallel_kruskal {

using vertexId = int;
using edgeId = unsigned int;
using edgeWeight = float;

struct indexedEdge {
  vertexId u; vertexId v; edgeId id; edgeWeight w;
  indexedEdge(vertexId u, vertexId v, edgeId id, edgeWeight w)
    : u(u), v(v), id(id), w(w){}
  indexedEdge() {};
};

using reservation = pbbs::reservation<edgeId>;

struct UnionFindStep {
  parlay::sequence<indexedEdge> &E;
  parlay::sequence<reservation> &R;
  unionFind<vertexId> &UF;
  parlay::sequence<bool> &inST;
  UnionFindStep(parlay::sequence<indexedEdge> &E,
                unionFind<vertexId> &UF,
                parlay::sequence<reservation> &R,
                parlay::sequence<bool> &inST) :
    E(E), R(R), UF(UF), inST(inST) {}

  bool reserve(edgeId i) {
    vertexId u = E[i].u = UF.find(E[i].u);
    vertexId v = E[i].v = UF.find(E[i].v);
    if (u != v) {
      R[v].reserve(i);
      R[u].reserve(i);
      return true;
    } else return false;
  }

  bool commit(edgeId i) {
    vertexId u = E[i].u;
    vertexId v = E[i].v;
    if (R[v].check(i)) {
      R[u].checkReset(i);
      UF.link(v, u);
      inST[E[i].id] = true;
      return true;}
    else if (R[u].check(i)) {
      UF.link(u, v);
      inST[E[i].id] = true;
      return true; }
    else return false;
  }
};

inline parlay::sequence<edgeId> mst(wghEdgeArray<vertexId,edgeWeight> &E) {
  size_t m = E.m;
  size_t n = E.n;

  auto edgeLess = [&] (indexedEdge a, indexedEdge b) {
    return (a.w < b.w) || ((a.w == b.w) && (a.id < b.id));};

  auto IW = parlay::delayed_seq<indexedEdge>(m, [&] (size_t i) {
      return indexedEdge(E[i].u, E[i].v, i, E[i].weight);});

  auto IW1 = parlay::sort(IW, edgeLess);

  parlay::sequence<bool> mstFlags(m, false);
  unionFind<vertexId> UF(n);
  parlay::sequence<reservation> R(n);
  UnionFindStep UFStep1(IW1, UF, R,  mstFlags);
  pbbs::speculative_for<vertexId>(UFStep1, 0, IW1.size(), 20, false);

  parlay::sequence<edgeId> mst_result = parlay::pack_index<edgeId>(mstFlags);
  return mst_result;
}

} // namespace pbbs_parallel_kruskal
