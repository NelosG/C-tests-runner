// rangeQueryKDTree performance scenario. Matches pbbs bench input shape
// (one point set, every point is a query, Euclidean rad).

#include <random>
#include <test_builder.h>

namespace {

    setup::Fn random_pts(std::size_t n, double rad,
                         std::uint64_t seed = 42) {
        return [n, rad, seed](TestData& in) {
            std::mt19937_64 gen(seed);
            std::uniform_real_distribution<double> dist(-10.0, 10.0);
            std::vector<double> xs(n), ys(n);
            for(std::size_t i = 0; i < n; ++i) { xs[i] = dist(gen); ys[i] = dist(gen); }
            in.write_array<double>("xs", xs);
            in.write_array<double>("ys", ys);
            in.write_value<double>("rad", rad);
        };
    }

    verify::Fn nonempty_header() {
        return [](const TestData& in, const TestData& out)
                -> std::pair<bool, std::string> {
            auto xs = in.read_array<double>("xs");
            auto flat = out.read_array<long long>("neighbors");
            if(flat.empty() || flat[0] != (long long)xs.size())
                return {false, "bad header"};
            return {true, {}};
        };
    }

} // namespace

class RQKDPerf final : public TestScenarioExtension {
    public:
        // Single large scenario. Every input point is itself a query;
        // 2M points with a moderate radius gives threads=1 baseline
        // around 25-40 s. Smaller radius keeps per-query result lists
        // bounded so memory stays under 1 GB.
        std::vector<Test> get_tests() const override {
            return {
                {"perf_2M_r0.05", random_pts(2'000'000, 0.05), nonempty_header()}
            };
        }
        std::string name() const override { return "Performance.Basic"; }
        ScenarioType scenario_type() const override { return ScenarioType::PERFORMANCE; }
};

REGISTER_TEST(RQKDPerf)
