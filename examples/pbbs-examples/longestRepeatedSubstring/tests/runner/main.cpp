// The character sequence is materialised outside RUNNER_EXECUTE so only the
// LRS body (suffix array + LCP) is timed.

#include <cstdint>
#include <stdexcept>
#include <string>
#include <tuple>

#include <parlay/sequence.h>

#include <lrs.h>
#include <runner.h>
#include <runner_parlay.h>

RUNNER_MAIN {
    TestData vars = runner::read_object("vars");
    std::string algo = vars.read_string("algo");
    auto s = runner::read_parlay_chars<unsigned char>(vars, "s");
    std::int64_t length = 0, pos1 = 0, pos2 = 0;

    RUNNER_EXECUTE {
        if(algo == "doubling_lrs") {
            auto t = student::doubling_lrs(s);
            length = std::get<0>(t);
            pos1   = std::get<1>(t);
            pos2   = std::get<2>(t);
        } else {
            throw std::runtime_error("LRS runner: unknown algo '" + algo + "'");
        }
    };

    runner::write_value<std::int64_t>("length", length);
    runner::write_value<std::int64_t>("pos1", pos1);
    runner::write_value<std::int64_t>("pos2", pos2);
}
