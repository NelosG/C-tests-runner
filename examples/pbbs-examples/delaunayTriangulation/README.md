# pbbs-examples/delaunayTriangulation

**Assignment:** pbbs delaunayTriangulation - Bowyer-Watson
**Framework:** parlay
**Mode:** all
**Run status:** completed

Benchmark run on WSL (Ubuntu, parlay scheduler) at thread counts: correctness=[1, 2, 4, 8, 16], performance=[1, 2, 4, 8, 16].

## Results

```
--- Correctness: Correctness.BowyerWatson ---
  ✓ n10 (1T) [10.9ms, 8MB, speedup=1.00x, eff=100%]
  ✓ n10 (2T) [1.7ms, 8MB, speedup=6.24x, eff=312%]
  ✓ n10 (4T) [1.8ms, 8MB, speedup=6.00x, eff=150%]
  ✓ n10 (8T) [1.9ms, 8MB, speedup=5.59x, eff=70%]
  ✓ n10 (16T) [2.0ms, 8MB, speedup=5.32x, eff=33%]
  ✓ n50 (1T) [1.8ms, 8MB, speedup=1.00x, eff=100%]
  ✓ n50 (2T) [1.8ms, 8MB, speedup=1.00x, eff=50%]
  ✓ n50 (4T) [1.9ms, 8MB, speedup=0.95x, eff=24%]
  ✓ n50 (8T) [1.9ms, 8MB, speedup=0.94x, eff=12%]
  ✓ n50 (16T) [2.0ms, 8MB, speedup=0.86x, eff=5%]
  ✓ n200 (1T) [2.5ms, 8MB, speedup=1.00x, eff=100%]
  ✓ n200 (2T) [2.6ms, 8MB, speedup=0.97x, eff=48%]
  ✓ n200 (4T) [2.4ms, 8MB, speedup=1.03x, eff=26%]
  ✓ n200 (8T) [2.6ms, 8MB, speedup=0.96x, eff=12%]
  ✓ n200 (16T) [2.6ms, 8MB, speedup=0.94x, eff=6%]
  ✓ n500 (1T) [4.5ms, 8MB, speedup=1.00x, eff=100%]
  ✓ n500 (2T) [4.3ms, 8MB, speedup=1.04x, eff=52%]
  ✓ n500 (4T) [4.3ms, 7MB, speedup=1.05x, eff=26%]
  ✓ n500 (8T) [4.3ms, 7MB, speedup=1.06x, eff=13%]
  ✓ n500 (16T) [4.7ms, 8MB, speedup=0.96x, eff=6%]
  ✓ n1k (1T) [7.9ms, 8MB, speedup=1.00x, eff=100%]
  ✓ n1k (2T) [8.2ms, 8MB, speedup=0.96x, eff=48%]
  ✓ n1k (4T) [7.7ms, 8MB, speedup=1.02x, eff=26%]
  ✓ n1k (8T) [7.7ms, 8MB, speedup=1.02x, eff=13%]
  ✓ n1k (16T) [10.1ms, 8MB, speedup=0.78x, eff=5%]
--- Performance: Performance.BowyerWatson ---
  T1=5.23s  Tp=588.3ms  Speedup=8.88x  Efficiency=56%
  ✓ perf_2M (1T) [5.23s, 1009MB, speedup=1.00x, eff=100%]
  ✓ perf_2M (2T) [2.70s, 1010MB, speedup=1.94x, eff=97%]
  ✓ perf_2M (4T) [1.62s, 1012MB, speedup=3.22x, eff=81%]
  ✓ perf_2M (8T) [893.8ms, 1018MB, speedup=5.85x, eff=73%]
  ✓ perf_2M (16T) [588.3ms, 1030MB, speedup=8.88x, eff=56%]
--- Summary: Correctness ---
  Tests: 25/25 passed, 0 failed
  Max time: 10.9ms
  Peak RSS: 8MB
  Peak cgroup memory: 7MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |    27.5ms |   1.00x |   100% |     0.04s |     8MB |   5/ 5
          2 |    18.6ms |   1.48x |    74% |     0.04s |     8MB |   5/ 5
          4 |    18.1ms |   1.52x |    38% |     0.06s |     8MB |   5/ 5
          8 |    18.4ms |   1.50x |    19% |     0.11s |     8MB |   5/ 5
         16 |    21.5ms |   1.28x |     8% |     0.27s |     8MB |   5/ 5
--- Summary: Performance ---
  Tests: 5/5 passed, 0 failed
  Max time: 5.23s
  Peak RSS: 1030MB
  Peak cgroup memory: 1032MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |     5.23s |   1.00x |   100% |     5.53s |   1009MB |   1/ 1
          2 |     2.70s |   1.94x |    97% |     5.34s |   1010MB |   1/ 1
          4 |     1.62s |   3.22x |    81% |     5.75s |   1012MB |   1/ 1
          8 |   893.8ms |   5.85x |    73% |     6.43s |   1018MB |   1/ 1
         16 |   588.3ms |   8.88x |    56% |     7.96s |   1030MB |   1/ 1
```
