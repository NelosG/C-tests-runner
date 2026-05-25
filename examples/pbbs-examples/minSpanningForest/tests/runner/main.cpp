#include <mst.h>
#include <runner.h>
#include <stdexcept>

RUNNER_MAIN {
    TestData vars = runner::read_object("vars");
    std::string algo = vars.read_string("algo");
    TestData ed = vars.read_object("wedges");
    auto n = ed.read_value<std::int64_t>("n");
    auto us = ed.read_array<std::int64_t>("us");
    auto vs = ed.read_array<std::int64_t>("vs");
    auto weights = ed.read_array<float>("weights");
    std::vector<std::int64_t> result;

    RUNNER_EXECUTE {
        if(algo == "parallel_kruskal_mst") {
            result = student::parallel_kruskal_mst(n, us, vs, weights);
        } else {
            throw std::runtime_error("minSpanningForest runner: unknown algo '" + algo + "'");
        }
    };

    runner::write_array<std::int64_t>("mst_edges", result);
}
