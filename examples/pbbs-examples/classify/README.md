# pbbs-examples/classify

**Assignment:** pbbs classify - decisionTree
**Framework:** parlay
**Mode:** all
**Run status:** completed

Benchmark run on WSL (Ubuntu, parlay scheduler) at thread counts: correctness=[1, 2, 4, 8, 16], performance=[1, 2, 4, 8, 16].

## Results

```
--- Correctness: Correctness.DecisionTree ---
  ✓ copy_3vals_3feat (1T) [3.8ms, 14MB, speedup=1.00x, eff=100%]
  ✓ copy_3vals_3feat (2T) [3.4ms, 15MB, speedup=1.11x, eff=56%]
  ✓ copy_3vals_3feat (4T) [4.4ms, 18MB, speedup=0.86x, eff=21%]
  ✓ copy_3vals_3feat (8T) [4.7ms, 20MB, speedup=0.81x, eff=10%]
  ✓ copy_3vals_3feat (16T) [5.9ms, 20MB, speedup=0.65x, eff=4%]
  ✓ copy_4vals_3feat (1T) [4.4ms, 14MB, speedup=1.00x, eff=100%]
  ✓ copy_4vals_3feat (2T) [3.9ms, 16MB, speedup=1.12x, eff=56%]
  ✓ copy_4vals_3feat (4T) [4.2ms, 19MB, speedup=1.03x, eff=26%]
  ✓ copy_4vals_3feat (8T) [5.2ms, 24MB, speedup=0.83x, eff=10%]
  ✓ copy_4vals_3feat (16T) [8.4ms, 29MB, speedup=0.52x, eff=3%]
  ✓ copy_3vals_5feat (1T) [8.0ms, 14MB, speedup=1.00x, eff=100%]
  ✓ copy_3vals_5feat (2T) [6.2ms, 17MB, speedup=1.28x, eff=64%]
  ✓ copy_3vals_5feat (4T) [6.1ms, 20MB, speedup=1.31x, eff=33%]
  ✓ copy_3vals_5feat (8T) [7.0ms, 26MB, speedup=1.13x, eff=14%]
  ✓ copy_3vals_5feat (16T) [11.4ms, 35MB, speedup=0.70x, eff=4%]
  ✓ copy_3vals_5k (1T) [13.9ms, 14MB, speedup=1.00x, eff=100%]
  ✓ copy_3vals_5k (2T) [9.4ms, 16MB, speedup=1.48x, eff=74%]
  ✓ copy_3vals_5k (4T) [7.9ms, 19MB, speedup=1.75x, eff=44%]
  ✓ copy_3vals_5k (8T) [7.7ms, 22MB, speedup=1.79x, eff=22%]
  ✓ copy_3vals_5k (16T) [10.9ms, 28MB, speedup=1.27x, eff=8%]
--- Performance: Performance.DecisionTree ---
  T1=11.36s  Tp=1.79s  Speedup=6.35x  Efficiency=40%
  ✓ perf_20feat_15M (1T) [11.36s, 1938MB, speedup=1.00x, eff=100%]
  ✓ perf_20feat_15M (2T) [5.93s, 2061MB, speedup=1.92x, eff=96%]
  ✓ perf_20feat_15M (4T) [3.13s, 2319MB, speedup=3.63x, eff=91%]
  ✓ perf_20feat_15M (8T) [1.93s, 2409MB, speedup=5.88x, eff=73%]
  ✓ perf_20feat_15M (16T) [1.79s, 2575MB, speedup=6.35x, eff=40%]
--- Summary: Correctness ---
  Tests: 20/20 passed, 0 failed
  Max time: 13.9ms
  Peak RSS: 35MB
  Peak cgroup memory: 27MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |    30.0ms |   1.00x |   100% |     0.05s |    14MB |   4/ 4
          2 |    22.9ms |   1.31x |    65% |     0.05s |    17MB |   4/ 4
          4 |    22.6ms |   1.33x |    33% |     0.07s |    20MB |   4/ 4
          8 |    24.7ms |   1.21x |    15% |     0.14s |    26MB |   4/ 4
         16 |    36.6ms |   0.82x |     5% |     0.40s |    35MB |   4/ 4
--- Summary: Performance ---
  Tests: 5/5 passed, 0 failed
  Max time: 11.36s
  Peak RSS: 2575MB
  Peak cgroup memory: 2579MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |    11.36s |   1.00x |   100% |    12.74s |   1938MB |   1/ 1
          2 |     5.93s |   1.92x |    96% |    12.98s |   2061MB |   1/ 1
          4 |     3.13s |   3.63x |    91% |    13.62s |   2319MB |   1/ 1
          8 |     1.93s |   5.88x |    73% |    15.35s |   2409MB |   1/ 1
         16 |     1.79s |   6.35x |    40% |    24.21s |   2575MB |   1/ 1
```
