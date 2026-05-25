// Dispatcher for the histogram assignment. Input is a `vars` object
// carrying the dispatch key + payload; runner picks the matching student
// function and writes back the bucket counts.
//
// The input sequence is materialised outside RUNNER_EXECUTE so only the
// histogram body is timed (matches pbbs's time_loop).

#include <cstdint>
#include <stdexcept>
#include <string>

#include <parlay/sequence.h>

#include <histogram.h>
#include <runner.h>
#include <runner_parlay.h>

RUNNER_MAIN {
    TestData vars = runner::read_object("vars");
    std::string algo = vars.read_string("algo");
    auto in = runner::read_parlay_sequence<std::uint32_t>(vars, "in");
    auto buckets = vars.read_value<std::uint32_t>("buckets");
    parlay::sequence<std::uint32_t> result;

    RUNNER_EXECUTE {
        if(algo == "parallel_histogram") {
            result = student::parallel_histogram(in, buckets);
        } else {
            throw std::runtime_error("histogram runner: unknown algo '" + algo + "'");
        }
    };

    runner::write_parlay_sequence<std::uint32_t>("result", result);
}
