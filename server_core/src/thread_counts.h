#pragma once

/**
 * @file thread_counts.h
 * @brief Helper to determine thread counts based on machine capacity.
 */

#include <vector>


namespace ThreadCounts {

    /**
     * @brief Pick thread counts to sweep over.
     *
     * Powers of two from 1 up to max_threads. If max_threads itself is not a
     * power of two, it is appended so we always measure the actual ceiling.
     *
     *   max=1  -> {1}
     *   max=4  -> {1, 2, 4}
     *   max=7  -> {1, 2, 4, 7}
     *   max=16 -> {1, 2, 4, 8, 16}
     *   max=20 -> {1, 2, 4, 8, 16, 20}
     *
     * The same ladder is used for correctness and performance modes - the only
     * difference between them is which scenarios run, not how many cores.
     */
    inline std::vector<int> get(int max_threads) {
        if(max_threads < 1) max_threads = 1;
        std::vector<int> counts;
        for(int n = 1; n <= max_threads; n *= 2) {
            counts.push_back(n);
        }
        if(counts.back() != max_threads) {
            counts.push_back(max_threads);
        }
        return counts;
    }

} // namespace ThreadCounts
