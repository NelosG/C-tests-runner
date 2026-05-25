# pbbs-examples/nearestNeighbors

**Assignment:** pbbs nearestNeighbors - parallel KD-tree
**Framework:** parlay
**Mode:** all
**Run status:** completed

Benchmark run on WSL (Ubuntu, parlay scheduler) at thread counts: correctness=[1, 2, 4, 8, 16], performance=[1, 2, 4, 8, 16].

## Results

```
--- Correctness: Correctness.OctTreeKNN ---
  ✓ 2d_k1_n100 (1T) [1.1ms, 6MB, speedup=1.00x, eff=100%]
  ✓ 2d_k1_n100 (2T) [1.2ms, 6MB, speedup=0.89x, eff=45%]
  ✓ 2d_k1_n100 (4T) [1.1ms, 6MB, speedup=0.95x, eff=24%]
  ✓ 2d_k1_n100 (8T) [1.4ms, 6MB, speedup=0.79x, eff=10%]
  ✓ 2d_k1_n100 (16T) [1.4ms, 6MB, speedup=0.79x, eff=5%]
  ✓ 2d_k5_n200 (1T) [1.6ms, 6MB, speedup=1.00x, eff=100%]
  ✓ 2d_k5_n200 (2T) [1.9ms, 6MB, speedup=0.84x, eff=42%]
  ✓ 2d_k5_n200 (4T) [1.8ms, 6MB, speedup=0.89x, eff=22%]
  ✓ 2d_k5_n200 (8T) [1.7ms, 6MB, speedup=0.91x, eff=11%]
  ✓ 2d_k5_n200 (16T) [2.1ms, 6MB, speedup=0.73x, eff=5%]
  ✓ 3d_k5_n200 (1T) [1.6ms, 6MB, speedup=1.00x, eff=100%]
  ✓ 3d_k5_n200 (2T) [1.7ms, 6MB, speedup=0.96x, eff=48%]
  ✓ 3d_k5_n200 (4T) [1.9ms, 6MB, speedup=0.87x, eff=22%]
  ✓ 3d_k5_n200 (8T) [1.8ms, 6MB, speedup=0.90x, eff=11%]
  ✓ 3d_k5_n200 (16T) [2.2ms, 6MB, speedup=0.74x, eff=5%]
  ✓ 2d_k10_n1k (1T) [6.4ms, 6MB, speedup=1.00x, eff=100%]
  ✓ 2d_k10_n1k (2T) [6.4ms, 7MB, speedup=0.99x, eff=50%]
  ✓ 2d_k10_n1k (4T) [6.6ms, 6MB, speedup=0.96x, eff=24%]
  ✓ 2d_k10_n1k (8T) [7.0ms, 6MB, speedup=0.91x, eff=11%]
  ✓ 2d_k10_n1k (16T) [7.3ms, 6MB, speedup=0.88x, eff=5%]
  ✓ 3d_k10_n1k (1T) [8.3ms, 6MB, speedup=1.00x, eff=100%]
  ✓ 3d_k10_n1k (2T) [12.1ms, 6MB, speedup=0.68x, eff=34%]
  ✓ 3d_k10_n1k (4T) [8.8ms, 6MB, speedup=0.94x, eff=24%]
  ✓ 3d_k10_n1k (8T) [8.4ms, 6MB, speedup=0.99x, eff=12%]
  ✓ 3d_k10_n1k (16T) [10.4ms, 6MB, speedup=0.80x, eff=5%]
--- Performance: Performance.OctTreeKNN ---
  T1=5.97s  Tp=678.6ms  Speedup=8.80x  Efficiency=55%
  ✓ perf_3d_k10_3M (1T) [5.97s, 1494MB, speedup=1.00x, eff=100%]
  ✓ perf_3d_k10_3M (2T) [2.96s, 1495MB, speedup=2.02x, eff=101%]
  ✓ perf_3d_k10_3M (4T) [1.66s, 1497MB, speedup=3.61x, eff=90%]
  ✓ perf_3d_k10_3M (8T) [984.5ms, 1499MB, speedup=6.07x, eff=76%]
  ✓ perf_3d_k10_3M (16T) [678.6ms, 1505MB, speedup=8.80x, eff=55%]
--- Summary: Correctness ---
  Tests: 25/25 passed, 0 failed
  Max time: 12.1ms
  Peak RSS: 7MB
  Peak cgroup memory: 6MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |    18.9ms |   1.00x |   100% |     0.03s |     6MB |   5/ 5
          2 |    23.3ms |   0.81x |    41% |     0.04s |     7MB |   5/ 5
          4 |    20.2ms |   0.94x |    23% |     0.07s |     6MB |   5/ 5
          8 |    20.2ms |   0.94x |    12% |     0.12s |     6MB |   5/ 5
         16 |    23.4ms |   0.81x |     5% |     0.29s |     6MB |   5/ 5
--- Summary: Performance ---
  Tests: 5/5 passed, 0 failed
  Max time: 5.97s
  Peak RSS: 1505MB
  Peak cgroup memory: 1509MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |     5.97s |   1.00x |   100% |     6.49s |   1494MB |   1/ 1
          2 |     2.96s |   2.02x |   101% |     6.18s |   1495MB |   1/ 1
          4 |     1.66s |   3.61x |    90% |     6.39s |   1497MB |   1/ 1
          8 |   984.5ms |   6.07x |    76% |     6.93s |   1499MB |   1/ 1
         16 |   678.6ms |   8.80x |    55% |     8.46s |   1505MB |   1/ 1
```
