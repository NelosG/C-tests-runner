# pbbs-examples/concurrentKNN

**Assignment:** pbbs concurrentKNN - parallel KD-tree
**Framework:** parlay
**Mode:** all
**Run status:** completed

Benchmark run on WSL (Ubuntu, parlay scheduler) at thread counts: correctness=[1, 2, 4, 8, 16], performance=[1, 2, 4, 8, 16].

## Results

```
--- Correctness: Correctness.OctTreeCKNN ---
  ✓ 2d_k1_n100 (1T) [1.0ms, 6MB, speedup=1.00x, eff=100%]
  ✓ 2d_k1_n100 (2T) [1.1ms, 6MB, speedup=0.96x, eff=48%]
  ✓ 2d_k1_n100 (4T) [1.1ms, 6MB, speedup=0.92x, eff=23%]
  ✓ 2d_k1_n100 (8T) [1.2ms, 6MB, speedup=0.89x, eff=11%]
  ✓ 2d_k1_n100 (16T) [1.4ms, 6MB, speedup=0.74x, eff=5%]
  ✓ 2d_k5_n200 (1T) [1.6ms, 6MB, speedup=1.00x, eff=100%]
  ✓ 2d_k5_n200 (2T) [1.6ms, 6MB, speedup=1.02x, eff=51%]
  ✓ 2d_k5_n200 (4T) [1.6ms, 6MB, speedup=1.01x, eff=25%]
  ✓ 2d_k5_n200 (8T) [1.7ms, 6MB, speedup=0.97x, eff=12%]
  ✓ 2d_k5_n200 (16T) [1.8ms, 6MB, speedup=0.90x, eff=6%]
  ✓ 3d_k5_n200 (1T) [1.5ms, 6MB, speedup=1.00x, eff=100%]
  ✓ 3d_k5_n200 (2T) [1.5ms, 6MB, speedup=0.95x, eff=47%]
  ✓ 3d_k5_n200 (4T) [1.6ms, 6MB, speedup=0.93x, eff=23%]
  ✓ 3d_k5_n200 (8T) [1.6ms, 6MB, speedup=0.92x, eff=11%]
  ✓ 3d_k5_n200 (16T) [1.7ms, 6MB, speedup=0.85x, eff=5%]
  ✓ 2d_k10_n1k (1T) [6.5ms, 6MB, speedup=1.00x, eff=100%]
  ✓ 2d_k10_n1k (2T) [6.3ms, 6MB, speedup=1.02x, eff=51%]
  ✓ 2d_k10_n1k (4T) [6.7ms, 6MB, speedup=0.96x, eff=24%]
  ✓ 2d_k10_n1k (8T) [6.3ms, 6MB, speedup=1.03x, eff=13%]
  ✓ 2d_k10_n1k (16T) [6.9ms, 6MB, speedup=0.93x, eff=6%]
  ✓ 3d_k10_n1k (1T) [8.3ms, 6MB, speedup=1.00x, eff=100%]
  ✓ 3d_k10_n1k (2T) [8.1ms, 6MB, speedup=1.02x, eff=51%]
  ✓ 3d_k10_n1k (4T) [8.2ms, 6MB, speedup=1.00x, eff=25%]
  ✓ 3d_k10_n1k (8T) [8.0ms, 6MB, speedup=1.03x, eff=13%]
  ✓ 3d_k10_n1k (16T) [8.8ms, 6MB, speedup=0.94x, eff=6%]
--- Performance: Performance.OctTreeCKNN ---
  T1=6.83s  Tp=922.6ms  Speedup=7.41x  Efficiency=46%
  ✓ perf_3d_k10_3M (1T) [6.83s, 1494MB, speedup=1.00x, eff=100%]
  ✓ perf_3d_k10_3M (2T) [3.11s, 1495MB, speedup=2.20x, eff=110%]
  ✓ perf_3d_k10_3M (4T) [1.87s, 1498MB, speedup=3.65x, eff=91%]
  ✓ perf_3d_k10_3M (8T) [1.23s, 1499MB, speedup=5.55x, eff=69%]
  ✓ perf_3d_k10_3M (16T) [922.6ms, 1506MB, speedup=7.41x, eff=46%]
--- Summary: Correctness ---
  Tests: 25/25 passed, 0 failed
  Max time: 8.8ms
  Peak RSS: 6MB
  Peak cgroup memory: 6MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |    18.9ms |   1.00x |   100% |     0.03s |     6MB |   5/ 5
          2 |    18.7ms |   1.01x |    51% |     0.04s |     6MB |   5/ 5
          4 |    19.3ms |   0.98x |    24% |     0.06s |     6MB |   5/ 5
          8 |    18.8ms |   1.01x |    13% |     0.11s |     6MB |   5/ 5
         16 |    20.7ms |   0.91x |     6% |     0.26s |     6MB |   5/ 5
--- Summary: Performance ---
  Tests: 5/5 passed, 0 failed
  Max time: 6.83s
  Peak RSS: 1506MB
  Peak cgroup memory: 1509MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |     6.83s |   1.00x |   100% |     7.48s |   1494MB |   1/ 1
          2 |     3.11s |   2.20x |   110% |     6.34s |   1495MB |   1/ 1
          4 |     1.87s |   3.65x |    91% |     6.62s |   1498MB |   1/ 1
          8 |     1.23s |   5.55x |    69% |     6.90s |   1499MB |   1/ 1
         16 |   922.6ms |   7.41x |    46% |     7.79s |   1506MB |   1/ 1
```
