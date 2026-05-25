// Vendored from pbbsbench/benchmarks/suffixArray/parallelKS/SA.C
// (MIT licensed, (c) Guy Blelloch and the PBBS team).
#pragma once

#include <parlay/internal/integer_sort.h>
#include <parlay/internal/sample_sort.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <utility>

namespace pbbs_parallel_ks {

using uchar = unsigned char;
using uint = unsigned int;
using uintPair = std::pair<uint,uint>;
using longInt = unsigned __int128;

inline bool leq(uint a1, uint a2, uint b1, uint b2) {
  return(a1 < b1 || (a1 == b1 && a2 <= b2));
}

inline bool leq(uint a1, uint a2, uint a3, uint b1, uint b2, uint b3) {
  return(a1 < b1 || (a1 == b1 && leq(a2, a3, b2, b3)));
}

// This recursive version requires s[n]=s[n+1]=s[n+2] = 0.
// K is the maximum value of any element in s.
inline parlay::sequence<uint> suffixArrayRec(parlay::sequence<uint> const &s,
                                             size_t K, bool findLCPs) {
  size_t n = s.size() - 3;
  n = n+1;
  size_t n0=(n+2)/3, n1=(n+1)/3, n12=n-n0;
  auto sorted12 = parlay::sequence<uint>::uninitialized(n12);
  auto name12 = parlay::sequence<uint>::uninitialized(n12);
  auto get_first = [&] (uintPair p) {return p.first;};
  size_t bits = parlay::log2_up(K);

  if (3*bits <= 8*sizeof(uint)) {
    auto C = parlay::tabulate(n12, [&] (size_t i) -> uintPair {
      size_t j = 1+(3*i)/2;
      return std::make_pair((((uint) s[j]) << 2*bits) +
                            (((uint) s[j+1]) << bits) +
                            s[j+2],
                            j);
      });

    parlay::internal::integer_sort_inplace(parlay::make_slice(C), get_first, 3*bits);

    parlay::parallel_for (1, n12, [&] (size_t i) {
        sorted12[i] = C[i].second;
        name12[i] = (C[i].first != C[i-1].first);
      });
    name12[0] = 1;
    sorted12[0] = C[0].second;
  } else {
    auto C = parlay::tabulate(n12, [&] (size_t i) -> longInt {
        size_t j = 1+(3*i)/2;
        longInt r = ((((longInt) s[j]) << 2*bits) +
                     (((size_t) s[j+1]) << bits) +
                     s[j+2]);
        return (r << 32) + j;
      });

    parlay::internal::sample_sort_inplace(parlay::make_slice(C), std::less<longInt>());

    longInt mask = ((((longInt) 1) << 32)-1);
    parlay::parallel_for (1, n12, [&] (size_t i) {
        sorted12[i] = (uint)(C[i] & mask);
        name12[i] = (C[i] >> 32) != (C[i-1] >> 32);
      });
    name12[0] = 1;
    sorted12[0] = (uint)(C[0] & mask);
  }

  size_t num_names = parlay::scan_inclusive_inplace(name12, parlay::addm<uint>());
  parlay::sequence<uint> SA12;
  if (num_names < n12) {
    auto s12 = parlay::sequence<uint>::uninitialized(n12 + 3);
    s12[n12] = s12[n12+1] = s12[n12+2] = 0;

    parlay::parallel_for (0, n12, [&] (size_t i) {
        uint div3 = sorted12[i]/3;
        if (sorted12[i]-3*div3 == 1) s12[div3] = name12[i];
        else s12[div3+n1] = name12[i];
      });
    name12.clear();
    sorted12.clear();

    SA12 = suffixArrayRec(s12, num_names+1, findLCPs);
    s12.clear();

    parlay::parallel_for (0, n12, [&] (size_t i) {
        size_t l = SA12[i];
        SA12[i] = (l<n1) ? 3*l+1 : 3*(l-n1)+2;
      });
  } else {
    name12.clear();
    SA12 = std::move(sorted12);
  }

  auto rank = parlay::sequence<uint>::uninitialized(n+2);
  rank[n]=1; rank[n+1] = 0;
  parlay::parallel_for (0, n12, [&] (size_t i) {rank[SA12[i]] = i+2;});
  auto mod3is1 = [&] (size_t i) {return i%3 == 1;};
  parlay::sequence<uint> s0 = parlay::filter(SA12, mod3is1);
  size_t s0n = s0.size();
  auto D = parlay::sequence<uintPair>::uninitialized(n0);
  D[0] = std::make_pair(s[n-1], n-1);
  parlay::parallel_for (0, s0n, [&] (size_t i) {
      D[i+n0-s0n] = std::make_pair(s[s0[i]-1], s0[i]-1);});
  s0.clear();
  parlay::internal::integer_sort_inplace(parlay::make_slice(D), get_first, bits);
  auto SA0 = parlay::tabulate(n0, [&] (size_t i) -> uint {return D[i].second;});
  D.clear();

  auto less = [&] (size_t i, size_t j) {
    if (i%3 == 1 || j%3 == 1)
      return (std::make_pair(s[i], rank[i+1]) <
              std::make_pair(s[j], rank[j+1]));
    else
      return (std::make_tuple(s[i], s[i+1], rank[i+2]) <
              std::make_tuple(s[j], s[j+1], rank[j+2]));
  };

  int offset = (n%3 == 1);
  parlay::sequence<uint> SA = parlay::merge(SA0.cut(offset, n0),
                                          SA12.cut(1 - offset, n12),
                                          less);

  return SA;
}

inline parlay::sequence<unsigned int> suffixArray(parlay::sequence<unsigned char> const &s) {
  bool findLCPs = false;
  size_t n = s.size();
  auto ss = parlay::tabulate(n + 3, [&] (size_t i) -> uint {
      if (i >= n) return 0;
      else return ((uint) s[i]) + (uint) 1;
    });
  size_t k = 1 + parlay::reduce(ss, parlay::maxm<uint>());

  parlay::sequence<uint> SA = suffixArrayRec(ss, k, findLCPs);
  return SA;
}

} // namespace pbbs_parallel_ks
