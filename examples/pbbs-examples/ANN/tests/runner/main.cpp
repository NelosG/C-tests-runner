#include <ann.h>
#include <runner.h>
#include <stdexcept>

RUNNER_MAIN {
    TestData vars = runner::read_object("vars");
    std::string algo = vars.read_string("algo");
    auto k = vars.read_value<std::int64_t>("k");
    auto dim = vars.read_value<std::int64_t>("dim");
    auto points = vars.read_array<float>("points");
    std::vector<std::int64_t> result;

    RUNNER_EXECUTE {
        if(algo == "hcnng_ann") {
            result = student::hcnng_ann(k, dim, points);
        } else {
            throw std::runtime_error("ANN runner: unknown algo '" + algo + "'");
        }
    };

    runner::write_array<std::int64_t>("ann", result);
}
