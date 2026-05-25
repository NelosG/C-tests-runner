# pbbs-examples/suffixArray

**Assignment:** pbbs suffixArray - parallelKS
**Framework:** parlay
**Mode:** all
**Run status:** completed

Benchmark run on WSL (Ubuntu, parlay scheduler) at thread counts: correctness=[1, 2, 4, 8, 16], performance=[1, 2, 4, 8, 16].

## Results

```
--- Correctness: Correctness.ParallelKS ---
  ✓ banana (1T) [1.4ms, 7MB, speedup=1.00x, eff=100%]
  ✓ banana (2T) [1.3ms, 8MB, speedup=1.10x, eff=55%]
  ✓ banana (4T) [1.3ms, 7MB, speedup=1.11x, eff=28%]
  ✓ banana (8T) [1.4ms, 8MB, speedup=1.02x, eff=13%]
  ✓ banana (16T) [1.5ms, 7MB, speedup=0.97x, eff=6%]
  ✓ mississippi (1T) [1.4ms, 8MB, speedup=1.00x, eff=100%]
  ✓ mississippi (2T) [1.3ms, 8MB, speedup=1.06x, eff=53%]
  ✓ mississippi (4T) [1.5ms, 8MB, speedup=0.94x, eff=24%]
  ✓ mississippi (8T) [1.4ms, 8MB, speedup=0.96x, eff=12%]
  ✓ mississippi (16T) [1.8ms, 8MB, speedup=0.77x, eff=5%]
  ✓ all_same (1T) [1.4ms, 8MB, speedup=1.00x, eff=100%]
  ✓ all_same (2T) [1.3ms, 8MB, speedup=1.08x, eff=54%]
  ✓ all_same (4T) [1.4ms, 8MB, speedup=1.00x, eff=25%]
  ✓ all_same (8T) [1.5ms, 8MB, speedup=0.94x, eff=12%]
  ✓ all_same (16T) [1.6ms, 8MB, speedup=0.87x, eff=5%]
  ✓ alphabet (1T) [1.5ms, 8MB, speedup=1.00x, eff=100%]
  ✓ alphabet (2T) [1.4ms, 8MB, speedup=1.14x, eff=57%]
  ✓ alphabet (4T) [1.4ms, 8MB, speedup=1.12x, eff=28%]
  ✓ alphabet (8T) [1.5ms, 8MB, speedup=1.05x, eff=13%]
  ✓ alphabet (16T) [1.7ms, 8MB, speedup=0.91x, eff=6%]
  ✓ random_1k (1T) [2.5ms, 8MB, speedup=1.00x, eff=100%]
  ✓ random_1k (2T) [2.3ms, 8MB, speedup=1.08x, eff=54%]
  ✓ random_1k (4T) [2.4ms, 8MB, speedup=1.05x, eff=26%]
  ✓ random_1k (8T) [2.3ms, 8MB, speedup=1.10x, eff=14%]
  ✓ random_1k (16T) [2.4ms, 8MB, speedup=1.02x, eff=6%]
  ✓ random_10k (1T) [10.6ms, 8MB, speedup=1.00x, eff=100%]
  ✓ random_10k (2T) [7.6ms, 8MB, speedup=1.40x, eff=70%]
  ✓ random_10k (4T) [6.0ms, 8MB, speedup=1.79x, eff=45%]
  ✓ random_10k (8T) [5.8ms, 8MB, speedup=1.82x, eff=23%]
  ✓ random_10k (16T) [6.3ms, 8MB, speedup=1.68x, eff=10%]
  ✓ random_100k (1T) [96.0ms, 13MB, speedup=1.00x, eff=100%]
  ✓ random_100k (2T) [59.4ms, 13MB, speedup=1.62x, eff=81%]
  ✓ random_100k (4T) [40.8ms, 13MB, speedup=2.35x, eff=59%]
  ✓ random_100k (8T) [32.3ms, 12MB, speedup=2.97x, eff=37%]
  ✓ random_100k (16T) [33.8ms, 11MB, speedup=2.84x, eff=18%]
--- Performance: Performance.ParallelKS ---
  T1=6.30s  Tp=847.0ms  Speedup=7.44x  Efficiency=46%
  ✓ perf_30M (1T) [6.30s, 1748MB, speedup=1.00x, eff=100%]
  ✓ perf_30M (2T) [3.37s, 1753MB, speedup=1.87x, eff=93%]
  ✓ perf_30M (4T) [1.94s, 1755MB, speedup=3.24x, eff=81%]
  ✓ perf_30M (8T) [1.17s, 1761MB, speedup=5.37x, eff=67%]
  ✓ perf_30M (16T) [847.0ms, 1770MB, speedup=7.44x, eff=46%]
--- Summary: Correctness ---
  Tests: 35/35 passed, 0 failed
  Max time: 96.0ms
  Peak RSS: 13MB
  Peak cgroup memory: 11MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |   114.9ms |   1.00x |   100% |     0.14s |    13MB |   7/ 7
          2 |    74.6ms |   1.54x |    77% |     0.15s |    13MB |   7/ 7
          4 |    54.7ms |   2.10x |    53% |     0.18s |    13MB |   7/ 7
          8 |    46.3ms |   2.48x |    31% |     0.29s |    12MB |   7/ 7
         16 |    49.2ms |   2.33x |    15% |     0.61s |    11MB |   7/ 7
--- Summary: Performance ---
  Tests: 5/5 passed, 0 failed
  Max time: 6.30s
  Peak RSS: 1770MB
  Peak cgroup memory: 1775MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |     6.30s |   1.00x |   100% |     6.84s |   1748MB |   1/ 1
          2 |     3.37s |   1.87x |    93% |     7.05s |   1753MB |   1/ 1
          4 |     1.94s |   3.24x |    81% |     7.60s |   1755MB |   1/ 1
          8 |     1.17s |   5.37x |    67% |     9.13s |   1761MB |   1/ 1
         16 |   847.0ms |   7.44x |    46% |    11.63s |   1770MB |   1/ 1
```
