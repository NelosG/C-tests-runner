# pbbs-examples/ANN

**Assignment:** pbbs ANN - KD-tree approximate NN
**Framework:** parlay
**Mode:** all
**Run status:** completed

Benchmark run on WSL (Ubuntu, parlay scheduler) at thread counts: correctness=[1, 2, 4, 8, 16], performance=[1, 2, 4, 8, 16].

## Results

```
--- Correctness: Correctness.HCNNG ---
  ✓ d16_k10_n500 (1T) [690.0ms, 11MB, speedup=1.00x, eff=100%]
  ✓ d16_k10_n500 (2T) [359.7ms, 14MB, speedup=1.92x, eff=96%]
  ✓ d16_k10_n500 (4T) [213.0ms, 19MB, speedup=3.24x, eff=81%]
  ✓ d16_k10_n500 (8T) [135.0ms, 28MB, speedup=5.11x, eff=64%]
  ✓ d16_k10_n500 (16T) [114.1ms, 48MB, speedup=6.05x, eff=38%]
  ✓ d32_k10_n1k (1T) [1.62s, 12MB, speedup=1.00x, eff=100%]
  ✓ d32_k10_n1k (2T) [833.4ms, 15MB, speedup=1.95x, eff=97%]
  ✓ d32_k10_n1k (4T) [452.8ms, 20MB, speedup=3.59x, eff=90%]
  ✓ d32_k10_n1k (8T) [273.4ms, 30MB, speedup=5.94x, eff=74%]
  ✓ d32_k10_n1k (16T) [213.4ms, 49MB, speedup=7.61x, eff=48%]
  ✓ d64_k10_n1k (1T) [2.09s, 13MB, speedup=1.00x, eff=100%]
  ✓ d64_k10_n1k (2T) [1.08s, 15MB, speedup=1.94x, eff=97%]
  ✓ d64_k10_n1k (4T) [602.5ms, 20MB, speedup=3.47x, eff=87%]
  ✓ d64_k10_n1k (8T) [352.6ms, 30MB, speedup=5.93x, eff=74%]
  ✓ d64_k10_n1k (16T) [260.2ms, 49MB, speedup=8.04x, eff=50%]
--- Performance: Performance.HCNNG ---
  T1=5.24s  Tp=490.3ms  Speedup=10.70x  Efficiency=67%
  ✓ perf_d128_k10_n7k (1T) [5.24s, 29MB, speedup=1.00x, eff=100%]
  ✓ perf_d128_k10_n7k (2T) [2.57s, 32MB, speedup=2.04x, eff=102%]
  ✓ perf_d128_k10_n7k (4T) [1.33s, 37MB, speedup=3.95x, eff=99%]
  ✓ perf_d128_k10_n7k (8T) [747.9ms, 47MB, speedup=7.01x, eff=88%]
  ✓ perf_d128_k10_n7k (16T) [490.3ms, 68MB, speedup=10.70x, eff=67%]
--- Summary: Correctness ---
  Tests: 15/15 passed, 0 failed
  Max time: 2.09s
  Peak RSS: 49MB
  Peak cgroup memory: 46MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |     4.41s |   1.00x |   100% |     4.42s |    13MB |   3/ 3
          2 |     2.27s |   1.94x |    97% |     4.49s |    15MB |   3/ 3
          4 |     1.27s |   3.47x |    87% |     4.84s |    20MB |   3/ 3
          8 |   761.1ms |   5.79x |    72% |     5.62s |    30MB |   3/ 3
         16 |   587.7ms |   7.50x |    47% |     8.07s |    49MB |   3/ 3
--- Summary: Performance ---
  Tests: 5/5 passed, 0 failed
  Max time: 5.24s
  Peak RSS: 68MB
  Peak cgroup memory: 68MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |     5.24s |   1.00x |   100% |     5.24s |    29MB |   1/ 1
          2 |     2.57s |   2.04x |   102% |     5.11s |    32MB |   1/ 1
          4 |     1.33s |   3.95x |    99% |     5.22s |    37MB |   1/ 1
          8 |   747.9ms |   7.01x |    88% |     5.72s |    47MB |   1/ 1
         16 |   490.3ms |   10.70x |    67% |     6.79s |    68MB |   1/ 1
```
