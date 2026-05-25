#include <range.h>
#include <runner.h>
#include <stdexcept>

RUNNER_MAIN {
    TestData vars = runner::read_object("vars");
    std::string algo = vars.read_string("algo");
    TestData g = vars.read_object("data");
    auto all_x = g.read_array<double>("all_x");
    auto all_y = g.read_array<double>("all_y");
    auto n_q = g.read_value<std::int64_t>("n_queries");
    std::int64_t total = 0;

    RUNNER_EXECUTE {
        if(algo == "parallel_range_count") {
            total = student::parallel_range_count(all_x, all_y, n_q);
        } else {
            throw std::runtime_error("rangeQuery2d runner: unknown algo '" + algo + "'");
        }
    };

    runner::write_value<std::int64_t>("total", total);
}
