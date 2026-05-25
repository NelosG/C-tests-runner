// Single TU - one variant for now, but keeping the convention to make
// adding more variants painless. Each pbbs_<variant>::histogram lives in
// its own namespace so they coexist without ODR.
//
// Parlay-native: input/output are parlay::sequence directly, no conversion
// inside the timed region.

#include <histogram.h>
#include <pbbs_parallel_histogram.h>

namespace student {

    parlay::sequence<std::uint32_t> parallel_histogram(
        const parlay::sequence<std::uint32_t>& in,
        std::uint32_t buckets)
    {
        return pbbs_parallel_histogram::histogram(in, buckets);
    }

} // namespace student
