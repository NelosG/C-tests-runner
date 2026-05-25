// Student API for longestRepeatedSubstring. One variant: doubling-LRS
// from pbbsbench (SA + LCP + max).
//
// Parlay-native: input is a parlay::sequence<unsigned char> materialised by
// the runner outside RUNNER_EXECUTE.

#pragma once

#include <cstdint>
#include <tuple>

#include <parlay/sequence.h>

namespace student {

    // Returns (length, pos1, pos2) of the longest substring of `s` that
    // appears at two distinct positions.
    std::tuple<std::int64_t,std::int64_t,std::int64_t>
    doubling_lrs(const parlay::sequence<unsigned char>& s);

} // namespace student
