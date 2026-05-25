// The point sequence is built outside RUNNER_EXECUTE (matches pbbs's
// delaunay bench, which builds points before time_loop and times only
// delaunay()). warmupIterations is 0 because delaunay() mutates its input.

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

#include <parlay/sequence.h>

#include <delaunay.h>
#include <runner.h>
#include <runner_parlay.h>

RUNNER_MAIN {
    TestData vars = runner::read_object("vars");
    std::string algo = vars.read_string("algo");
    TestData pd = vars.read_object("points");
    auto xs = runner::read_parlay_sequence<double>(pd, "xs");
    auto ys = runner::read_parlay_sequence<double>(pd, "ys");

    if(algo != "bowyer_watson") {
        throw std::runtime_error("delaunay runner: unknown algo '" + algo + "'");
    }

    auto ctx = student::build_delaunay(xs, ys);

    RUNNER_EXECUTE {
        ctx->run();
    };

    auto result = ctx->result();
    runner::write_parlay_sequence<std::int64_t>("triangles", result);
}
