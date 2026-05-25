// Student API for spanningForest. Two parallel variants from pbbsbench:
// nondeterministic CAS-based ST (ndST) and speculative incremental ST.
//
// Input: n vertices + edge list. Output: indices of edges forming a
// spanning forest.

#pragma once

#include <cstdint>
#include <vector>

namespace student {

    std::vector<std::int64_t> nd_st(
        std::int64_t n,
        const std::vector<std::int64_t>& us,
        const std::vector<std::int64_t>& vs);

    std::vector<std::int64_t> incremental_st(
        std::int64_t n,
        const std::vector<std::int64_t>& us,
        const std::vector<std::int64_t>& vs);

} // namespace student
