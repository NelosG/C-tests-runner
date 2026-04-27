#include <scan.h>

#include <algorithm>
#include <omp.h>
#include <vector>


namespace parallel {

    // Two-pass inclusive scan: per-thread local sum, then add prefix offsets.
    void scan(const std::vector<long long>& array, std::vector<long long>& result) {
        const std::size_t n = array.size();
        result.resize(n);
        if(n == 0) return;

        int num_threads = 1;
        #pragma omp parallel
        {
            #pragma omp single
            num_threads = omp_get_num_threads();
        }

        std::vector<long long> chunk_sums(static_cast<std::size_t>(num_threads) + 1, 0);
        const std::size_t chunk = (n + static_cast<std::size_t>(num_threads) - 1)
            / static_cast<std::size_t>(num_threads);

        #pragma omp parallel num_threads(num_threads)
        {
            const int tid = omp_get_thread_num();
            const std::size_t s = static_cast<std::size_t>(tid) * chunk;
            const std::size_t e = std::min(s + chunk, n);

            long long acc = 0;
            for(std::size_t i = s; i < e; ++i) {
                acc += array[i];
                result[i] = acc;
            }
            chunk_sums[static_cast<std::size_t>(tid) + 1] = acc;

            #pragma omp barrier

            long long offset = 0;
            for(int t = 1; t <= tid; ++t) offset += chunk_sums[static_cast<std::size_t>(t)];

            for(std::size_t i = s; i < e; ++i) result[i] += offset;
        }
    }

} // namespace parallel
