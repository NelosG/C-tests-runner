// Vendored from pbbsbench/common/speculative_for.h
// (MIT licensed, (c) Guy Blelloch and the PBBS team).
#pragma once

#include <algorithm>
#include <atomics.h>
#include <limits>
#include <parlay/parallel.h>
#include <parlay/primitives.h>

namespace pbbs {

  // idxT should be able to represent the range of iterations
  // int OK for up to 2^31 iterations
  // unsigned OK if freeze not used
  template <class idxT>
  struct reservation {
    std::atomic<idxT> r;
    static constexpr idxT max_idx = std::numeric_limits<idxT>::max();
    reservation() : r(max_idx) {}
    idxT get() const { return r.load();}
    bool reserve(idxT i) { return parlay::write_min(&r, i, std::less<idxT>());}
    bool reserved() const { return (r.load() < max_idx);}
    void reset() {r = max_idx;}
    void freeze() {r = -1;}
    bool check(idxT i) const { return (r.load() == i);}
    bool checkReset(idxT i) {
      if (r==i) { r = max_idx; return 1;}
      else return 0;
    }
  };

  template <class idxT, class S>
  long speculative_for(S step, idxT s, idxT e, long granularity,
                       bool hasState=1, long maxTries=-1) {
    if (maxTries < 0) maxTries = 100 + 200*granularity;
    long maxRoundSize = (e-s)/granularity+1;
    // floor at 1: original pbbsbench init can yield 0 for n < granularity*4,
    // which deadlocks the round loop on small graphs.
    long currentRoundSize = std::max(long{1}, maxRoundSize/4);
    auto I = parlay::sequence<idxT>::uninitialized(maxRoundSize);
    auto keep = parlay::sequence<bool>::uninitialized(maxRoundSize);
    parlay::sequence<idxT> Ihold;
    parlay::sequence<S> state;
    if (hasState)
      state = parlay::tabulate(maxRoundSize, [&] (size_t i) -> S {return step;});

    long round = 0;
    long numberDone = s;
    long numberKeep = 0;
    long totalProcessed = 0;

    while (numberDone < e) {
      if (round++ > maxTries)
        throw std::runtime_error("speculative_for: too many iterations, increase maxTries");
      // explicit long cast: idxT may be unsigned and conflict with currentRoundSize=long
      long size = std::min(currentRoundSize, static_cast<long>(e) - numberDone);

      totalProcessed += size;
      size_t loop_granularity = 0;

      if (hasState) {
        parlay::parallel_for (0, size, [&] (size_t i) {
          I[i] = (i < numberKeep) ? Ihold[i] : numberDone + i;
          keep[i] = state[i].reserve(I[i]);
        }, loop_granularity);
      } else {
        parlay::parallel_for (0, size, [&] (size_t i) {
          I[i] = (i < numberKeep) ? Ihold[i] : numberDone + i;
          keep[i] = step.reserve(I[i]);
        }, loop_granularity);
      }

      if (hasState) {
        parlay::parallel_for (0, size, [&] (size_t i) {
          if (keep[i]) keep[i] = !state[i].commit(I[i]);}, loop_granularity);
      } else {
        parlay::parallel_for (0, size, [&] (size_t i) {
          if (keep[i]) keep[i] = !step.commit(I[i]);}, loop_granularity);
      }

      Ihold = parlay::pack(I.head(size), keep.head(size));
      numberKeep = Ihold.size();
      numberDone += size - numberKeep;

      if (float(numberKeep)/float(size) > .2)
        currentRoundSize = std::max(currentRoundSize/2,
                                    std::max(maxRoundSize/64 + 1, numberKeep));
      else if (float(numberKeep)/float(size) < .1)
        currentRoundSize = std::min(currentRoundSize * 2, maxRoundSize);
    }
    return totalProcessed;
  }
}
