// Student API for rangeQuery2d. Matches pbbs/benchmarks/rangeQuery2d:
// receive a single point sequence (all_x, all_y), the first 2*n_queries
// of which define query rectangles as pairs of opposite corners; the
// remainder are the data points. Return the total count summed across
// all queries.
//
// NOTE: pbbsbench's parallelPlaneSweep variant builds a persistent
// augmented map (PAM, separate ~3kLOC library). This port uses a
// simpler sort+binary-search counting approach that produces the same
// count answer in parallel, without dragging in PAM.

#pragma once

#include <cstdint>
#include <vector>

namespace student {

    std::int64_t parallel_range_count(
        const std::vector<double>& all_x,
        const std::vector<double>& all_y,
        std::int64_t n_queries);

} // namespace student
