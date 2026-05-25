// Dispatcher for integerSort.
//
// The runner materialises the parlay::sequence for the input *outside*
// RUNNER_EXECUTE so the timed region matches pbbs's `time_loop` body:
// only `B = int_sort(A, bits)` is timed. Writing the result back to TLV
// happens after RUNNER_EXECUTE closes.

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

#include <parlay/sequence.h>

#include <isort.h>
#include <runner.h>
#include <runner_parlay.h>

RUNNER_MAIN {
    TestData vars = runner::read_object("vars");
    std::string algo = vars.read_string("algo");
    auto in = runner::read_parlay_sequence<std::uint32_t>(vars, "in");
    auto bits = vars.read_value<std::size_t>("bits");
    parlay::sequence<std::uint32_t> result;

    RUNNER_EXECUTE {
        if(algo == "parallel_radix_sort") {
            result = student::parallel_radix_sort(in, bits);
        } else {
            throw std::runtime_error("integerSort runner: unknown algo '" + algo + "'");
        }
    };

    runner::write_parlay_sequence<std::uint32_t>("result", result);
}
