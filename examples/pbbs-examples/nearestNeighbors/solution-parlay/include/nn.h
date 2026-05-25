// Student API for nearestNeighbors. One pbbs parlay variant: octTree.
//
// Parlay-native, 3-phase to mirror pbbs's neighborsTime.C exactly: the
// vertex array (vv) and pointer array (v) are built OUTSIDE the timed
// region (octtree_knn_build), only ANN is timed (KnnContext::run), and the
// neighbor indices are extracted OUTSIDE the timed region
// (KnnContext::result). pbbs builds vv/v before time_loop and times only
// ANN(v,k); this matches that.

#pragma once

#include <cstdint>
#include <memory>

#include <parlay/sequence.h>

namespace student {

    // Opaque handle owning the prebuilt vertex array. run() executes the
    // (timed) octTree kNN; result() extracts the row-major (n*k) indices.
    struct KnnContext {
        virtual ~KnnContext() = default;
        virtual void run() = 0;
        virtual parlay::sequence<std::int64_t> result() const = 0;
    };

    // points: row-major (n * dim) doubles. Builds the vertex array (untimed).
    std::unique_ptr<KnnContext> octtree_knn_build(
        std::int64_t k,
        std::int64_t dim,
        const parlay::sequence<double>& points);

} // namespace student
