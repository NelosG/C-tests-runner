// Student API for rangeQueryKDTree. Matches pbbs/benchmarks/
// rangeQueryKDTree bench: input is one set of 2D points; for each
// input point find all other points within Euclidean distance `rad`.
// Output is pbbs's flat result format:
//   out[0]        = n               (number of input points)
//   out[1..n+1]   = counts[i]       (number of neighbors of point i)
//   out[n+1..]    = ids flat        (all neighbour ids, ordered by i)
// Total size = 1 + n + sum(counts).

#pragma once

#include <cstdint>
#include <vector>

namespace student {

    std::vector<long long> range_neighbors(
        const std::vector<double>& xs,
        const std::vector<double>& ys,
        double rad);

} // namespace student
