// rangeQueryKDTree correctness. Matches pbbs/benchmarks/
// rangeQueryKDTree bench: each input point is a query; the algorithm
// returns, for every point, the ids of all OTHER input points within
// Euclidean distance `rad`. Output uses pbbs's flat layout (see rqkd.h
// for the format). Verify samples a few random points and brute-force
// checks that all points within radius are reported.

#include <random>
#include <set>
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

    // pbbs's checker samples r random points and verifies that every
    // brute-force result for those points is contained in the reported
    // set. We do the same.
    verify::Fn brute_force_sample(int r = 20, std::uint64_t check_seed = 9001) {
        return [r, check_seed](const TestData& in, const TestData& out)
                -> std::pair<bool, std::string> {
            auto xs = in.read_array<double>("xs");
            auto ys = in.read_array<double>("ys");
            auto rad = in.read_value<double>("rad");
            auto flat = out.read_array<long long>("neighbors");
            if(flat.empty()) return {false, "empty result"};
            long long n = flat[0];
            if(n != (long long)xs.size())
                return {false, "n header " + std::to_string(n)
                    + " != input size " + std::to_string(xs.size())};
            if((long long)flat.size() < 1 + n)
                return {false, "missing per-point counts"};

            std::vector<long long> counts(n);
            for(long long i = 0; i < n; ++i) counts[i] = flat[1 + i];

            std::vector<long long> off(n + 1);
            off[0] = 0;
            for(long long i = 0; i < n; ++i) off[i + 1] = off[i] + counts[i];
            long long ids_start = 1 + n;
            if((long long)flat.size() != ids_start + off[n])
                return {false, "flat size " + std::to_string(flat.size())
                    + " != header " + std::to_string(ids_start + off[n])};

            double rad2 = rad * rad;
            std::mt19937_64 g(check_seed);
            for(int t = 0; t < r; ++t) {
                long long q = (long long)(g() % (std::uint64_t)n);
                std::set<long long> reported;
                for(long long k = 0; k < counts[q]; ++k)
                    reported.insert(flat[ids_start + off[q] + k]);
                for(long long j = 0; j < n; ++j) {
                    if(j == q) continue;
                    double dx = xs[j] - xs[q], dy = ys[j] - ys[q];
                    if(dx * dx + dy * dy <= rad2) {
                        if(!reported.count(j))
                            return {false, "q=" + std::to_string(q)
                                + " missing j=" + std::to_string(j)};
                    }
                }
            }
            return {true, {}};
        };
    }

} // namespace

class RQKDBasic final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return {
                {"random_100_r2",  random_pts(100,   2.0), brute_force_sample(20)},
                {"random_1k_r2",   random_pts(1000,  2.0), brute_force_sample(20)},
                {"random_10k_r1",  random_pts(10000, 1.0), brute_force_sample(20)}
            };
        }
        std::string name() const override { return "Correctness.Basic"; }
        ScenarioType scenario_type() const override { return ScenarioType::CORRECTNESS; }
};

REGISTER_TEST(RQKDBasic)
