// Student API for breadthFirstSearch. Three pbbsbench variants:
// simpleBFS, deterministicBFS, backForwardBFS (LigraLight-based).
//
// Parlay-native: the graph (CSR, native width) is built by the runner
// OUTSIDE the timed region and passed by const reference, mirroring pbbs's
// bench which reads the graph from file before time_loop. The int64 ->
// native narrowing of the edge array no longer happens inside RUNNER_EXECUTE.
//
// Returns parent[] sequence: parent[v] = u if v's BFS parent is u,
// -1 if not reached, start points to itself.

#pragma once

#include <graph.h>
#include <parlay/sequence.h>

namespace student {

    parlay::sequence<int> simple_bfs(
        int start, const graph<int, unsigned int>& G);

    parlay::sequence<int> deterministic_bfs(
        int start, const graph<int, unsigned int>& G);

    parlay::sequence<int> back_forward_bfs(
        int start, const graph<int, unsigned int>& G);

} // namespace student
