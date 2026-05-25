# pbbs-examples/histogram

**Assignment:** pbbs histogram - parallel variant
**Framework:** parlay
**Mode:** all
**Run status:** completed

Benchmark run on WSL (Ubuntu, parlay scheduler) at thread counts: correctness=[1, 2, 4, 8, 16], performance=[1, 2, 4, 8, 16].

## Results

```
--- Correctness: Correctness.ParallelHistogram ---
  ✓ single_element (1T) [0.6ms, 6MB, speedup=1.00x, eff=100%]
  ✓ single_element (2T) [0.6ms, 6MB, speedup=1.03x, eff=52%]
  ✓ single_element (4T) [0.6ms, 6MB, speedup=0.98x, eff=24%]
  ✓ single_element (8T) [0.7ms, 6MB, speedup=0.91x, eff=11%]
  ✓ single_element (16T) [0.7ms, 6MB, speedup=0.91x, eff=6%]
  ✓ all_same_bucket (1T) [0.4ms, 6MB, speedup=1.00x, eff=100%]
  ✓ all_same_bucket (2T) [0.4ms, 6MB, speedup=1.14x, eff=57%]
  ✓ all_same_bucket (4T) [0.4ms, 6MB, speedup=1.07x, eff=27%]
  ✓ all_same_bucket (8T) [0.4ms, 6MB, speedup=1.05x, eff=13%]
  ✓ all_same_bucket (16T) [0.4ms, 6MB, speedup=1.07x, eff=7%]
  ✓ spread (1T) [0.5ms, 6MB, speedup=1.00x, eff=100%]
  ✓ spread (2T) [0.5ms, 6MB, speedup=1.08x, eff=54%]
  ✓ spread (4T) [0.5ms, 6MB, speedup=1.04x, eff=26%]
  ✓ spread (8T) [0.5ms, 6MB, speedup=1.04x, eff=13%]
  ✓ spread (16T) [0.6ms, 6MB, speedup=0.86x, eff=5%]
  ✓ random_10k_256buckets (1T) [1.9ms, 7MB, speedup=1.00x, eff=100%]
  ✓ random_10k_256buckets (2T) [1.6ms, 8MB, speedup=1.14x, eff=57%]
  ✓ random_10k_256buckets (4T) [1.6ms, 9MB, speedup=1.13x, eff=28%]
  ✓ random_10k_256buckets (8T) [1.6ms, 9MB, speedup=1.17x, eff=15%]
  ✓ random_10k_256buckets (16T) [2.7ms, 9MB, speedup=0.70x, eff=4%]
  ✓ random_100k_1024buckets (1T) [10.6ms, 8MB, speedup=1.00x, eff=100%]
  ✓ random_100k_1024buckets (2T) [7.2ms, 8MB, speedup=1.47x, eff=73%]
  ✓ random_100k_1024buckets (4T) [4.6ms, 9MB, speedup=2.33x, eff=58%]
  ✓ random_100k_1024buckets (8T) [3.4ms, 11MB, speedup=3.10x, eff=39%]
  ✓ random_100k_1024buckets (16T) [3.1ms, 15MB, speedup=3.39x, eff=21%]
  ✓ random_1M_65536buckets (1T) [93.0ms, 22MB, speedup=1.00x, eff=100%]
  ✓ random_1M_65536buckets (2T) [58.4ms, 23MB, speedup=1.59x, eff=80%]
  ✓ random_1M_65536buckets (4T) [33.6ms, 24MB, speedup=2.77x, eff=69%]
  ✓ random_1M_65536buckets (8T) [19.1ms, 25MB, speedup=4.86x, eff=61%]
  ✓ random_1M_65536buckets (16T) [14.3ms, 28MB, speedup=6.49x, eff=41%]
--- Performance: Performance.ParallelHistogram ---
  T1=4.75s  Tp=735.6ms  Speedup=6.46x  Efficiency=40%
  ✓ perf_500M_1Mbuckets (1T) [4.75s, 7637MB, speedup=1.00x, eff=100%]
  ✓ perf_500M_1Mbuckets (2T) [2.43s, 7683MB, speedup=1.96x, eff=98%]
  ✓ perf_500M_1Mbuckets (4T) [1.42s, 7683MB, speedup=3.35x, eff=84%]
  ✓ perf_500M_1Mbuckets (8T) [882.5ms, 7683MB, speedup=5.39x, eff=67%]
  ✓ perf_500M_1Mbuckets (16T) [735.6ms, 7684MB, speedup=6.46x, eff=40%]
--- Summary: Correctness ---
  Tests: 30/30 passed, 0 failed
  Max time: 93.0ms
  Peak RSS: 28MB
  Peak cgroup memory: 27MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |   107.0ms |   1.00x |   100% |     0.16s |    22MB |   6/ 6
          2 |    68.8ms |   1.56x |    78% |     0.19s |    23MB |   6/ 6
          4 |    41.4ms |   2.59x |    65% |     0.21s |    24MB |   6/ 6
          8 |    25.8ms |   4.16x |    52% |     0.24s |    25MB |   6/ 6
         16 |    21.8ms |   4.91x |    31% |     0.35s |    28MB |   6/ 6
--- Summary: Performance ---
  Tests: 5/5 passed, 0 failed
  Max time: 4.75s
  Peak RSS: 7684MB
  Peak cgroup memory: 7699MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |     4.75s |   1.00x |   100% |    10.14s |   7637MB |   1/ 1
          2 |     2.43s |   1.96x |    98% |     9.49s |   7683MB |   1/ 1
          4 |     1.42s |   3.35x |    84% |    10.06s |   7683MB |   1/ 1
          8 |   882.5ms |   5.39x |    67% |    10.62s |   7683MB |   1/ 1
         16 |   735.6ms |   6.46x |    40% |    13.25s |   7684MB |   1/ 1
```
