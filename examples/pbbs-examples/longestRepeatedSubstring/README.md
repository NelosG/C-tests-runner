# pbbs-examples/longestRepeatedSubstring

**Assignment:** pbbs longestRepeatedSubstring - doubling
**Framework:** parlay
**Mode:** all
**Run status:** completed

Benchmark run on WSL (Ubuntu, parlay scheduler) at thread counts: correctness=[1, 2, 4, 8, 16], performance=[1, 2, 4, 8, 16].

## Results

```
--- Correctness: Correctness.DoublingLRS ---
  ✓ banana (1T) [1.4ms, 8MB, speedup=1.00x, eff=100%]
  ✓ banana (2T) [1.3ms, 8MB, speedup=1.02x, eff=51%]
  ✓ banana (4T) [1.5ms, 8MB, speedup=0.95x, eff=24%]
  ✓ banana (8T) [1.6ms, 8MB, speedup=0.89x, eff=11%]
  ✓ banana (16T) [1.5ms, 8MB, speedup=0.91x, eff=6%]
  ✓ mississippi (1T) [1.6ms, 8MB, speedup=1.00x, eff=100%]
  ✓ mississippi (2T) [1.4ms, 8MB, speedup=1.14x, eff=57%]
  ✓ mississippi (4T) [1.5ms, 8MB, speedup=1.07x, eff=27%]
  ✓ mississippi (8T) [1.5ms, 8MB, speedup=1.04x, eff=13%]
  ✓ mississippi (16T) [1.7ms, 8MB, speedup=0.91x, eff=6%]
  ✓ planted_short (1T) [1.5ms, 8MB, speedup=1.00x, eff=100%]
  ✓ planted_short (2T) [1.4ms, 9MB, speedup=1.06x, eff=53%]
  ✓ planted_short (4T) [1.5ms, 8MB, speedup=0.98x, eff=25%]
  ✓ planted_short (8T) [1.8ms, 8MB, speedup=0.84x, eff=10%]
  ✓ planted_short (16T) [1.9ms, 8MB, speedup=0.79x, eff=5%]
  ✓ planted_long (1T) [1.6ms, 8MB, speedup=1.00x, eff=100%]
  ✓ planted_long (2T) [1.7ms, 8MB, speedup=0.92x, eff=46%]
  ✓ planted_long (4T) [1.8ms, 8MB, speedup=0.86x, eff=21%]
  ✓ planted_long (8T) [1.8ms, 8MB, speedup=0.88x, eff=11%]
  ✓ planted_long (16T) [1.9ms, 8MB, speedup=0.82x, eff=5%]
  ✓ random_1k (1T) [2.5ms, 8MB, speedup=1.00x, eff=100%]
  ✓ random_1k (2T) [2.6ms, 8MB, speedup=0.95x, eff=47%]
  ✓ random_1k (4T) [2.3ms, 8MB, speedup=1.06x, eff=26%]
  ✓ random_1k (8T) [2.5ms, 8MB, speedup=0.99x, eff=12%]
  ✓ random_1k (16T) [2.6ms, 8MB, speedup=0.96x, eff=6%]
  ✓ random_10k (1T) [12.5ms, 9MB, speedup=1.00x, eff=100%]
  ✓ random_10k (2T) [8.6ms, 9MB, speedup=1.46x, eff=73%]
  ✓ random_10k (4T) [6.8ms, 9MB, speedup=1.83x, eff=46%]
  ✓ random_10k (8T) [5.9ms, 8MB, speedup=2.11x, eff=26%]
  ✓ random_10k (16T) [6.7ms, 8MB, speedup=1.86x, eff=12%]
  ✓ random_100k (1T) [106.7ms, 12MB, speedup=1.00x, eff=100%]
  ✓ random_100k (2T) [64.4ms, 13MB, speedup=1.66x, eff=83%]
  ✓ random_100k (4T) [44.6ms, 12MB, speedup=2.39x, eff=60%]
  ✓ random_100k (8T) [33.7ms, 12MB, speedup=3.16x, eff=40%]
  ✓ random_100k (16T) [33.8ms, 10MB, speedup=3.16x, eff=20%]
--- Performance: Performance.DoublingLRS ---
  T1=5.41s  Tp=623.8ms  Speedup=8.67x  Efficiency=54%
  ✓ perf_25M (1T) [5.41s, 1190MB, speedup=1.00x, eff=100%]
  ✓ perf_25M (2T) [2.57s, 1194MB, speedup=2.10x, eff=105%]
  ✓ perf_25M (4T) [1.41s, 1197MB, speedup=3.84x, eff=96%]
  ✓ perf_25M (8T) [862.3ms, 1202MB, speedup=6.27x, eff=78%]
  ✓ perf_25M (16T) [623.8ms, 1212MB, speedup=8.67x, eff=54%]
--- Summary: Correctness ---
  Tests: 35/35 passed, 0 failed
  Max time: 106.7ms
  Peak RSS: 13MB
  Peak cgroup memory: 9MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |   127.6ms |   1.00x |   100% |     0.15s |    12MB |   7/ 7
          2 |    81.4ms |   1.57x |    78% |     0.17s |    13MB |   7/ 7
          4 |    60.0ms |   2.13x |    53% |     0.20s |    12MB |   7/ 7
          8 |    48.8ms |   2.62x |    33% |     0.30s |    12MB |   7/ 7
         16 |    50.1ms |   2.55x |    16% |     0.62s |    10MB |   7/ 7
--- Summary: Performance ---
  Tests: 5/5 passed, 0 failed
  Max time: 5.41s
  Peak RSS: 1212MB
  Peak cgroup memory: 1214MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |     5.41s |   1.00x |   100% |     5.53s |   1190MB |   1/ 1
          2 |     2.57s |   2.10x |   105% |     5.25s |   1194MB |   1/ 1
          4 |     1.41s |   3.84x |    96% |     5.68s |   1197MB |   1/ 1
          8 |   862.3ms |   6.27x |    78% |     6.81s |   1202MB |   1/ 1
         16 |   623.8ms |   8.67x |    54% |     9.10s |   1212MB |   1/ 1
```
