#include <rs.h>
#include <runner.h>

RUNNER_MAIN {
    auto corpus = runner::read_array<float>("corpus");
    auto queries = runner::read_array<float>("queries");
    auto dim = runner::read_value<std::int64_t>("dim");
    auto rad = runner::read_value<double>("rad");
    std::vector<long long> out;
    RUNNER_EXECUTE {
        out = student::hcnng_range_search(corpus, queries, dim, rad);
    };
    runner::write_array<long long>("neighbors", out);
}
