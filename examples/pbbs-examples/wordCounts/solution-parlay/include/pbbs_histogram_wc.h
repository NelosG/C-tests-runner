// Vendored from pbbsbench/benchmarks/wordCounts/histogram/wc.C
// (MIT licensed, (c) Guy Blelloch and the PBBS team).
#pragma once

#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <parlay/io.h>
#include <parlay/internal/group_by.h>
#include <cstddef>
#include <utility>

namespace pbbs_histogram_wc {

using charseq = parlay::sequence<char>;
using result_type = std::pair<charseq, std::size_t>;

inline parlay::sequence<result_type> wordCounts(charseq const &s) {
  // blank out all non alpha characters, and convert upper to lowercase
  auto str = parlay::map(s, [] (char c) -> char {
    if (c >= 65 && c < 91) return c + 32;   // upper to lower
    else if (c >= 97 && c < 123) return c;  // already lower
    else return 0;});                       // all other

  // generate tokens (i.e., contiguous regions of non-zero characters)
  auto words = parlay::tokens(str, [] (char c) {return c == 0;});

  auto result = parlay::histogram_by_key(std::move(words));
  words.clear();

  return result;
}

} // namespace pbbs_histogram_wc
