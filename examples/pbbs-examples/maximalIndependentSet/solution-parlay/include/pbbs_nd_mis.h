// Vendored from pbbsbench/benchmarks/maximalIndependentSet/ndMIS/MIS.C
// (MIT licensed, (c) Guy Blelloch and the PBBS team).
#pragma once

#include <atomics.h>
#include <graph.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>

namespace pbbs_nd_mis {

using vertexId = unsigned int;
using edgeId = unsigned int;
using Graph = graph<vertexId, edgeId>;

inline parlay::sequence<char> maximalIndependentSet(const Graph &G) {
  size_t n = G.n;
  parlay::sequence<char> Flags(n, (char) 0);
  parlay::sequence<bool> V(n, false);

  parlay::parallel_for(0, n, [&] (size_t i) {
      size_t v = i;
      while (1) {
        //drop out if already in or out of MIS
        if (Flags[v]) break;
        //try to lock self and neighbors
        if (pbbs::atomic_compare_and_swap<bool>(&V[v], false, true)) {
          size_t k = 0;
          for (size_t j = 0; j < G[v].degree; j++){
            vertexId ngh = G[v].Neighbors[j];
            // if ngh is not in MIS or we successfully
            // acquire lock, increment k
            if (Flags[ngh] == 2 || pbbs::atomic_compare_and_swap(&V[ngh], false, true))
              k++;
            else break;
          }
          if(k == G[v].degree){
            //win on self and neighbors so fill flags
            Flags[v] = 1;
            for(size_t j = 0; j < G[v].degree; j++){
              vertexId ngh = G[v].Neighbors[j];
              if(Flags[ngh] != 2) Flags[ngh] = 2;
            }
          } else {
            //lose so reset V values up to point
            //where it lost
            V[v] = false;
            for(size_t j = 0; j < k; j++){
              vertexId ngh = G[v].Neighbors[j];
              if(Flags[ngh] != 2) V[ngh] = false;
            }
          }
        }
      }
    });
  return Flags;
}

} // namespace pbbs_nd_mis
