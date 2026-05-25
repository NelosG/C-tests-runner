// The particle array is built outside RUNNER_EXECUTE (matches pbbs's nBody
// bench, which builds particles before time_loop and times only stepBH).

#include <memory>
#include <stdexcept>
#include <string>

#include <parlay/sequence.h>

#include <nbody.h>
#include <runner.h>
#include <runner_parlay.h>

RUNNER_MAIN {
    TestData vars = runner::read_object("vars");
    std::string algo = vars.read_string("algo");
    TestData g = vars.read_object("data");
    auto pos = runner::read_parlay_sequence<double>(g, "pos");
    auto mass = runner::read_parlay_sequence<double>(g, "mass");

    if(algo != "all_pairs_nbody") {
        throw std::runtime_error("nBody runner: unknown algo '" + algo + "'");
    }

    auto ctx = student::build_nbody(pos, mass);

    RUNNER_EXECUTE {
        ctx->run();
    };

    auto result = ctx->result();
    runner::write_parlay_sequence<double>("forces", result);
}
