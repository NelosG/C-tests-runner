// Student API for delaunayTriangulation. Vendors pbbs's
// incrementalDelaunay (random incremental insertion with octree point
// location and speculative parallel batch insertion).
//
// Parlay-native, 3-phase like pbbs's delaunay bench: the point sequence is
// built OUTSIDE the timed region (build_delaunay), only delaunay() is timed
// (DelaunayContext::run), triangle indices extracted afterwards (result()).
// warmupIterations is kept at 0 because pbbs's delaunay() mutates its point
// sequence in place (single timed pass, like pbbs's -r 1).

#pragma once

#include <cstdint>
#include <memory>

#include <parlay/sequence.h>

namespace student {

    struct DelaunayContext {
        virtual ~DelaunayContext() = default;
        virtual void run() = 0;
        // flat triangle list, 3 vertex indices per triangle.
        virtual parlay::sequence<std::int64_t> result() const = 0;
    };

    std::unique_ptr<DelaunayContext> build_delaunay(
        const parlay::sequence<double>& xs,
        const parlay::sequence<double>& ys);

} // namespace student
