# pbbs-examples/rangeSearch

**Assignment:** pbbs rangeSearch
**Framework:** parlay
**Mode:** all
**Run status:** completed

Benchmark run on WSL (Ubuntu, parlay scheduler) at thread counts: correctness=[1, 2, 4, 8, 16], performance=[1, 2, 4, 8, 16].

## Results

```
--- Correctness: Correctness.HCNNGRange ---
  ✓ d16_n500_q50 (1T) [546.8ms, 11MB, speedup=1.00x, eff=100%]
  ✓ d16_n500_q50 (2T) [287.1ms, 14MB, speedup=1.90x, eff=95%]
  ✓ d16_n500_q50 (4T) [166.7ms, 19MB, speedup=3.28x, eff=82%]
  ✓ d16_n500_q50 (8T) [114.6ms, 29MB, speedup=4.77x, eff=60%]
  ✓ d16_n500_q50 (16T) [121.8ms, 48MB, speedup=4.49x, eff=28%]
  ✓ d32_n1k_q100 (1T) [1.24s, 12MB, speedup=1.00x, eff=100%]
  ✓ d32_n1k_q100 (2T) [648.9ms, 15MB, speedup=1.91x, eff=95%]
  ✓ d32_n1k_q100 (4T) [379.8ms, 20MB, speedup=3.26x, eff=82%]
  ✓ d32_n1k_q100 (8T) [228.5ms, 30MB, speedup=5.42x, eff=68%]
  ✓ d32_n1k_q100 (16T) [208.5ms, 49MB, speedup=5.94x, eff=37%]
  ✓ d64_n2k_q200 (1T) [2.85s, 14MB, speedup=1.00x, eff=100%]
  ✓ d64_n2k_q200 (2T) [1.47s, 17MB, speedup=1.94x, eff=97%]
  ✓ d64_n2k_q200 (4T) [787.7ms, 22MB, speedup=3.61x, eff=90%]
  ✓ d64_n2k_q200 (8T) [462.0ms, 32MB, speedup=6.16x, eff=77%]
  ✓ d64_n2k_q200 (16T) [375.9ms, 52MB, speedup=7.57x, eff=47%]
--- Performance: Performance.HCNNGRange ---
  T1=2.67s  Tp=306.4ms  Speedup=8.70x  Efficiency=54%
  ✓ perf_d128_n12k_q1500 (1T) [2.67s, 39MB, speedup=1.00x, eff=100%]
  ✓ perf_d128_n12k_q1500 (2T) [1.42s, 42MB, speedup=1.88x, eff=94%]
  ✓ perf_d128_n12k_q1500 (4T) [719.3ms, 47MB, speedup=3.71x, eff=93%]
  ✓ perf_d128_n12k_q1500 (8T) [413.2ms, 57MB, speedup=6.45x, eff=81%]
  ✓ perf_d128_n12k_q1500 (16T) [306.4ms, 78MB, speedup=8.70x, eff=54%]
--- Summary: Correctness ---
  Tests: 15/15 passed, 0 failed
  Max time: 2.85s
  Peak RSS: 52MB
  Peak cgroup memory: 49MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |     4.63s |   1.00x |   100% |     4.64s |    14MB |   3/ 3
          2 |     2.41s |   1.93x |    96% |     4.74s |    17MB |   3/ 3
          4 |     1.33s |   3.47x |    87% |     5.10s |    22MB |   3/ 3
          8 |   805.2ms |   5.75x |    72% |     5.91s |    32MB |   3/ 3
         16 |   706.1ms |   6.56x |    41% |     9.72s |    52MB |   3/ 3
--- Summary: Performance ---
  Tests: 5/5 passed, 0 failed
  Max time: 2.67s
  Peak RSS: 78MB
  Peak cgroup memory: 78MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |     2.67s |   1.00x |   100% |     2.68s |    39MB |   1/ 1
          2 |     1.42s |   1.88x |    94% |     2.83s |    42MB |   1/ 1
          4 |   719.3ms |   3.71x |    93% |     2.77s |    47MB |   1/ 1
          8 |   413.2ms |   6.45x |    81% |     2.98s |    57MB |   1/ 1
         16 |   306.4ms |   8.70x |    54% |     3.81s |    78MB |   1/ 1
```
