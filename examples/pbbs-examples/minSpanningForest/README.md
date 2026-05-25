# pbbs-examples/minSpanningForest

**Assignment:** pbbs minSpanningForest - parallelKruskal
**Framework:** parlay
**Mode:** all
**Run status:** completed

Benchmark run on WSL (Ubuntu, parlay scheduler) at thread counts: correctness=[1, 2, 4, 8, 16], performance=[1, 2, 4, 8, 16].

## Results

```
--- Correctness: Correctness.ParallelKruskalMST ---
  ✓ single_edge (1T) [0.8ms, 6MB, speedup=1.00x, eff=100%]
  ✓ single_edge (2T) [0.8ms, 6MB, speedup=0.96x, eff=48%]
  ✓ single_edge (4T) [0.7ms, 6MB, speedup=1.08x, eff=27%]
  ✓ single_edge (8T) [0.7ms, 6MB, speedup=1.08x, eff=13%]
  ✓ single_edge (16T) [0.8ms, 6MB, speedup=1.02x, eff=6%]
  ✓ triangle (1T) [0.8ms, 6MB, speedup=1.00x, eff=100%]
  ✓ triangle (2T) [0.8ms, 6MB, speedup=0.95x, eff=47%]
  ✓ triangle (4T) [0.8ms, 6MB, speedup=1.00x, eff=25%]
  ✓ triangle (8T) [0.9ms, 6MB, speedup=0.90x, eff=11%]
  ✓ triangle (16T) [1.0ms, 6MB, speedup=0.81x, eff=5%]
  ✓ square_cycle (1T) [0.9ms, 6MB, speedup=1.00x, eff=100%]
  ✓ square_cycle (2T) [0.9ms, 6MB, speedup=1.02x, eff=51%]
  ✓ square_cycle (4T) [0.8ms, 6MB, speedup=1.03x, eff=26%]
  ✓ square_cycle (8T) [1.0ms, 6MB, speedup=0.87x, eff=11%]
  ✓ square_cycle (16T) [1.1ms, 6MB, speedup=0.81x, eff=5%]
  ✓ two_components (1T) [0.8ms, 6MB, speedup=1.00x, eff=100%]
  ✓ two_components (2T) [0.9ms, 6MB, speedup=0.94x, eff=47%]
  ✓ two_components (4T) [0.9ms, 6MB, speedup=0.94x, eff=23%]
  ✓ two_components (8T) [1.0ms, 6MB, speedup=0.86x, eff=11%]
  ✓ two_components (16T) [1.1ms, 6MB, speedup=0.74x, eff=5%]
  ✓ random_100 (1T) [1.4ms, 6MB, speedup=1.00x, eff=100%]
  ✓ random_100 (2T) [1.2ms, 7MB, speedup=1.16x, eff=58%]
  ✓ random_100 (4T) [1.2ms, 7MB, speedup=1.16x, eff=29%]
  ✓ random_100 (8T) [1.3ms, 7MB, speedup=1.05x, eff=13%]
  ✓ random_100 (16T) [1.4ms, 6MB, speedup=0.97x, eff=6%]
  ✓ random_1k (1T) [3.2ms, 7MB, speedup=1.00x, eff=100%]
  ✓ random_1k (2T) [2.6ms, 7MB, speedup=1.24x, eff=62%]
  ✓ random_1k (4T) [2.5ms, 7MB, speedup=1.27x, eff=32%]
  ✓ random_1k (8T) [2.5ms, 7MB, speedup=1.28x, eff=16%]
  ✓ random_1k (16T) [2.9ms, 7MB, speedup=1.13x, eff=7%]
  ✓ random_10k (1T) [23.8ms, 11MB, speedup=1.00x, eff=100%]
  ✓ random_10k (2T) [14.2ms, 11MB, speedup=1.68x, eff=84%]
  ✓ random_10k (4T) [10.1ms, 10MB, speedup=2.36x, eff=59%]
  ✓ random_10k (8T) [7.3ms, 10MB, speedup=3.26x, eff=41%]
  ✓ random_10k (16T) [7.8ms, 9MB, speedup=3.05x, eff=19%]
--- Performance: Performance.ParallelKruskalMST ---
  T1=9.47s  Tp=1.10s  Speedup=8.62x  Efficiency=54%
  ✓ perf_7M_45M (1T) [9.47s, 4537MB, speedup=1.00x, eff=100%]
  ✓ perf_7M_45M (2T) [4.78s, 4537MB, speedup=1.98x, eff=99%]
  ✓ perf_7M_45M (4T) [2.56s, 4537MB, speedup=3.70x, eff=92%]
  ✓ perf_7M_45M (8T) [1.55s, 4536MB, speedup=6.13x, eff=77%]
  ✓ perf_7M_45M (16T) [1.10s, 4535MB, speedup=8.62x, eff=54%]
--- Summary: Correctness ---
  Tests: 35/35 passed, 0 failed
  Max time: 23.8ms
  Peak RSS: 11MB
  Peak cgroup memory: 7MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |    31.7ms |   1.00x |   100% |     0.06s |    11MB |   7/ 7
          2 |    21.4ms |   1.48x |    74% |     0.07s |    11MB |   7/ 7
          4 |    17.1ms |   1.86x |    46% |     0.08s |    10MB |   7/ 7
          8 |    14.7ms |   2.15x |    27% |     0.12s |    10MB |   7/ 7
         16 |    16.1ms |   1.98x |    12% |     0.24s |     9MB |   7/ 7
--- Summary: Performance ---
  Tests: 5/5 passed, 0 failed
  Max time: 9.47s
  Peak RSS: 4537MB
  Peak cgroup memory: 4545MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |     9.47s |   1.00x |   100% |    13.41s |   4537MB |   1/ 1
          2 |     4.78s |   1.98x |    99% |    12.73s |   4537MB |   1/ 1
          4 |     2.56s |   3.70x |    92% |    13.29s |   4537MB |   1/ 1
          8 |     1.55s |   6.13x |    77% |    14.83s |   4536MB |   1/ 1
         16 |     1.10s |   8.62x |    54% |    18.22s |   4535MB |   1/ 1
```
