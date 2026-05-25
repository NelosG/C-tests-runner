#include <runner.h>
#include <st.h>
#include <stdexcept>

RUNNER_MAIN {
    TestData vars = runner::read_object("vars");
    std::string algo = vars.read_string("algo");
    TestData ed = vars.read_object("edges");
    auto n = ed.read_value<std::int64_t>("n");
    auto us = ed.read_array<std::int64_t>("us");
    auto vs = ed.read_array<std::int64_t>("vs");
    std::vector<std::int64_t> result;

    RUNNER_EXECUTE {
        if(algo == "nd_st") {
            result = student::nd_st(n, us, vs);
        } else if(algo == "incremental_st") {
            result = student::incremental_st(n, us, vs);
        } else {
            throw std::runtime_error("spanningForest runner: unknown algo '" + algo + "'");
        }
    };

    runner::write_array<std::int64_t>("st_edges", result);
}
