// Student API for integerSort. One parallel variant: radix sort via
// parlay::internal::integer_sort. Mirrors pbbsbench's parallelRadixSort.
//
// Parlay-native signature: the input/output are parlay::sequence so the
// runner does no std::vector <-> parlay::sequence conversion inside the
// timed region. The runner main reads the TLV array via
// runner::read_parlay_sequence (un-timed) and passes the sequence in.

#pragma once

#include <cstddef>
#include <cstdint>

#include <parlay/sequence.h>

namespace student {

    // Returns the input sorted ascending. `bits` is log2 of the largest
    // value range to consider (pbbs uses 32 for uint). `in` is taken by
    // mutable reference (it is read, not consumed - pbbs's int_sort uses a
    // mutable slice but does not modify it), so the same buffer survives
    // across warmup iterations without a copy.
    parlay::sequence<std::uint32_t> parallel_radix_sort(
        parlay::sequence<std::uint32_t>& in,
        std::size_t bits);

} // namespace student
