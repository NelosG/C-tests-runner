// Vendored from pbbsbench/algorithm/lcp.h
// (MIT licensed, (c) Guy Blelloch and the PBBS team).
#pragma once

#include <parlay/sequence.h>
#include <range_min.h>

template <class Seq1, class Seq2>
auto lcp(Seq1 const &s_, Seq2 const &SA_)
  -> parlay::sequence<typename Seq2::value_type>
{
  auto s = parlay::make_slice(s_);
  auto SA = parlay::make_slice(SA_);
  using Uint = typename Seq2::value_type;
  size_t len = 111;
  size_t n = SA.size();

  auto L_ = parlay::tabulate(n-1, [&] (size_t i) -> Uint {
               size_t j = 0;
               size_t max_j = std::min(len, n - SA[i]);
               while (j < max_j && (s[SA[i]+j] == s[SA[i+1]+j])) j++;
               return (j < len) ? j : n;
            });
  auto L = parlay::make_slice(L_);

  auto remain = parlay::pack_index(parlay::map(L, [&] (Uint l) {return l == n;}));

  if (remain.size() == 0) return L_;

  parlay::sequence<Uint> ISA_(n);
  auto ISA = parlay::make_slice(ISA_);
  parlay::parallel_for(0, n, [&] (size_t i) {
                               ISA[SA[i]] = i;});

  do {
    auto rq = make_range_min(L, std::less<Uint>(), 111);

    remain = parlay::filter(remain, [&] (Uint i) {
                if (SA[i] + len >= n) {L[i] = len; return false;};
                Uint i1 = ISA[SA[i]+len];
                Uint i2 = ISA[SA[i+1]+len];
                size_t l = L[rq.query(i1, i2-1)];
                if (l < len) {L[i] = len + l; return false;}
                else return true;
             });
    len *= 2;
  } while (remain.size() > 0);

  return L_;
}
