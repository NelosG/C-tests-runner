# pbbs-examples/delaunayRefine

**Assignment:** pbbs delaunayRefine - incremental refine
**Framework:** parlay
**Mode:** all
**Run status:** completed

Benchmark run on WSL (Ubuntu, parlay scheduler) at thread counts: correctness=[1, 2, 4, 8, 16], performance=[1, 2, 4, 8, 16].

## Results

```
--- Correctness: Correctness.IncrementalRefine ---
  ✓ n20_ang20 (1T) [20.1ms, 32MB, speedup=1.00x, eff=100%]
  ✓ n20_ang20 (2T) [15.2ms, 32MB, speedup=1.32x, eff=66%]
  ✓ n20_ang20 (4T) [10.6ms, 32MB, speedup=1.90x, eff=47%]
  ✓ n20_ang20 (8T) [12.3ms, 31MB, speedup=1.63x, eff=20%]
  ✓ n20_ang20 (16T) [17.8ms, 30MB, speedup=1.13x, eff=7%]
  ✓ n50_ang25 (1T) [21.7ms, 33MB, speedup=1.00x, eff=100%]
  ✓ n50_ang25 (2T) [16.1ms, 32MB, speedup=1.34x, eff=67%]
  ✓ n50_ang25 (4T) [12.4ms, 33MB, speedup=1.74x, eff=44%]
  ✓ n50_ang25 (8T) [14.2ms, 32MB, speedup=1.52x, eff=19%]
  ✓ n50_ang25 (16T) [19.0ms, 31MB, speedup=1.14x, eff=7%]
  ✓ n100_ang20 (1T) [24.5ms, 34MB, speedup=1.00x, eff=100%]
  ✓ n100_ang20 (2T) [17.9ms, 34MB, speedup=1.37x, eff=68%]
  ✓ n100_ang20 (4T) [13.0ms, 33MB, speedup=1.88x, eff=47%]
  ✓ n100_ang20 (8T) [12.6ms, 32MB, speedup=1.94x, eff=24%]
  ✓ n100_ang20 (16T) [19.4ms, 32MB, speedup=1.26x, eff=8%]
  ✓ n200_ang20 (1T) [31.5ms, 36MB, speedup=1.00x, eff=100%]
  ✓ n200_ang20 (2T) [22.1ms, 36MB, speedup=1.42x, eff=71%]
  ✓ n200_ang20 (4T) [17.5ms, 36MB, speedup=1.81x, eff=45%]
  ✓ n200_ang20 (8T) [16.0ms, 35MB, speedup=1.97x, eff=25%]
  ✓ n200_ang20 (16T) [20.3ms, 34MB, speedup=1.56x, eff=10%]
--- Performance: Performance.IncrementalRefine ---
  T1=1.95s  Tp=340.3ms  Speedup=5.74x  Efficiency=36%
  ✓ perf_100k (1T) [1.95s, 1872MB, speedup=1.00x, eff=100%]
  ✓ perf_100k (2T) [783.8ms, 1872MB, speedup=2.49x, eff=125%]
  ✓ perf_100k (4T) [509.8ms, 1872MB, speedup=3.83x, eff=96%]
  ✓ perf_100k (8T) [368.3ms, 1871MB, speedup=5.31x, eff=66%]
  ✓ perf_100k (16T) [340.3ms, 1870MB, speedup=5.74x, eff=36%]
--- Summary: Correctness ---
  Tests: 20/20 passed, 0 failed
  Max time: 31.5ms
  Peak RSS: 36MB
  Peak cgroup memory: 34MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |    97.7ms |   1.00x |   100% |     0.11s |    36MB |   4/ 4
          2 |    71.4ms |   1.37x |    68% |     0.14s |    36MB |   4/ 4
          4 |    53.5ms |   1.83x |    46% |     0.19s |    36MB |   4/ 4
          8 |    55.1ms |   1.77x |    22% |     0.36s |    35MB |   4/ 4
         16 |    76.4ms |   1.28x |     8% |     0.76s |    34MB |   4/ 4
--- Summary: Performance ---
  Tests: 5/5 passed, 0 failed
  Max time: 1.95s
  Peak RSS: 1872MB
  Peak cgroup memory: 1875MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |     1.95s |   1.00x |   100% |     2.04s |   1872MB |   1/ 1
          2 |   783.8ms |   2.49x |   125% |     1.53s |   1872MB |   1/ 1
          4 |   509.8ms |   3.83x |    96% |     1.80s |   1872MB |   1/ 1
          8 |   368.3ms |   5.31x |    66% |     2.33s |   1871MB |   1/ 1
         16 |   340.3ms |   5.74x |    36% |     3.93s |   1870MB |   1/ 1
```
