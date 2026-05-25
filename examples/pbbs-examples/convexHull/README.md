# pbbs-examples/convexHull

**Assignment:** pbbs convexHull - quickHull
**Framework:** parlay
**Mode:** all
**Run status:** completed

Benchmark run on WSL (Ubuntu, parlay scheduler) at thread counts: correctness=[1, 2, 4, 8, 16], performance=[1, 2, 4, 8, 16].

## Results

```
--- Correctness: Correctness.ParallelQuickHull ---
  ✓ triangle (1T) [0.8ms, 5MB, speedup=1.00x, eff=100%]
  ✓ triangle (2T) [0.8ms, 6MB, speedup=0.98x, eff=49%]
  ✓ triangle (4T) [0.8ms, 5MB, speedup=0.99x, eff=25%]
  ✓ triangle (8T) [0.9ms, 5MB, speedup=0.92x, eff=11%]
  ✓ triangle (16T) [1.2ms, 5MB, speedup=0.71x, eff=4%]
  ✓ square (1T) [0.7ms, 5MB, speedup=1.00x, eff=100%]
  ✓ square (2T) [0.8ms, 6MB, speedup=0.86x, eff=43%]
  ✓ square (4T) [0.8ms, 6MB, speedup=0.88x, eff=22%]
  ✓ square (8T) [0.8ms, 5MB, speedup=0.85x, eff=11%]
  ✓ square (16T) [1.0ms, 6MB, speedup=0.69x, eff=4%]
  ✓ square_w_interior (1T) [0.8ms, 5MB, speedup=1.00x, eff=100%]
  ✓ square_w_interior (2T) [0.8ms, 5MB, speedup=0.97x, eff=48%]
  ✓ square_w_interior (4T) [0.7ms, 6MB, speedup=1.02x, eff=26%]
  ✓ square_w_interior (8T) [0.8ms, 5MB, speedup=0.93x, eff=12%]
  ✓ square_w_interior (16T) [1.1ms, 5MB, speedup=0.66x, eff=4%]
  ✓ random_1k (1T) [2.0ms, 7MB, speedup=1.00x, eff=100%]
  ✓ random_1k (2T) [1.9ms, 8MB, speedup=1.06x, eff=53%]
  ✓ random_1k (4T) [2.0ms, 8MB, speedup=1.03x, eff=26%]
  ✓ random_1k (8T) [1.8ms, 8MB, speedup=1.15x, eff=14%]
  ✓ random_1k (16T) [2.4ms, 8MB, speedup=0.85x, eff=5%]
  ✓ random_10k (1T) [6.4ms, 7MB, speedup=1.00x, eff=100%]
  ✓ random_10k (2T) [4.4ms, 9MB, speedup=1.48x, eff=74%]
  ✓ random_10k (4T) [3.6ms, 10MB, speedup=1.80x, eff=45%]
  ✓ random_10k (8T) [3.1ms, 10MB, speedup=2.06x, eff=26%]
  ✓ random_10k (16T) [3.1ms, 10MB, speedup=2.07x, eff=13%]
  ✓ random_100k (1T) [52.2ms, 14MB, speedup=1.00x, eff=100%]
  ✓ random_100k (2T) [28.2ms, 16MB, speedup=1.85x, eff=92%]
  ✓ random_100k (4T) [17.2ms, 18MB, speedup=3.03x, eff=76%]
  ✓ random_100k (8T) [10.9ms, 18MB, speedup=4.80x, eff=60%]
  ✓ random_100k (16T) [9.2ms, 17MB, speedup=5.66x, eff=35%]
--- Performance: Performance.ParallelQuickHull ---
  T1=3.10s  Tp=689.1ms  Speedup=4.51x  Efficiency=28%
  ✓ perf_100M (1T) [3.10s, 6938MB, speedup=1.00x, eff=100%]
  ✓ perf_100M (2T) [1.91s, 7007MB, speedup=1.63x, eff=81%]
  ✓ perf_100M (4T) [1.09s, 7030MB, speedup=2.86x, eff=71%]
  ✓ perf_100M (8T) [614.2ms, 7030MB, speedup=5.06x, eff=63%]
  ✓ perf_100M (16T) [689.1ms, 7034MB, speedup=4.51x, eff=28%]
--- Summary: Correctness ---
  Tests: 30/30 passed, 0 failed
  Max time: 52.2ms
  Peak RSS: 18MB
  Peak cgroup memory: 17MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |    62.9ms |   1.00x |   100% |     0.11s |    14MB |   6/ 6
          2 |    36.9ms |   1.70x |    85% |     0.12s |    16MB |   6/ 6
          4 |    25.2ms |   2.50x |    63% |     0.13s |    18MB |   6/ 6
          8 |    18.3ms |   3.44x |    43% |     0.16s |    18MB |   6/ 6
         16 |    18.0ms |   3.49x |    22% |     0.29s |    17MB |   6/ 6
--- Summary: Performance ---
  Tests: 5/5 passed, 0 failed
  Max time: 3.10s
  Peak RSS: 7034MB
  Peak cgroup memory: 7048MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |     3.10s |   1.00x |   100% |    10.60s |   6938MB |   1/ 1
          2 |     1.91s |   1.63x |    81% |    10.47s |   7007MB |   1/ 1
          4 |     1.09s |   2.86x |    71% |     9.96s |   7030MB |   1/ 1
          8 |   614.2ms |   5.06x |    63% |    10.20s |   7030MB |   1/ 1
         16 |   689.1ms |   4.51x |    28% |    14.97s |   7034MB |   1/ 1
```
