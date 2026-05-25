// Vendored from pbbsbench/benchmarks/histogram/parallel/histogram.C
// (MIT licensed, (c) Guy Blelloch and the PBBS team).
// Wrapped in a unique namespace to avoid ODR collisions if other variants
// are added later.
#pragma once

#include <parlay/primitives.h>

namespace pbbs_parallel_histogram {

// `uint` is not a C++ standard type; pbbsbench picks it up from <sys/types.h>
// on Linux. We define it locally so the verbatim function body below compiles
// on Windows/MinGW too.
using uint = unsigned int;

parlay::sequence<uint> histogram(parlay::sequence<uint> const &In, uint buckets) {
  return parlay::histogram_by_index(In, buckets);
}

} // namespace pbbs_parallel_histogram
