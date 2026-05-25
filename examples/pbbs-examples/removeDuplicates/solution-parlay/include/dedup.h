// Student API for removeDuplicates. Single parallel variant - uses
// parlay::remove_duplicates internally (a parallel hash-set dedup).
//
// Parlay-native: input/output are parlay::sequence directly, materialised
// outside the timed region by the runner (runner_parlay.h).

#pragma once

#include <cstdint>

#include <parlay/sequence.h>

namespace student {

    // Returns a sequence containing each distinct value from `in` once.
    // Order is unspecified (verify compares sorted-unique sets).
    parlay::sequence<std::uint32_t> parlayhash_dedup(
        const parlay::sequence<std::uint32_t>& in);

} // namespace student
