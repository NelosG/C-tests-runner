// The point sequence is built outside RUNNER_EXECUTE (matches pbbs's hull
// bench, which builds points before time_loop and times only hull(pts)).

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

#include <parlay/sequence.h>

#include <hull.h>
#include <runner.h>
#include <runner_parlay.h>

RUNNER_MAIN {
    TestData vars = runner::read_object("vars");
    std::string algo = vars.read_string("algo");
    TestData pd = vars.read_object("points");
    auto xs = runner::read_parlay_sequence<double>(pd, "xs");
    auto ys = runner::read_parlay_sequence<double>(pd, "ys");

    if(algo != "parallel_quick_hull") {
        throw std::runtime_error("convexHull runner: unknown algo '" + algo + "'");
    }

    auto ctx = student::build_hull(xs, ys);

    RUNNER_EXECUTE {
        ctx->run();
    };

    auto result = ctx->result();
    runner::write_parlay_sequence<std::int64_t>("hull", result);
}
