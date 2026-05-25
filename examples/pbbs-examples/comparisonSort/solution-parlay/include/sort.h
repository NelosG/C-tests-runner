#pragma once

#include <parlay/sequence.h>

// Student API for the pbbsbench comparisonSort assignment. Four parallel
// sort variants; each takes a parlay::sequence<int> (materialised by the
// runner outside the timed region) and returns the sorted sequence. Element
// type is int (4-byte) to match pbbs's comparisonSort, which sorts `int`
// with std::less<int> for a `randomSeq -t int` input.
//
// Parlay-native: no std::vector <-> sequence conversion inside RUNNER_EXECUTE.
// comparisonSort keeps warmupIterations=0 (the sort mutates its input), so a
// mutable-reference input that the in-place variants consume is fine.
namespace student {

    // Wraps parlay::internal::sample_sort (returns new sequence).
    parlay::sequence<int> parlay_sample_sort(parlay::sequence<int>& arr);

    // Wraps parlay::internal::p_quicksort_inplace.
    parlay::sequence<int> parallel_quick_sort(parlay::sequence<int>& arr);

    // Wraps parlay::internal::merge_sort_inplace.
    parlay::sequence<int> parallel_merge_sort(parlay::sequence<int>& arr);

    // Wraps parlay::internal::sample_sort with stable=true.
    parlay::sequence<int> parallel_stable_sample_sort(parlay::sequence<int>& arr);

} // namespace student
