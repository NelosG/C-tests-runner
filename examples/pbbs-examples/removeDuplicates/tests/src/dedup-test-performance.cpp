#include <dedup_test_common.h>

namespace {

    using dedup_common::random_input;
    using dedup_common::has_max_size;

    // Single large scenario. 100M ints, 50M unique -> parlayhash
    // fills heavily. threads=1 baseline ~25-40 s. RAM ~800 MB.
    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"perf_200M", random_input(algo, 200'000'000, 100'000'000), has_max_size(100'000'000)}
        };
    }

} // namespace

class ParlayhashDedupPerf final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("parlayhash_dedup");
        }
        std::string name() const override {
            return "Performance.ParlayhashDedup";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::PERFORMANCE;
        }
};
REGISTER_TEST(ParlayhashDedupPerf)
