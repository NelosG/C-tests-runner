#include <bw_test_common.h>

namespace {

    using bw_common::random_input;
    using bw_common::has_size;

    // List ranking is pointer-chasing memory-bound; 5M chars saturates
    // L3 and shows real scalability across threads.
    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"perf_100M", random_input(algo, 100'000'000), has_size(100'000'000)}
        };
    }

} // namespace

class ListRankBWDecodePerf final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("list_rank_bw_decode");
        }
        std::string name() const override {
            return "Performance.ListRankBWDecode";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::PERFORMANCE;
        }
};
REGISTER_TEST(ListRankBWDecodePerf)
