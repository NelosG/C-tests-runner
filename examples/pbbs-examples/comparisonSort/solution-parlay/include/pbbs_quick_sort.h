// Vendored from pbbsbench/benchmarks/comparisonSort/quickSort/sort.h
// (MIT licensed, (c) Guy Blelloch and the PBBS team).
#pragma once

#include <parlay/internal/quicksort.h>

namespace pbbs_quick_sort {

constexpr bool INPLACE = true;

template <class T, class BinPred>
void compSort(parlay::sequence<T>  &A, const BinPred& f) {
  parlay::internal::p_quicksort_inplace(parlay::make_slice(A), f);
}

} // namespace pbbs_quick_sort
