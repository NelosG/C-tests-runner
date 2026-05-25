// Vendored from pbbsbench/benchmarks/comparisonSort/mergeSort/sort.h
// (MIT licensed, (c) Guy Blelloch and the PBBS team).
#pragma once

#include <parlay/internal/merge_sort.h>

namespace pbbs_merge_sort {

constexpr bool INPLACE = true;

template <class T, class BinPred>
void compSort(parlay::sequence<T> &A, const BinPred& f) {
  parlay::internal::merge_sort_inplace(parlay::make_slice(A), f);
}

} // namespace pbbs_merge_sort
