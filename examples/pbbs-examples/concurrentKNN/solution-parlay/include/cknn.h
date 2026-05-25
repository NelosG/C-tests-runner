// Student API for concurrentKNN. Uses pbbsbench's octTree k-NN.
//
// NOTE: pbbsbench's true `concurrentKNN/octTree` variant builds on
// flock/verlib (multi-version concurrent memory management with
// epoch-based reclamation and versioned pointers). Those are separate
// libraries which by project policy we do not bundle into the example
// tree. We instead use the same plain `nearestNeighbors/octTree`
// vendored verbatim from pbbs; it is still parallel octree k-NN and is
// the closest pbbs variant we can ship self-contained.
#pragma once

#include <cstdint>
#include <vector>

namespace student {

    std::vector<std::int64_t> octtree_knn(
        std::int64_t k,
        std::int64_t dim,
        const std::vector<double>& points);

} // namespace student
