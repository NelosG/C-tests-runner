// Vendored from pbbsbench/benchmarks/invertedIndex/parallel/index.C
// (MIT licensed, (c) Guy Blelloch and the PBBS team).
#pragma once

#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <parlay/internal/collect_reduce.h>
#include <parlay/io.h>
#include <parlay/internal/group_by.h>
#include <parlay/internal/delayed/filter.h>
#include <utility>

namespace pbbs_parallel_index {

namespace delayed = parlay::block_delayed;

using charseq = parlay::sequence<char>;

inline charseq build_index(charseq const &s, charseq const &doc_start) {
  size_t n = s.size();
  size_t m = doc_start.size();

  // sequence of indices to the start of each document
  auto starts = delayed::filter(parlay::iota(n-m+1), [&] (size_t i) {
    for (size_t j=0; j < m; j++)
      if (doc_start[j] != s[i+j]) return false;
    return true;});
  auto num_docs = starts.size();

  // generate sequence of token-doc_id pairs for each document
  auto docs = parlay::tabulate(num_docs, [&] (unsigned int doc_id) {
    size_t start = starts[doc_id] + m;
    size_t end = (doc_id==num_docs-1) ? n : starts[doc_id+1];

    // blank out all non characters, and convert to lowercase
    auto str = parlay::map(s.cut(start, end), [] (char c) -> char {
        if (c >= 65 && c < 91) return c + 32;   // upper to lower
        else if (c >= 97 && c < 123) return c;  // already lower
        else return 0;});                       // all other

    // generate tokens (i.e., contiguous regions of non-zero characters)
    auto tokens = parlay::tokens(str, [] (char c) {return c == 0;});

    // remove duplicate tokens
    tokens = parlay::remove_duplicates(std::move(tokens));

    // tag each remaining token with document id
    return parlay::map(tokens, [&] (auto str) {
        return std::pair(str, doc_id);});
  });

  auto word_doc_pairs = parlay::flatten(std::move(docs));

  // group by word, each with a sequence of docs it appears in.
  auto words = parlay::group_by_key(std::move(word_doc_pairs));

  parlay::sort_inplace(words, [] (auto const &l, auto const &r) {
                                   return l.first < r.first;});

  // generate string for each document number
  auto docstr = parlay::tabulate(num_docs, [] (size_t i) {
                     return parlay::to_chars(i);});

  // format output for each word
  charseq space(1, ' ');
  charseq newline(1, '\n');
  auto b = parlay::map(words, [&] (auto const &wd_pair) -> charseq {
     auto word = std::move(wd_pair.first);
     auto doc_ids = std::move(wd_pair.second);
     size_t len = doc_ids.size()*2 + 2;
     // each line consists of the word followed by
     // the list of documents ids separared by spaces
     // and terminated by a newline.
     auto ss = parlay::tabulate(len, [&] (size_t i) {
       if (i == 0) return word;
       if (i == len-1) return newline;
       if (i%2 == 1) return space;
       return docstr[doc_ids[i/2-1]];});
     return parlay::flatten(ss);});

  // flatten across words
  auto c = parlay::flatten(std::move(b));
  return c;
}

} // namespace pbbs_parallel_index
