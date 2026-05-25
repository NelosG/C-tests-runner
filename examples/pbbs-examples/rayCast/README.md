# pbbs-examples/rayCast

**Assignment:** pbbs rayCast - parallel ray cast
**Framework:** parlay
**Mode:** all
**Run status:** completed

Benchmark run on WSL (Ubuntu, parlay scheduler) at thread counts: correctness=[1, 2, 4, 8, 16], performance=[1, 2, 4, 8, 16].

## Results

```
--- Correctness: Correctness.KdTreeRayCast ---
  ✓ 50t_20r (1T) [1.4ms, 7MB, speedup=1.00x, eff=100%]
  ✓ 50t_20r (2T) [1.3ms, 7MB, speedup=1.08x, eff=54%]
  ✓ 50t_20r (4T) [1.3ms, 7MB, speedup=1.06x, eff=26%]
  ✓ 50t_20r (8T) [1.3ms, 7MB, speedup=1.13x, eff=14%]
  ✓ 50t_20r (16T) [1.4ms, 7MB, speedup=1.02x, eff=6%]
  ✓ 200t_50r (1T) [3.1ms, 7MB, speedup=1.00x, eff=100%]
  ✓ 200t_50r (2T) [2.3ms, 7MB, speedup=1.34x, eff=67%]
  ✓ 200t_50r (4T) [1.9ms, 7MB, speedup=1.69x, eff=42%]
  ✓ 200t_50r (8T) [1.5ms, 7MB, speedup=2.06x, eff=26%]
  ✓ 200t_50r (16T) [1.7ms, 7MB, speedup=1.88x, eff=12%]
  ✓ 1k_100r (1T) [23.4ms, 8MB, speedup=1.00x, eff=100%]
  ✓ 1k_100r (2T) [13.8ms, 8MB, speedup=1.70x, eff=85%]
  ✓ 1k_100r (4T) [9.1ms, 8MB, speedup=2.58x, eff=64%]
  ✓ 1k_100r (8T) [7.1ms, 8MB, speedup=3.29x, eff=41%]
  ✓ 1k_100r (16T) [6.8ms, 8MB, speedup=3.46x, eff=22%]
--- Performance: Performance.KdTreeRayCast ---
  T1=3.76s  Tp=284.8ms  Speedup=13.20x  Efficiency=82%
  ✓ perf_30k_5k (1T) [3.76s, 22MB, speedup=1.00x, eff=100%]
  ✓ perf_30k_5k (2T) [1.93s, 23MB, speedup=1.95x, eff=98%]
  ✓ perf_30k_5k (4T) [971.4ms, 26MB, speedup=3.87x, eff=97%]
  ✓ perf_30k_5k (8T) [499.1ms, 30MB, speedup=7.53x, eff=94%]
  ✓ perf_30k_5k (16T) [284.8ms, 38MB, speedup=13.20x, eff=82%]
--- Summary: Correctness ---
  Tests: 15/15 passed, 0 failed
  Max time: 23.4ms
  Peak RSS: 8MB
  Peak cgroup memory: 7MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |    27.9ms |   1.00x |   100% |     0.04s |     8MB |   3/ 3
          2 |    17.4ms |   1.60x |    80% |     0.05s |     8MB |   3/ 3
          4 |    12.3ms |   2.28x |    57% |     0.05s |     8MB |   3/ 3
          8 |     9.9ms |   2.83x |    35% |     0.07s |     8MB |   3/ 3
         16 |     9.8ms |   2.85x |    18% |     0.14s |     8MB |   3/ 3
--- Summary: Performance ---
  Tests: 5/5 passed, 0 failed
  Max time: 3.76s
  Peak RSS: 38MB
  Peak cgroup memory: 39MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |     3.76s |   1.00x |   100% |     3.77s |    22MB |   1/ 1
          2 |     1.93s |   1.95x |    98% |     3.85s |    23MB |   1/ 1
          4 |   971.4ms |   3.87x |    97% |     3.88s |    26MB |   1/ 1
          8 |   499.1ms |   7.53x |    94% |     3.98s |    30MB |   1/ 1
         16 |   284.8ms |   13.20x |    82% |     4.22s |    38MB |   1/ 1
```
