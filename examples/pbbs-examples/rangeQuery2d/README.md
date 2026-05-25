# pbbs-examples/rangeQuery2d

**Assignment:** pbbs rangeQuery2d - parallel range count
**Framework:** parlay
**Mode:** all
**Run status:** completed

Benchmark run on WSL (Ubuntu, parlay scheduler) at thread counts: correctness=[1, 2, 4, 8, 16], performance=[1, 2, 4, 8, 16].

## Results

```
--- Correctness: Correctness.ParallelRangeCount ---
  ✓ 110p_10q (1T) [0.4ms, 5MB, speedup=1.00x, eff=100%]
  ✓ 110p_10q (2T) [0.4ms, 5MB, speedup=1.03x, eff=51%]
  ✓ 110p_10q (4T) [0.5ms, 4MB, speedup=0.86x, eff=21%]
  ✓ 110p_10q (8T) [0.5ms, 4MB, speedup=0.80x, eff=10%]
  ✓ 110p_10q (16T) [0.6ms, 5MB, speedup=0.67x, eff=4%]
  ✓ 1k_50q (1T) [0.8ms, 5MB, speedup=1.00x, eff=100%]
  ✓ 1k_50q (2T) [0.7ms, 5MB, speedup=1.17x, eff=58%]
  ✓ 1k_50q (4T) [0.7ms, 5MB, speedup=1.19x, eff=30%]
  ✓ 1k_50q (8T) [0.7ms, 4MB, speedup=1.28x, eff=16%]
  ✓ 1k_50q (16T) [0.8ms, 5MB, speedup=1.06x, eff=7%]
  ✓ 10k_100q (1T) [5.9ms, 5MB, speedup=1.00x, eff=100%]
  ✓ 10k_100q (2T) [4.6ms, 5MB, speedup=1.29x, eff=65%]
  ✓ 10k_100q (4T) [4.0ms, 5MB, speedup=1.49x, eff=37%]
  ✓ 10k_100q (8T) [3.4ms, 5MB, speedup=1.74x, eff=22%]
  ✓ 10k_100q (16T) [3.6ms, 5MB, speedup=1.63x, eff=10%]
--- Performance: Performance.ParallelRangeCount ---
  T1=608.3ms  Tp=63.8ms  Speedup=9.54x  Efficiency=60%
  ✓ perf_2M_4k (1T) [608.3ms, 156MB, speedup=1.00x, eff=100%]
  ✓ perf_2M_4k (2T) [303.3ms, 158MB, speedup=2.01x, eff=100%]
  ✓ perf_2M_4k (4T) [156.3ms, 162MB, speedup=3.89x, eff=97%]
  ✓ perf_2M_4k (8T) [90.5ms, 168MB, speedup=6.72x, eff=84%]
  ✓ perf_2M_4k (16T) [63.8ms, 182MB, speedup=9.54x, eff=60%]
--- Summary: Correctness ---
  Tests: 15/15 passed, 0 failed
  Max time: 5.9ms
  Peak RSS: 5MB
  Peak cgroup memory: 5MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |     7.2ms |   1.00x |   100% |     0.02s |     5MB |   3/ 3
          2 |     5.7ms |   1.26x |    63% |     0.02s |     5MB |   3/ 3
          4 |     5.2ms |   1.39x |    35% |     0.02s |     5MB |   3/ 3
          8 |     4.6ms |   1.57x |    20% |     0.03s |     5MB |   3/ 3
         16 |     5.0ms |   1.43x |     9% |     0.04s |     5MB |   3/ 3
--- Summary: Performance ---
  Tests: 5/5 passed, 0 failed
  Max time: 608.3ms
  Peak RSS: 182MB
  Peak cgroup memory: 183MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |   608.3ms |   1.00x |   100% |     0.72s |   156MB |   1/ 1
          2 |   303.3ms |   2.01x |   100% |     0.71s |   158MB |   1/ 1
          4 |   156.3ms |   3.89x |    97% |     0.74s |   162MB |   1/ 1
          8 |    90.5ms |   6.72x |    84% |     0.82s |   168MB |   1/ 1
         16 |    63.8ms |   9.54x |    60% |     1.03s |   182MB |   1/ 1
```
