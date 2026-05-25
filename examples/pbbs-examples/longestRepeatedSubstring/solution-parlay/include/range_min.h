// Vendored from pbbsbench/algorithm/range_min.h
// (MIT licensed, (c) Guy Blelloch and the PBBS team).
#ifndef PBBS_RANGE_MIN_H_
#define PBBS_RANGE_MIN_H_

#include <parlay/parallel.h>
#include <parlay/primitives.h>

template <class Seq, class Compare, class Uint=unsigned int>
class range_min {
public:
  range_min(Seq &a, Compare less, long block_size=32)
    :  a(a), less(less), n(a.size()), block_size(block_size) {
    m = 1 + (n-1)/block_size;
    precomputeQueries();
  }

  Uint query(Uint i, Uint j) {
    if (j-i < block_size) {
      Uint r = i;
      for (long k = i+1; k <= j; k++)
        r = min_index(r, k);
      return r;
    }
    long block_i = i/block_size;
    long block_j = j/block_size;
    Uint minl = i;

    for (long k = minl+1; k < (block_i+1) * block_size; k++)
      minl = min_index(minl, k);

    long minr = block_j * block_size;
    for (long k = minr+1; k <= j; k++)
      minr = min_index(minr, k);

    if (block_j == block_i + 1) return min_index(minl,minr);

    Uint outOfBlockMin;
    block_i++;
    block_j--;
    if (block_j == block_i)
      outOfBlockMin = table[0][block_i];
    else if(block_j == block_i + 1)
      outOfBlockMin = table[1][block_i];
    else {
      long k = parlay::log2_up(block_j - block_i + 1) - 1;
      long p = 1 << k;
      outOfBlockMin = min_index(table[k][block_i], table[k][block_j+1-p]);
    }
    return min_index(minl, min_index(outOfBlockMin, minr));
  }

private:
  Seq &a;
  parlay::sequence<parlay::sequence<Uint>> table;
  Compare less;
  long n, m, depth, block_size;

  Uint min_index(Uint i, Uint j) {
    return less(a[j], a[i]) ? j : i;}

  void precomputeQueries() {
    depth = parlay::log2_up(m+1);
    table = parlay::tabulate(depth, [&] (size_t) {
         return parlay::sequence<Uint>(m);});

    parlay::internal::sliced_for(n, block_size, [&] (size_t i, size_t start, size_t end) {
          long k = start;
          for (size_t j = start+1; j < end; j++)
            k = min_index(j, k);
          table[0][i] = k;
        });

    long dist = 1;
    for (long j = 1; j < depth; j++) {
      parlay::parallel_for (0, m - dist, [&] (size_t i) {
           table[j][i] = min_index(table[j-1][i], table[j-1][i+dist]);});
      parlay::parallel_for (m - dist, m, [&] (size_t i) {
           table[j][i] = table[j-1][i];});
      dist*=2;
    }
  }
};

template <class Seq, class Compare, class Uint=unsigned int>
range_min<Seq,Compare,Uint> make_range_min(Seq &a, Compare less, long block_size=32) {
  return range_min<Seq,Compare,Uint>(a, less, block_size);
}

#endif
