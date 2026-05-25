// Dispatcher for nearestNeighbors.
//
// 3-phase to mirror pbbs's neighborsTime.C: build the vertex array outside
// RUNNER_EXECUTE, time only ANN, extract neighbor indices afterwards.

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

#include <parlay/sequence.h>

#include <nn.h>
#include <runner.h>
#include <runner_parlay.h>

RUNNER_MAIN {
    TestData vars = runner::read_object("vars");
    std::string algo = vars.read_string("algo");
    auto k = vars.read_value<std::int64_t>("k");
    auto dim = vars.read_value<std::int64_t>("dim");
    auto points = runner::read_parlay_sequence<double>(vars, "points");

    if(algo != "octtree_knn") {
        throw std::runtime_error("NN runner: unknown algo '" + algo + "'");
    }

    // Build the vertex array outside the timed region (matches pbbs).
    auto ctx = student::octtree_knn_build(k, dim, points);

    RUNNER_EXECUTE {
        ctx->run();
    };

    auto result = ctx->result();
    runner::write_parlay_sequence<std::int64_t>("knn", result);
}
