# pbbs-examples/nBody

**Assignment:** pbbs nBody - all-pairs O(n^2)
**Framework:** parlay
**Mode:** all
**Run status:** completed

Benchmark run on WSL (Ubuntu, parlay scheduler) at thread counts: correctness=[1, 2, 4, 8, 16], performance=[1, 2, 4, 8, 16].

## Results

```
--- Correctness: Correctness.AllPairsNBody ---
  ✓ n50 (1T) [2.2ms, 9MB, speedup=1.00x, eff=100%]
  ✓ n50 (2T) [2.1ms, 9MB, speedup=1.03x, eff=51%]
  ✓ n50 (4T) [2.1ms, 9MB, speedup=1.03x, eff=26%]
  ✓ n50 (8T) [2.2ms, 9MB, speedup=0.98x, eff=12%]
  ✓ n50 (16T) [2.5ms, 9MB, speedup=0.87x, eff=5%]
  ✓ n200 (1T) [4.6ms, 8MB, speedup=1.00x, eff=100%]
  ✓ n200 (2T) [4.6ms, 9MB, speedup=1.01x, eff=50%]
  ✓ n200 (4T) [4.8ms, 8MB, speedup=0.97x, eff=24%]
  ✓ n200 (8T) [4.7ms, 8MB, speedup=0.98x, eff=12%]
  ✓ n200 (16T) [5.3ms, 8MB, speedup=0.87x, eff=5%]
  ✓ n500 (1T) [16.6ms, 10MB, speedup=1.00x, eff=100%]
  ✓ n500 (2T) [12.8ms, 12MB, speedup=1.30x, eff=65%]
  ✓ n500 (4T) [12.0ms, 12MB, speedup=1.39x, eff=35%]
  ✓ n500 (8T) [12.3ms, 12MB, speedup=1.35x, eff=17%]
  ✓ n500 (16T) [14.0ms, 12MB, speedup=1.19x, eff=7%]
  ✓ n1k (1T) [46.6ms, 10MB, speedup=1.00x, eff=100%]
  ✓ n1k (2T) [30.5ms, 12MB, speedup=1.53x, eff=76%]
  ✓ n1k (4T) [21.6ms, 15MB, speedup=2.16x, eff=54%]
  ✓ n1k (8T) [21.0ms, 17MB, speedup=2.23x, eff=28%]
  ✓ n1k (16T) [22.4ms, 17MB, speedup=2.08x, eff=13%]
--- Performance: Performance.AllPairsNBody ---
  T1=25.29s  Tp=2.24s  Speedup=11.31x  Efficiency=71%
  ✓ perf_1M (1T) [25.29s, 977MB, speedup=1.00x, eff=100%]
  ✓ perf_1M (2T) [13.75s, 994MB, speedup=1.84x, eff=92%]
  ✓ perf_1M (4T) [6.72s, 998MB, speedup=3.76x, eff=94%]
  ✓ perf_1M (8T) [3.59s, 1022MB, speedup=7.04x, eff=88%]
  ✓ perf_1M (16T) [2.24s, 1044MB, speedup=11.31x, eff=71%]
--- Summary: Correctness ---
  Tests: 20/20 passed, 0 failed
  Max time: 46.6ms
  Peak RSS: 17MB
  Peak cgroup memory: 13MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |    70.1ms |   1.00x |   100% |     0.08s |    10MB |   4/ 4
          2 |    50.0ms |   1.40x |    70% |     0.09s |    12MB |   4/ 4
          4 |    40.5ms |   1.73x |    43% |     0.12s |    15MB |   4/ 4
          8 |    40.2ms |   1.74x |    22% |     0.22s |    17MB |   4/ 4
         16 |    44.3ms |   1.58x |    10% |     0.48s |    17MB |   4/ 4
--- Summary: Performance ---
  Tests: 5/5 passed, 0 failed
  Max time: 25.29s
  Peak RSS: 1044MB
  Peak cgroup memory: 1046MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |    25.29s |   1.00x |   100% |    25.46s |   977MB |   1/ 1
          2 |    13.75s |   1.84x |    92% |    27.60s |   994MB |   1/ 1
          4 |     6.72s |   3.76x |    94% |    26.90s |   998MB |   1/ 1
          8 |     3.59s |   7.04x |    88% |    28.48s |   1022MB |   1/ 1
         16 |     2.24s |   11.31x |    71% |    32.93s |   1044MB |   1/ 1
```
