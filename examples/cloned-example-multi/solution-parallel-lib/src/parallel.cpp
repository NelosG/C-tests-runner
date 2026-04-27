#include <scan.h>

#include <algorithm>
#include <par/pragma.h>
#include <vector>


namespace parallel {

    // Same two-pass algorithm, but expressed via parallel_lib's OMP_* macros so
    // that the engine's monitor counters (parallel regions, barriers, for-loops)
    // are populated.
    void scan(const std::vector<long long>& array, std::vector<long long>& result) {
        const std::size_t n = array.size();
        result.resize(n);
        if(n == 0) return;

        const int num_threads = par::max_threads();
        const std::size_t chunk = (n + static_cast<std::size_t>(num_threads) - 1)
            / static_cast<std::size_t>(num_threads);

        std::vector<long long> chunk_sums(static_cast<std::size_t>(num_threads) + 1, 0);

        OMP_PARALLEL(num_threads(num_threads))
        {
            const int tid = par::thread_id();
            const std::size_t s = static_cast<std::size_t>(tid) * chunk;
            const std::size_t e = std::min(s + chunk, n);

            long long acc = 0;
            for(std::size_t i = s; i < e; ++i) {
                acc += array[i];
                result[i] = acc;
            }
            chunk_sums[static_cast<std::size_t>(tid) + 1] = acc;

            OMP_BARRIER;

            long long offset = 0;
            for(int t = 1; t <= tid; ++t) offset += chunk_sums[static_cast<std::size_t>(t)];

            for(std::size_t i = s; i < e; ++i) result[i] += offset;
        }
    }

} // namespace parallel
