// Student API for rangeSearch. Matches pbbs/benchmarks/rangeSearch
// HCNNG variant: given a corpus of high-dimensional feature vectors and
// a separate set of query vectors, return for each query the corpus IDs
// inside a radius `rad` in Euclidean distance. The answer is approximate
// because HCNNG is a graph-based ANN structure - tests check recall vs
// brute-force ground truth, not exact equality.
//
// Output uses pbbs's flat layout (parallels rangeQueryKDTree):
//   out[0]        = q             (number of queries)
//   out[1..q+1]   = counts[i]     (in-range result count for query i)
//   out[q+1..]    = ids flat      (corpus ids ordered by query)
// Total size = 1 + q + sum(counts).

#pragma once

#include <cstdint>
#include <vector>

namespace student {

    std::vector<long long> hcnng_range_search(
        const std::vector<float>& corpus,    // n * dim, row-major
        const std::vector<float>& queries,   // q * dim, row-major
        std::int64_t dim,
        double rad);

} // namespace student
