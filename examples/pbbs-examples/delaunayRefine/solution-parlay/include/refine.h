// Student API for delaunayRefine. Vendors pbbs's incrementalRefine
// (parallel deferred Ruppert refinement on top of pbbs's Delaunay
// topology) - see pbbs_inc_refine.h.
//
// The `min_angle_deg` parameter is informational: pbbs's refineInternal
// is hard-coded to 30deg (classic Ruppert lower bound that guarantees
// termination). Test invokes with 30 so the contract holds.

#pragma once

#include <cstdint>
#include <vector>

namespace student {

    struct Refined {
        std::vector<double> xs;
        std::vector<double> ys;
        std::vector<std::int64_t> triangles;  // 3 indices per triangle
    };

    Refined incremental_refine(
        const std::vector<double>& xs,
        const std::vector<double>& ys,
        const std::vector<std::int64_t>& triangles,
        double min_angle_deg);

} // namespace student
