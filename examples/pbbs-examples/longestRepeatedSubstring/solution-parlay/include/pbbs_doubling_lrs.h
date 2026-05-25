// Adapted from pbbsbench/benchmarks/longestRepeatedSubstring/doubling/lrs.C
// (MIT licensed, (c) Guy Blelloch and the PBBS team).
// Identical algorithm to pbbs: the doubling suffix array (vendored from
// pbbsbench/algorithm/suffix_array.h as pbbs_doubling_sa) + LCP + max.
#pragma once

#include <lcp.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <pbbs_doubling_sa.h>
#include <tuple>

namespace pbbs_doubling_lrs {

using charseq = parlay::sequence<unsigned char>;
using result_type = std::tuple<size_t, size_t, size_t>;

inline result_type lrs(charseq const &s) {
  // pbbs's lrs.C: SA (doubling variant), then LCP, then max element.
  auto sa = pbbs_doubling_sa::suffix_array<unsigned int>(s);
  auto lcps = lcp(s, sa);
  size_t idx = parlay::max_element(lcps) - lcps.begin();
  return result_type(lcps[idx], sa[idx], sa[idx + 1]);
}

} // namespace pbbs_doubling_lrs
