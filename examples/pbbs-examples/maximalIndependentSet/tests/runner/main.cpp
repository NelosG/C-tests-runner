// Dispatcher for maximalIndependentSet.
//
// The CSR graph is narrowed to native 32-bit width and constructed OUTSIDE
// RUNNER_EXECUTE (mirrors pbbs reading the graph from file before
// time_loop). Only the MIS body is timed.

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

#include <graph.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include <mis.h>
#include <runner.h>
#include <runner_parlay.h>

RUNNER_MAIN {
    TestData vars = runner::read_object("vars");
    std::string algo = vars.read_string("algo");
    TestData gd = vars.read_object("graph");
    auto n = gd.read_value<std::int64_t>("n");
    auto offsets = runner::read_parlay_sequence_narrow<unsigned int, std::int64_t>(gd, "offsets");
    auto neighbors = runner::read_parlay_sequence_narrow<unsigned int, std::int64_t>(gd, "neighbors");

    graph<unsigned int, unsigned int> G(
        std::move(offsets), std::move(neighbors),
        static_cast<std::size_t>(n));

    parlay::sequence<char> flags;

    RUNNER_EXECUTE {
        if(algo == "nd_mis") {
            flags = student::nd_mis(G);
        } else if(algo == "incremental_mis") {
            flags = student::incremental_mis(G);
        } else {
            throw std::runtime_error("MIS runner: unknown algo '" + algo + "'");
        }
    };

    // Map pbbs's char flags (1 = in MIS) to the uint8 output. Outside the
    // timed region; n is small next to the edge array anyway.
    auto out = parlay::tabulate(flags.size(), [&](std::size_t i) -> std::uint8_t {
        return (flags[i] == 1) ? std::uint8_t{1} : std::uint8_t{0};
    });
    runner::write_parlay_sequence<std::uint8_t>("in_mis", out);
}
