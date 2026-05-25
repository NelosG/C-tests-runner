// Vendored from pbbsbench/benchmarks/removeDuplicates/parlayhash/dedup.h
// (MIT licensed, (c) Guy Blelloch and the PBBS team).
#pragma once

#include <parlay/primitives.h>

namespace pbbs_parlayhash_dedup {

template <class T>
parlay::sequence<T> dedup(parlay::sequence<T> const &A) {
  return parlay::remove_duplicates(A);
}

} // namespace pbbs_parlayhash_dedup
