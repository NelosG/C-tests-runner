// Vendored from pbbsbench/benchmarks/maximalMatching/incrementalMatching/matching.C
// (MIT licensed, (c) Guy Blelloch and the PBBS team).
#pragma once

#include <graph.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <speculative_for.h>

namespace pbbs_incremental_matching {

using vertexId = unsigned int;
using edgeId = unsigned int;
using edges = edgeArray<vertexId>;
using reservation = pbbs::reservation<edgeId>;

struct matchStep {
  edges const &E;
  parlay::sequence<reservation> &R;
  parlay::sequence<bool> &matched;

  matchStep(edges const &E,
            parlay::sequence<reservation> &R,
            parlay::sequence<bool> &matched)
    : E(E), R(R), matched(matched) {}

  bool reserve(edgeId i) {
    size_t u = E[i].u;
    size_t v = E[i].v;
    if (matched[u] || matched[v] || (u == v)) return 0;
    R[u].reserve(i);
    R[v].reserve(i);
    return 1;
  }

  bool commit(edgeId i) {
    size_t u = E[i].u;
    size_t v = E[i].v;
    if (R[v].check(i)) {
      R[v].reset();
      if (R[u].check(i)) {
        matched[u] = matched[v] = 1;
        return 1;
      }
    } else if (R[u].check(i)) R[u].reset();
    return 0;
  }
};

inline parlay::sequence<edgeId> maximalMatching(edges const &E) {
  size_t n = std::max(E.numCols, E.numRows);
  size_t m = E.nonZeros;

  parlay::sequence<reservation> R(n);
  parlay::sequence<bool> matched(n, false);
  matchStep mStep(E, R, matched);
  pbbs::speculative_for<edgeId>(mStep, 0, m, 10, 0);
  parlay::sequence<edgeId> matchingIdx =
    parlay::pack(parlay::delayed_seq<edgeId>(n, [&] (size_t i) {return R[i].get();}),
                 parlay::tabulate(n, [&] (size_t i) -> bool {return R[i].reserved();}));
  return matchingIdx;
}

} // namespace pbbs_incremental_matching
