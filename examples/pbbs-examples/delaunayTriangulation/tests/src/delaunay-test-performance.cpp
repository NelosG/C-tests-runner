#include <delaunay_test_common.h>

namespace {

    using delaunay_common::random_input;
    using delaunay_common::has_some_tris;

    // Single large scenario. pbbs incrementalDelaunay scales to
    // millions of points; 500k gives a threads=1 baseline near 25-40 s
    // and stays well within 8 GB (vertex + triangle arena).
    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"perf_2M", random_input(algo, 2'000'000), has_some_tris()}
        };
    }

} // namespace

class BowyerWatsonPerf final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("bowyer_watson");
        }
        std::string name() const override {
            return "Performance.BowyerWatson";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::PERFORMANCE;
        }
};
REGISTER_TEST(BowyerWatsonPerf)
