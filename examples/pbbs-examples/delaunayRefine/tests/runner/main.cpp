#include <refine.h>
#include <runner.h>
#include <stdexcept>

RUNNER_MAIN {
    TestData vars = runner::read_object("vars");
    std::string algo = vars.read_string("algo");
    TestData d = vars.read_object("data");
    auto xs = d.read_array<double>("xs");
    auto ys = d.read_array<double>("ys");
    auto initial = d.read_array<std::int64_t>("initial_tris");
    auto min_angle = d.read_value<double>("min_angle_deg");
    student::Refined result;

    RUNNER_EXECUTE {
        if(algo == "incremental_refine") {
            result = student::incremental_refine(xs, ys, initial, min_angle);
        } else {
            throw std::runtime_error("delaunayRefine runner: unknown algo '" + algo + "'");
        }
    };

    runner::write_array<double>("xs", result.xs);
    runner::write_array<double>("ys", result.ys);
    runner::write_array<std::int64_t>("triangles", result.triangles);
}
