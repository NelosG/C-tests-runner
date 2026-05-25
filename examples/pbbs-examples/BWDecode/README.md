# pbbs-examples/BWDecode

**Assignment:** pbbs BWDecode - listRank variant
**Framework:** parlay
**Mode:** all
**Run status:** completed

Benchmark run on WSL (Ubuntu, parlay scheduler) at thread counts: correctness=[1, 2, 4, 8, 16], performance=[1, 2, 4, 8, 16].

## Results

```
--- Correctness: Correctness.ListRankBWDecode ---
  ✓ short_text (1T) [1.2ms, 6MB, speedup=1.00x, eff=100%]
  ✓ short_text (2T) [1.2ms, 7MB, speedup=0.96x, eff=48%]
  ✓ short_text (4T) [1.4ms, 7MB, speedup=0.88x, eff=22%]
  ✓ short_text (8T) [1.4ms, 7MB, speedup=0.84x, eff=10%]
  ✓ short_text (16T) [1.5ms, 7MB, speedup=0.80x, eff=5%]
  ✓ single_char (1T) [0.9ms, 6MB, speedup=1.00x, eff=100%]
  ✓ single_char (2T) [1.1ms, 6MB, speedup=0.79x, eff=39%]
  ✓ single_char (4T) [1.2ms, 6MB, speedup=0.74x, eff=19%]
  ✓ single_char (8T) [1.3ms, 6MB, speedup=0.70x, eff=9%]
  ✓ single_char (16T) [1.1ms, 6MB, speedup=0.82x, eff=5%]
  ✓ all_same (1T) [1.0ms, 6MB, speedup=1.00x, eff=100%]
  ✓ all_same (2T) [1.1ms, 6MB, speedup=0.87x, eff=43%]
  ✓ all_same (4T) [1.3ms, 7MB, speedup=0.75x, eff=19%]
  ✓ all_same (8T) [1.3ms, 7MB, speedup=0.75x, eff=9%]
  ✓ all_same (16T) [1.3ms, 6MB, speedup=0.73x, eff=5%]
  ✓ pangram (1T) [1.3ms, 7MB, speedup=1.00x, eff=100%]
  ✓ pangram (2T) [1.5ms, 7MB, speedup=0.88x, eff=44%]
  ✓ pangram (4T) [1.4ms, 7MB, speedup=0.94x, eff=24%]
  ✓ pangram (8T) [1.4ms, 7MB, speedup=0.93x, eff=12%]
  ✓ pangram (16T) [1.6ms, 7MB, speedup=0.85x, eff=5%]
  ✓ repeated_pattern (1T) [1.1ms, 6MB, speedup=1.00x, eff=100%]
  ✓ repeated_pattern (2T) [1.2ms, 7MB, speedup=0.89x, eff=45%]
  ✓ repeated_pattern (4T) [1.2ms, 7MB, speedup=0.93x, eff=23%]
  ✓ repeated_pattern (8T) [1.5ms, 7MB, speedup=0.72x, eff=9%]
  ✓ repeated_pattern (16T) [1.3ms, 7MB, speedup=0.82x, eff=5%]
  ✓ random_1k (1T) [1.3ms, 6MB, speedup=1.00x, eff=100%]
  ✓ random_1k (2T) [1.4ms, 7MB, speedup=0.88x, eff=44%]
  ✓ random_1k (4T) [1.4ms, 7MB, speedup=0.90x, eff=22%]
  ✓ random_1k (8T) [1.5ms, 7MB, speedup=0.84x, eff=11%]
  ✓ random_1k (16T) [1.5ms, 7MB, speedup=0.83x, eff=5%]
  ✓ random_10k (1T) [3.2ms, 7MB, speedup=1.00x, eff=100%]
  ✓ random_10k (2T) [2.7ms, 7MB, speedup=1.21x, eff=61%]
  ✓ random_10k (4T) [2.6ms, 7MB, speedup=1.24x, eff=31%]
  ✓ random_10k (8T) [2.8ms, 7MB, speedup=1.15x, eff=14%]
  ✓ random_10k (16T) [3.2ms, 7MB, speedup=1.02x, eff=6%]
  ✓ random_100k (1T) [19.8ms, 8MB, speedup=1.00x, eff=100%]
  ✓ random_100k (2T) [14.8ms, 9MB, speedup=1.33x, eff=67%]
  ✓ random_100k (4T) [9.4ms, 11MB, speedup=2.10x, eff=53%]
  ✓ random_100k (8T) [8.1ms, 13MB, speedup=2.44x, eff=31%]
  ✓ random_100k (16T) [8.8ms, 13MB, speedup=2.24x, eff=14%]
--- Performance: Performance.ListRankBWDecode ---
  T1=14.33s  Tp=1.12s  Speedup=12.84x  Efficiency=80%
  ✓ perf_100M (1T) [14.33s, 1631MB, speedup=1.00x, eff=100%]
  ✓ perf_100M (2T) [6.76s, 1650MB, speedup=2.12x, eff=106%]
  ✓ perf_100M (4T) [3.56s, 1654MB, speedup=4.02x, eff=101%]
  ✓ perf_100M (8T) [1.84s, 1660MB, speedup=7.80x, eff=98%]
  ✓ perf_100M (16T) [1.12s, 1677MB, speedup=12.84x, eff=80%]
--- Summary: Correctness ---
  Tests: 40/40 passed, 0 failed
  Max time: 19.8ms
  Peak RSS: 13MB
  Peak cgroup memory: 12MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |    29.8ms |   1.00x |   100% |     0.05s |     8MB |   8/ 8
          2 |    25.2ms |   1.18x |    59% |     0.06s |     9MB |   8/ 8
          4 |    19.9ms |   1.50x |    37% |     0.08s |    11MB |   8/ 8
          8 |    19.4ms |   1.54x |    19% |     0.13s |    13MB |   8/ 8
         16 |    20.4ms |   1.46x |     9% |     0.23s |    13MB |   8/ 8
--- Summary: Performance ---
  Tests: 5/5 passed, 0 failed
  Max time: 14.33s
  Peak RSS: 1677MB
  Peak cgroup memory: 1682MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |    14.33s |   1.00x |   100% |    14.93s |   1631MB |   1/ 1
          2 |     6.76s |   2.12x |   106% |    13.98s |   1650MB |   1/ 1
          4 |     3.56s |   4.02x |   101% |    14.46s |   1654MB |   1/ 1
          8 |     1.84s |   7.80x |    98% |    14.59s |   1660MB |   1/ 1
         16 |     1.12s |   12.84x |    80% |    15.77s |   1677MB |   1/ 1
```
