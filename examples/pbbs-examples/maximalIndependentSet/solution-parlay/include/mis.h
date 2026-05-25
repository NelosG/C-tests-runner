// Student API for maximalIndependentSet. Two parallel variants:
// nondeterministic-greedy (ndMIS) and speculative incremental (incrementalMIS).
//
// Parlay-native: the graph (CSR in native 32-bit width) is built by the
// runner OUTSIDE the timed region and passed by const reference, mirroring
// pbbs's bench which reads the graph from file before time_loop. The
// expensive int64 -> uint32 narrowing of the edge array no longer happens
// inside RUNNER_EXECUTE. Output is a per-vertex flag sequence: 1 = in MIS.

#pragma once

#include <graph.h>
#include <parlay/sequence.h>

namespace student {

    // Nondeterministic greedy MIS - ndMIS variant.
    parlay::sequence<char> nd_mis(const graph<unsigned int, unsigned int>& G);

    // Speculative incremental MIS - incrementalMIS variant.
    parlay::sequence<char> incremental_mis(const graph<unsigned int, unsigned int>& G);

} // namespace student
