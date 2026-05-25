// Vendored from pbbsbench/algorithm/union_find.h
// (MIT licensed, (c) Guy Blelloch and the PBBS team).
#pragma once

#include <atomics.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>

// The following supports both "union" that is only safe sequentially
// and "link" that is safe in parallel.  Find is always safe in parallel.
template <class vertexId>
struct unionFind {
  parlay::sequence<vertexId> parents;

  bool is_root(vertexId u) {
    return parents[u] < 0;}

  unionFind(size_t n) {
    parents = parlay::sequence<vertexId>(n, -1);}

  vertexId find(vertexId i) {
    if (is_root(i)) return i;
    vertexId p = parents[i];
    if (is_root(p)) return p;

    do {
      vertexId gp = parents[p];
      parents[i] = gp;
      i = p;
      p = gp;
    } while (!is_root(p));
    return p;
  }

  void union_roots(vertexId u, vertexId v) {
    if (parents[v] < parents[u]) std::swap(u,v);
    parents[u] += parents[v];
    parents[v] = u;
  }

  void link(vertexId u, vertexId v) {
    parents[u] = v;}

  bool tryLink(vertexId u, vertexId v) {
    return (parents[u] == -1 &&
            pbbs::atomic_compare_and_swap(&parents[u], -1, v));
  }
};
