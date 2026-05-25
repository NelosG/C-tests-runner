#include <rqkd.h>
#include <runner.h>

RUNNER_MAIN {
    auto xs = runner::read_array<double>("xs");
    auto ys = runner::read_array<double>("ys");
    auto rad = runner::read_value<double>("rad");
    std::vector<long long> out;
    RUNNER_EXECUTE { out = student::range_neighbors(xs, ys, rad); };
    runner::write_array<long long>("neighbors", out);
}
