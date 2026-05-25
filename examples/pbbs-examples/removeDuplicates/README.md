# pbbs-examples/removeDuplicates

**Assignment:** pbbs removeDuplicates - parlayhash variant
**Framework:** parlay
**Mode:** all
**Run status:** completed

Benchmark run on WSL (Ubuntu, parlay scheduler) at thread counts: correctness=[1, 2, 4, 8, 16], performance=[1, 2, 4, 8, 16].

## Results

```
--- Correctness: Correctness.ParlayhashDedup ---
  ✓ single (1T) [0.5ms, 5MB, speedup=1.00x, eff=100%]
  ✓ single (2T) [0.5ms, 5MB, speedup=1.11x, eff=55%]
  ✓ single (4T) [0.5ms, 5MB, speedup=1.06x, eff=27%]
  ✓ single (8T) [0.5ms, 5MB, speedup=1.04x, eff=13%]
  ✓ single (16T) [0.5ms, 5MB, speedup=1.04x, eff=7%]
  ✓ all_unique (1T) [0.6ms, 5MB, speedup=1.00x, eff=100%]
  ✓ all_unique (2T) [0.6ms, 5MB, speedup=1.07x, eff=54%]
  ✓ all_unique (4T) [0.6ms, 5MB, speedup=1.14x, eff=29%]
  ✓ all_unique (8T) [0.6ms, 5MB, speedup=1.08x, eff=13%]
  ✓ all_unique (16T) [0.6ms, 5MB, speedup=1.04x, eff=7%]
  ✓ all_duplicates (1T) [0.9ms, 5MB, speedup=1.00x, eff=100%]
  ✓ all_duplicates (2T) [0.5ms, 5MB, speedup=1.62x, eff=81%]
  ✓ all_duplicates (4T) [0.6ms, 5MB, speedup=1.53x, eff=38%]
  ✓ all_duplicates (8T) [0.6ms, 5MB, speedup=1.51x, eff=19%]
  ✓ all_duplicates (16T) [0.6ms, 5MB, speedup=1.47x, eff=9%]
  ✓ mixed (1T) [0.6ms, 5MB, speedup=1.00x, eff=100%]
  ✓ mixed (2T) [0.6ms, 5MB, speedup=0.97x, eff=48%]
  ✓ mixed (4T) [0.5ms, 5MB, speedup=1.03x, eff=26%]
  ✓ mixed (8T) [0.6ms, 5MB, speedup=0.92x, eff=11%]
  ✓ mixed (16T) [0.5ms, 5MB, speedup=1.13x, eff=7%]
  ✓ random_10k_small_range (1T) [3.0ms, 6MB, speedup=1.00x, eff=100%]
  ✓ random_10k_small_range (2T) [2.3ms, 9MB, speedup=1.29x, eff=64%]
  ✓ random_10k_small_range (4T) [2.7ms, 12MB, speedup=1.11x, eff=28%]
  ✓ random_10k_small_range (8T) [3.1ms, 14MB, speedup=0.96x, eff=12%]
  ✓ random_10k_small_range (16T) [5.1ms, 19MB, speedup=0.59x, eff=4%]
  ✓ random_100k_medium_range (1T) [13.5ms, 7MB, speedup=1.00x, eff=100%]
  ✓ random_100k_medium_range (2T) [8.3ms, 8MB, speedup=1.62x, eff=81%]
  ✓ random_100k_medium_range (4T) [5.2ms, 10MB, speedup=2.58x, eff=64%]
  ✓ random_100k_medium_range (8T) [3.8ms, 13MB, speedup=3.56x, eff=44%]
  ✓ random_100k_medium_range (16T) [3.8ms, 18MB, speedup=3.57x, eff=22%]
  ✓ random_1M_large_range (1T) [123.9ms, 27MB, speedup=1.00x, eff=100%]
  ✓ random_1M_large_range (2T) [74.7ms, 29MB, speedup=1.66x, eff=83%]
  ✓ random_1M_large_range (4T) [45.1ms, 30MB, speedup=2.75x, eff=69%]
  ✓ random_1M_large_range (8T) [25.1ms, 34MB, speedup=4.93x, eff=62%]
  ✓ random_1M_large_range (16T) [21.2ms, 40MB, speedup=5.84x, eff=36%]
--- Performance: Performance.ParlayhashDedup ---
  T1=3.29s  Tp=698.1ms  Speedup=4.72x  Efficiency=29%
  ✓ perf_200M (1T) [3.29s, 3722MB, speedup=1.00x, eff=100%]
  ✓ perf_200M (2T) [1.78s, 3741MB, speedup=1.85x, eff=92%]
  ✓ perf_200M (4T) [1.05s, 3742MB, speedup=3.14x, eff=78%]
  ✓ perf_200M (8T) [760.0ms, 3745MB, speedup=4.33x, eff=54%]
  ✓ perf_200M (16T) [698.1ms, 3751MB, speedup=4.72x, eff=29%]
--- Summary: Correctness ---
  Tests: 35/35 passed, 0 failed
  Max time: 123.9ms
  Peak RSS: 40MB
  Peak cgroup memory: 40MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |   143.0ms |   1.00x |   100% |     0.21s |    27MB |   7/ 7
          2 |    87.6ms |   1.63x |    82% |     0.23s |    29MB |   7/ 7
          4 |    55.2ms |   2.59x |    65% |     0.27s |    30MB |   7/ 7
          8 |    34.4ms |   4.16x |    52% |     0.31s |    34MB |   7/ 7
         16 |    32.3ms |   4.42x |    28% |     0.47s |    40MB |   7/ 7
--- Summary: Performance ---
  Tests: 5/5 passed, 0 failed
  Max time: 3.29s
  Peak RSS: 3751MB
  Peak cgroup memory: 3760MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |     3.29s |   1.00x |   100% |     5.85s |   3722MB |   1/ 1
          2 |     1.78s |   1.85x |    92% |     5.70s |   3741MB |   1/ 1
          4 |     1.05s |   3.14x |    78% |     6.01s |   3742MB |   1/ 1
          8 |   760.0ms |   4.33x |    54% |     6.96s |   3745MB |   1/ 1
         16 |   698.1ms |   4.72x |    29% |     8.36s |   3751MB |   1/ 1
```
