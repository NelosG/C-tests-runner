#include <wc_test_common.h>

namespace {

    using wc_common::random_input;
    using wc_common::has_some_words;

    // Single large scenario. 100M words is ~500 MB raw plus the hash
    // table; threads=1 baseline ~25-40 s.
    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"perf_70M", random_input(algo, 70'000'000), has_some_words()}
        };
    }

} // namespace

class HistogramWordCountsPerf final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("histogram_word_counts");
        }
        std::string name() const override {
            return "Performance.HistogramWordCounts";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::PERFORMANCE;
        }
};
REGISTER_TEST(HistogramWordCountsPerf)
