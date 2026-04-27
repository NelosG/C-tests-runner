#include <scan.h>

#include <algorithm>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include <vector>


namespace parallel {

    // Two-pass inclusive scan: per-block local sum, then add prefix offsets.
    void scan(const std::vector<long long>& array, std::vector<long long>& result) {
        const std::size_t n = array.size();
        result.resize(n);
        if(n == 0) return;

        const int num_workers = __cilkrts_get_nworkers();
        const std::size_t num_blocks = std::max<std::size_t>(
            1,
            std::min<std::size_t>(static_cast<std::size_t>(num_workers), n)
        );
        const std::size_t block_size = (n + num_blocks - 1) / num_blocks;

        std::vector<long long> block_sums(num_blocks + 1, 0);

        // Phase 1: per-block local prefix sum (parallel)
        cilk_for(std::size_t b = 0;
        b < num_blocks;
        ++b
        )
        {
            std::size_t s = b * block_size;
            std::size_t e = std::min(s + block_size, n);
            long long acc = 0;
            for(std::size_t i = s; i < e; ++i) {
                acc += array[i];
                result[i] = acc;
            }
            block_sums[b + 1] = acc;
        }

        // Phase 2: sequential exclusive prefix sum over block_sums (small, num_blocks elements)
        for(std::size_t b = 1; b <= num_blocks; ++b) {
            block_sums[b] += block_sums[b - 1];
        }

        // Phase 3: distribute offsets (skip block 0 - already correct)
        cilk_for(std::size_t b = 1;
        b < num_blocks;
        ++b
        )
        {
            std::size_t s = b * block_size;
            std::size_t e = std::min(s + block_size, n);
            long long off = block_sums[b];
            for(std::size_t i = s; i < e; ++i) result[i] += off;
        }
    }

} // namespace parallel
