// Vendored from pbbsbench/benchmarks/comparisonSort/stableSampleSort/sort.h
// (MIT licensed, (c) Guy Blelloch and the PBBS team).
#pragma once

#include <parlay/internal/sample_sort.h>

namespace pbbs_stable_sample_sort {

constexpr bool INPLACE = false;

template <class T, class BinPred>
parlay::sequence<T> compSort(parlay::sequence<T> const &A, const BinPred& f) {
  return parlay::internal::sample_sort(parlay::make_slice(A), f, true); // true makes it stable
}

} // namespace pbbs_stable_sample_sort
