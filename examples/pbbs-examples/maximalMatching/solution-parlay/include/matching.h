// Student API for maximalMatching. Two variants from pbbsbench:
// nondeterministic atomic-CAS (ndMatching) and speculative incremental
// (incrementalMatching).
//
// Input: number of vertices n and an edge list (us[i], vs[i]).
// Output: list of edge indices i forming a maximal matching.

#pragma once

#include <cstdint>
#include <vector>

namespace student {

    std::vector<std::int64_t> nd_matching(
        std::int64_t n,
        const std::vector<std::int64_t>& us,
        const std::vector<std::int64_t>& vs);

    std::vector<std::int64_t> incremental_matching(
        std::int64_t n,
        const std::vector<std::int64_t>& us,
        const std::vector<std::int64_t>& vs);

} // namespace student
