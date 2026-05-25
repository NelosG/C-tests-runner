// Student API for ANN. One pbbsbench parlay variant: HCNNG
// (Hierarchical Clustering-based Nearest Neighbor Graph) + beam search.
//
// HCNNG is approximate, so verify uses a recall threshold.
#pragma once

#include <cstdint>
#include <vector>

namespace student {

    // points: row-major (n * dim) floats.
    // Returns row-major (n * k) ints: row i is k approximate-nearest
    // indices of point i (excluding i itself).
    std::vector<std::int64_t> hcnng_ann(
        std::int64_t k,
        std::int64_t dim,
        const std::vector<float>& points);

} // namespace student
