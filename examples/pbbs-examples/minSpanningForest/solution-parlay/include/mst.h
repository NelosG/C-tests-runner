// Student API for minSpanningForest. One parallel variant: pbbsbench's
// parallelKruskal (sort-then-speculative-union-find).

#pragma once

#include <cstdint>
#include <vector>

namespace student {

    std::vector<std::int64_t> parallel_kruskal_mst(
        std::int64_t n,
        const std::vector<std::int64_t>& us,
        const std::vector<std::int64_t>& vs,
        const std::vector<float>& weights);

} // namespace student
