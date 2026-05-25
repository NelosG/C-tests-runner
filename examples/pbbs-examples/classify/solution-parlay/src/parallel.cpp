#include <classify.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <pbbs_decision_tree.h>

namespace student {

    std::vector<std::uint8_t> decision_tree_classify(
        std::int64_t num_features,
        std::int64_t num_train_rows,
        std::int64_t num_values,
        const std::vector<std::uint8_t>& train,
        std::int64_t num_test_rows,
        const std::vector<std::uint8_t>& test)
    {
        const std::uint8_t* train_src = train.data();
        const std::uint8_t* test_src = test.data();
        // Column-oriented features: parallel transpose of row-major train.
        // tabulate is used here because pbbs_decision_tree::feature has
        // no default constructor, so we can't pre-size the sequence and
        // assign into it.
        pbbs_decision_tree::features Train = parlay::tabulate(num_features,
            [&](std::size_t f) {
                pbbs_decision_tree::row vals = parlay::tabulate(num_train_rows,
                    [&](std::size_t r) { return train_src[r * num_features + f]; });
                return pbbs_decision_tree::feature(
                    true, static_cast<int>(num_values), std::move(vals));
            });
        // Test rows: parallel slice per row.
        pbbs_decision_tree::rows Test = parlay::tabulate(num_test_rows,
            [&](std::size_t r) {
                return parlay::tabulate(num_features,
                    [&](std::size_t f) { return test_src[r * num_features + f]; });
            });
        auto pred = pbbs_decision_tree::classify(Train, Test);
        std::vector<std::uint8_t> out(pred.size());
        std::uint8_t* dst = out.data();
        parlay::parallel_for(0, pred.size(), [&](std::size_t i) {
            dst[i] = static_cast<std::uint8_t>(pred[i]);
        });
        return out;
    }

} // namespace student
