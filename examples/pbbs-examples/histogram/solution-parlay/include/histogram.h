// Student API for the histogram assignment. One parallel variant is
// required, matching pbbsbench's `parallel/histogram.C`.
//
// Parlay-native: the runner materialises the parlay::sequence outside the
// timed region (runner_parlay.h), so no std::vector <-> sequence copy
// happens inside RUNNER_EXECUTE.

#pragma once

#include <cstdint>

#include <parlay/sequence.h>

namespace student {

    // Returns a sequence of length `buckets` where output[b] is the number
    // of occurrences of value `b` in `in`. Each element of `in` must be in
    // [0, buckets). Mirrors pbbs's `parlay::histogram_by_index`.
    parlay::sequence<std::uint32_t> parallel_histogram(
        const parlay::sequence<std::uint32_t>& in,
        std::uint32_t buckets);

} // namespace student
