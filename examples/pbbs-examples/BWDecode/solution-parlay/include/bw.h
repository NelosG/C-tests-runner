// Student API for BWDecode. One parallel variant: pbbsbench's listRank
// list-ranking-based inverse BWT.
//
// Parlay-native: input is a parlay::sequence<unsigned char> materialised by
// the runner outside RUNNER_EXECUTE.

#pragma once

#include <string>

#include <parlay/sequence.h>

namespace student {

    // Input: BWT-encoded byte sequence (includes a single null sentinel
    // character as in pbbsbench). Output: plaintext with the null
    // dropped (length = input.size() - 1).
    std::string list_rank_bw_decode(const parlay::sequence<unsigned char>& encoded);

} // namespace student
