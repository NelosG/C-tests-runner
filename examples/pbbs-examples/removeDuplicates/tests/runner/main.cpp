// The input sequence is materialised outside RUNNER_EXECUTE so only the
// dedup body is timed (matches pbbs's time_loop).

#include <cstdint>
#include <stdexcept>
#include <string>

#include <parlay/sequence.h>

#include <dedup.h>
#include <runner.h>
#include <runner_parlay.h>

RUNNER_MAIN {
    TestData vars = runner::read_object("vars");
    std::string algo = vars.read_string("algo");
    auto in = runner::read_parlay_sequence<std::uint32_t>(vars, "in");
    parlay::sequence<std::uint32_t> result;

    RUNNER_EXECUTE {
        if(algo == "parlayhash_dedup") {
            result = student::parlayhash_dedup(in);
        } else {
            throw std::runtime_error("removeDuplicates runner: unknown algo '" + algo + "'");
        }
    };

    runner::write_parlay_sequence<std::uint32_t>("result", result);
}
