// Vendored from pbbsbench/benchmarks/comparisonSort/parlaySampleSort/sort.h
// (MIT licensed, (c) Guy Blelloch and the PBBS team).
// Wrapped in a unique namespace so multiple sort variants can coexist in
// the same project without ODR conflicts on `compSort`.
#pragma once

#include <parlay/internal/sample_sort.h>

namespace pbbs_parlay_sample_sort {

constexpr bool INPLACE = false;

template <class T, class BinPred>
parlay::sequence<T> compSort(parlay::sequence<T> const &A, const BinPred& f) {
  return parlay::internal::sample_sort(parlay::make_slice(A), f);
}

} // namespace pbbs_parlay_sample_sort
