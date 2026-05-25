// Dispatcher for breadthFirstSearch.
//
// The CSR graph is narrowed to native width (vertexId=int, edgeId=unsigned)
// and constructed OUTSIDE RUNNER_EXECUTE (mirrors pbbs reading the graph
// before time_loop). Only the BFS body is timed.

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

#include <graph.h>
#include <parlay/sequence.h>

#include <bfs.h>
#include <runner.h>
#include <runner_parlay.h>

RUNNER_MAIN {
    TestData vars = runner::read_object("vars");
    std::string algo = vars.read_string("algo");
    auto start = vars.read_value<std::int64_t>("start");
    TestData gd = vars.read_object("graph");
    auto n = gd.read_value<std::int64_t>("n");
    auto offsets = runner::read_parlay_sequence_narrow<unsigned int, std::int64_t>(gd, "offsets");
    auto neighbors = runner::read_parlay_sequence_narrow<int, std::int64_t>(gd, "neighbors");

    graph<int, unsigned int> G(
        std::move(offsets), std::move(neighbors),
        static_cast<std::size_t>(n));

    parlay::sequence<int> result;
    int start_v = static_cast<int>(start);

    RUNNER_EXECUTE {
        if(algo == "simple_bfs") {
            result = student::simple_bfs(start_v, G);
        } else if(algo == "deterministic_bfs") {
            result = student::deterministic_bfs(start_v, G);
        } else if(algo == "back_forward_bfs") {
            result = student::back_forward_bfs(start_v, G);
        } else {
            throw std::runtime_error("BFS runner: unknown algo '" + algo + "'");
        }
    };

    runner::write_parlay_sequence_as<std::int64_t, int>("parent", result);
}
