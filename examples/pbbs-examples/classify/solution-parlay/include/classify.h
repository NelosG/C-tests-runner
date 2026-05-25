// Student API for classify. One variant: pbbsbench's decisionTree
// (~C4.5, all-features discrete in our tests).
//
// Train: row-major matrix of train_rows x num_features bytes, with
// column 0 being the label. num_values per feature (max 256). All
// features treated as discrete here.
// Test: row-major matrix of test_rows x num_features bytes (col 0
// unused on input but expected to be predicted).
// Output: predicted label for each test row.

#pragma once

#include <cstdint>
#include <vector>

namespace student {

    std::vector<std::uint8_t> decision_tree_classify(
        std::int64_t num_features,
        std::int64_t num_train_rows,
        std::int64_t num_values,
        const std::vector<std::uint8_t>& train,
        std::int64_t num_test_rows,
        const std::vector<std::uint8_t>& test);

} // namespace student
