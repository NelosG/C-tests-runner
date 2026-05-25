// Vendored from pbbsbench/benchmarks/integerSort/parallelRadixSort/isort.h
// (MIT licensed, (c) Guy Blelloch and the PBBS team).
// Wrapped in a unique namespace so it can coexist with other variants.
#pragma once

#include <parlay/primitives.h>
#include <parlay/internal/integer_sort.h>
#include <utility>

namespace pbbs_parallel_radix_sort {

template <class T>
auto int_sort(parlay::slice<T*,T*> In, size_t bits) {
  auto f = [&] (T x) {return x;};
  return parlay::internal::integer_sort(parlay::make_slice(In), f, bits);
}

template <class E, class F>
auto int_sort(parlay::slice<std::pair<E,F>*, std::pair<E,F>*> In, size_t bits) {
  auto f = [&] (std::pair<E,F> x) {return x.first;};
  return parlay::internal::integer_sort(parlay::make_slice(In), f, bits);
}

} // namespace pbbs_parallel_radix_sort
