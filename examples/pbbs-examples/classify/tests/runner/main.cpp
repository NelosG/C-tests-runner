#include <classify.h>
#include <runner.h>
#include <stdexcept>

RUNNER_MAIN {
    TestData vars = runner::read_object("vars");
    std::string algo = vars.read_string("algo");
    TestData d = vars.read_object("data");
    auto num_features = d.read_value<std::int64_t>("num_features");
    auto num_values = d.read_value<std::int64_t>("num_values");
    auto num_train = d.read_value<std::int64_t>("num_train");
    auto num_test = d.read_value<std::int64_t>("num_test");
    auto train = d.read_array<std::uint8_t>("train");
    auto test = d.read_array<std::uint8_t>("test");
    std::vector<std::uint8_t> result;

    RUNNER_EXECUTE {
        if(algo == "decision_tree") {
            result = student::decision_tree_classify(
                num_features, num_train, num_values, train, num_test, test);
        } else {
            throw std::runtime_error("classify runner: unknown algo '" + algo + "'");
        }
    };

    runner::write_array<std::uint8_t>("pred", result);
}
