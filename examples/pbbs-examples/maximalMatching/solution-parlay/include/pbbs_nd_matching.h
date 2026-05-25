// Vendored from pbbsbench/benchmarks/maximalMatching/ndMatching/matching.C
// (MIT licensed, (c) Guy Blelloch and the PBBS team).
#pragma once

#include <atomic>
#include <graph.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>

namespace pbbs_nd_matching {

using vertexId = unsigned int;
using edgeId = unsigned int;
using edges = edgeArray<vertexId>;

inline parlay::sequence<edgeId> maximalMatching(edges const &E) {
  size_t n = std::max(E.numCols, E.numRows);
  size_t m = E.nonZeros;
  enum state : char {Init, Locked, Taken};
  auto matched = parlay::tabulate<std::atomic<state>>(n, [] (int) {return Init;});
  parlay::sequence<bool> result(m, false);
  parlay::parallel_for(0, m, [&] (int i) {
        long u = E[i].u;
        long v = E[i].v;
        if (u == v) return;
        while (true) {
          state old = Init;
          if (matched[u].load() == Taken || matched[v].load() == Taken) {
            result[i] = false;
            return;
          } else if (matched[u].compare_exchange_strong(old, Locked)) {
            if (matched[v].compare_exchange_strong(old, Taken)) {
              matched[u] = Taken;
              result[i] = true;
              return;
            } else matched[u] = Init;
          }
        }});
  auto matchingIdx = parlay::pack(parlay::delayed::tabulate(m, [&] (unsigned int i) {return i;}), result);
  return matchingIdx;
}

} // namespace pbbs_nd_matching
