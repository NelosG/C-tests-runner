# pbbs-examples/rangeQueryKDTree

**Assignment:** pbbs rangeQueryKDTree
**Framework:** parlay
**Mode:** all
**Run status:** completed

Benchmark run on WSL (Ubuntu, parlay scheduler) at thread counts: correctness=[1, 2, 4, 8, 16], performance=[1, 2, 4, 8, 16].

## Results

```
--- Correctness: Correctness.Basic ---
  ✓ random_100_r2 (1T) [1.8ms, 6MB, speedup=1.00x, eff=100%]
  ✓ random_100_r2 (2T) [1.8ms, 7MB, speedup=0.99x, eff=49%]
  ✓ random_100_r2 (4T) [2.3ms, 9MB, speedup=0.80x, eff=20%]
  ✓ random_100_r2 (8T) [2.9ms, 11MB, speedup=0.62x, eff=8%]
  ✓ random_100_r2 (16T) [4.0ms, 12MB, speedup=0.45x, eff=3%]
  ✓ random_1k_r2 (1T) [14.4ms, 7MB, speedup=1.00x, eff=100%]
  ✓ random_1k_r2 (2T) [8.9ms, 9MB, speedup=1.61x, eff=80%]
  ✓ random_1k_r2 (4T) [6.6ms, 12MB, speedup=2.17x, eff=54%]
  ✓ random_1k_r2 (8T) [6.5ms, 17MB, speedup=2.20x, eff=28%]
  ✓ random_1k_r2 (16T) [9.4ms, 29MB, speedup=1.53x, eff=10%]
  ✓ random_10k_r1 (1T) [246.5ms, 23MB, speedup=1.00x, eff=100%]
  ✓ random_10k_r1 (2T) [133.9ms, 25MB, speedup=1.84x, eff=92%]
  ✓ random_10k_r1 (4T) [77.9ms, 29MB, speedup=3.17x, eff=79%]
  ✓ random_10k_r1 (8T) [53.2ms, 36MB, speedup=4.63x, eff=58%]
  ✓ random_10k_r1 (16T) [46.8ms, 51MB, speedup=5.27x, eff=33%]
--- Performance: Performance.Basic ---
  T1=9.49s  Tp=1.12s  Speedup=8.47x  Efficiency=53%
  ✓ perf_2M_r0.05 (1T) [9.49s, 2016MB, speedup=1.00x, eff=100%]
  ✓ perf_2M_r0.05 (2T) [5.06s, 2018MB, speedup=1.88x, eff=94%]
  ✓ perf_2M_r0.05 (4T) [2.83s, 2020MB, speedup=3.35x, eff=84%]
  ✓ perf_2M_r0.05 (8T) [1.76s, 2024MB, speedup=5.38x, eff=67%]
  ✓ perf_2M_r0.05 (16T) [1.12s, 2034MB, speedup=8.47x, eff=53%]
--- Summary: Correctness ---
  Tests: 15/15 passed, 0 failed
  Max time: 246.5ms
  Peak RSS: 51MB
  Peak cgroup memory: 51MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |   262.7ms |   1.00x |   100% |     0.28s |    23MB |   3/ 3
          2 |   144.7ms |   1.82x |    91% |     0.29s |    25MB |   3/ 3
          4 |    86.7ms |   3.03x |    76% |     0.32s |    29MB |   3/ 3
          8 |    62.6ms |   4.19x |    52% |     0.42s |    36MB |   3/ 3
         16 |    60.2ms |   4.36x |    27% |     0.69s |    51MB |   3/ 3
--- Summary: Performance ---
  Tests: 5/5 passed, 0 failed
  Max time: 9.49s
  Peak RSS: 2034MB
  Peak cgroup memory: 2040MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |     9.49s |   1.00x |   100% |    10.57s |   2016MB |   1/ 1
          2 |     5.06s |   1.88x |    94% |    10.65s |   2018MB |   1/ 1
          4 |     2.83s |   3.35x |    84% |    11.11s |   2020MB |   1/ 1
          8 |     1.76s |   5.38x |    67% |    11.99s |   2024MB |   1/ 1
         16 |     1.12s |   8.47x |    53% |    14.42s |   2034MB |   1/ 1
```
