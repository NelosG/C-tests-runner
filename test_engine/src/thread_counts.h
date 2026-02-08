#pragma once

/**
 * @file thread_counts.h
 * @brief Helper to determine thread counts based on test mode.
 */

#include <string>
#include <vector>


namespace ThreadCounts {

    /**
 * @brief Determine which thread counts to test based on mode.
 * @param mode "correctness" | "performance" | "all"
 * @param max_threads Maximum thread count available.
 * @return Vector of thread counts for TestEngine::execute().
 */
    inline std::vector<int> get(const std::string& mode, int max_threads) {
        if(mode == "performance") {
            return {1, max_threads};
        }
        // "correctness" or "all"
        std::vector<int> counts = {1};
        if(max_threads >= 2) counts.push_back(2);
        if(max_threads >= 4) counts.push_back(4);
        if(max_threads > 4) counts.push_back(max_threads);
        return counts;
    }

} // namespace ThreadCounts