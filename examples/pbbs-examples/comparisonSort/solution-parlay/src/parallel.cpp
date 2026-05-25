// All four sort variants in one translation unit. Splitting them across
// multiple .cpp files would cause link errors because parlay's scheduler
// header defines a non-inline thread_local that ODR-collides when included
// in more than one TU of the same shared library. Each vendored variant
// lives in its own namespace (pbbs_<variant>::), so they coexist cleanly.
//
// Parlay-native: input/output are parlay::sequence<int>; no std::vector <->
// sequence conversion inside the timed region. int (4-byte) matches pbbs's
// comparisonSort element type. The two sample-sort variants return a fresh
// sequence; quick/merge sort in place and return the (moved) input.

#include <functional>

#include <pbbs_merge_sort.h>
#include <pbbs_parlay_sample_sort.h>
#include <pbbs_quick_sort.h>
#include <pbbs_stable_sample_sort.h>
#include <sort.h>

namespace student {

    parlay::sequence<int> parlay_sample_sort(parlay::sequence<int>& arr) {
        return pbbs_parlay_sample_sort::compSort(arr, std::less<int>());
    }

    parlay::sequence<int> parallel_quick_sort(parlay::sequence<int>& arr) {
        pbbs_quick_sort::compSort(arr, std::less<int>());
        return std::move(arr);
    }

    parlay::sequence<int> parallel_merge_sort(parlay::sequence<int>& arr) {
        pbbs_merge_sort::compSort(arr, std::less<int>());
        return std::move(arr);
    }

    parlay::sequence<int> parallel_stable_sample_sort(parlay::sequence<int>& arr) {
        return pbbs_stable_sample_sort::compSort(arr, std::less<int>());
    }

} // namespace student
