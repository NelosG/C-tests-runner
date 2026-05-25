# pbbs-examples/wordCounts

**Assignment:** pbbs wordCounts - histogram variant
**Framework:** parlay
**Mode:** all
**Run status:** completed

Benchmark run on WSL (Ubuntu, parlay scheduler) at thread counts: correctness=[1, 2, 4, 8, 16], performance=[1, 2, 4, 8, 16].

## Results

```
--- Correctness: Correctness.HistogramWordCounts ---
  ✓ single_word (1T) [1.0ms, 6MB, speedup=1.00x, eff=100%]
  ✓ single_word (2T) [1.0ms, 6MB, speedup=1.00x, eff=50%]
  ✓ single_word (4T) [1.0ms, 6MB, speedup=0.94x, eff=24%]
  ✓ single_word (8T) [1.0ms, 6MB, speedup=0.94x, eff=12%]
  ✓ single_word (16T) [1.2ms, 6MB, speedup=0.82x, eff=5%]
  ✓ two_words (1T) [1.0ms, 6MB, speedup=1.00x, eff=100%]
  ✓ two_words (2T) [1.0ms, 6MB, speedup=1.03x, eff=52%]
  ✓ two_words (4T) [1.1ms, 6MB, speedup=0.89x, eff=22%]
  ✓ two_words (8T) [1.1ms, 6MB, speedup=0.89x, eff=11%]
  ✓ two_words (16T) [1.3ms, 6MB, speedup=0.78x, eff=5%]
  ✓ repeated_words (1T) [1.1ms, 6MB, speedup=1.00x, eff=100%]
  ✓ repeated_words (2T) [1.0ms, 7MB, speedup=1.09x, eff=54%]
  ✓ repeated_words (4T) [1.2ms, 7MB, speedup=0.93x, eff=23%]
  ✓ repeated_words (8T) [1.2ms, 6MB, speedup=0.95x, eff=12%]
  ✓ repeated_words (16T) [1.3ms, 6MB, speedup=0.85x, eff=5%]
  ✓ mixed_case (1T) [1.0ms, 6MB, speedup=1.00x, eff=100%]
  ✓ mixed_case (2T) [1.0ms, 7MB, speedup=0.99x, eff=50%]
  ✓ mixed_case (4T) [1.2ms, 6MB, speedup=0.84x, eff=21%]
  ✓ mixed_case (8T) [1.2ms, 6MB, speedup=0.86x, eff=11%]
  ✓ mixed_case (16T) [1.3ms, 6MB, speedup=0.77x, eff=5%]
  ✓ punctuation (1T) [1.0ms, 6MB, speedup=1.00x, eff=100%]
  ✓ punctuation (2T) [0.9ms, 6MB, speedup=1.01x, eff=50%]
  ✓ punctuation (4T) [1.0ms, 6MB, speedup=0.96x, eff=24%]
  ✓ punctuation (8T) [1.1ms, 6MB, speedup=0.88x, eff=11%]
  ✓ punctuation (16T) [1.2ms, 6MB, speedup=0.78x, eff=5%]
  ✓ only_separators (1T) [0.8ms, 6MB, speedup=1.00x, eff=100%]
  ✓ only_separators (2T) [0.8ms, 6MB, speedup=0.99x, eff=49%]
  ✓ only_separators (4T) [0.9ms, 6MB, speedup=0.84x, eff=21%]
  ✓ only_separators (8T) [0.9ms, 6MB, speedup=0.86x, eff=11%]
  ✓ only_separators (16T) [1.2ms, 6MB, speedup=0.62x, eff=4%]
  ✓ random_1k (1T) [1.5ms, 7MB, speedup=1.00x, eff=100%]
  ✓ random_1k (2T) [1.5ms, 7MB, speedup=0.96x, eff=48%]
  ✓ random_1k (4T) [1.7ms, 8MB, speedup=0.86x, eff=21%]
  ✓ random_1k (8T) [2.0ms, 8MB, speedup=0.76x, eff=9%]
  ✓ random_1k (16T) [2.6ms, 7MB, speedup=0.56x, eff=4%]
  ✓ random_10k (1T) [5.5ms, 7MB, speedup=1.00x, eff=100%]
  ✓ random_10k (2T) [4.6ms, 7MB, speedup=1.20x, eff=60%]
  ✓ random_10k (4T) [4.0ms, 8MB, speedup=1.36x, eff=34%]
  ✓ random_10k (8T) [4.2ms, 8MB, speedup=1.31x, eff=16%]
  ✓ random_10k (16T) [5.6ms, 8MB, speedup=0.98x, eff=6%]
  ✓ random_100k (1T) [45.1ms, 9MB, speedup=1.00x, eff=100%]
  ✓ random_100k (2T) [28.8ms, 12MB, speedup=1.56x, eff=78%]
  ✓ random_100k (4T) [20.6ms, 14MB, speedup=2.19x, eff=55%]
  ✓ random_100k (8T) [17.1ms, 19MB, speedup=2.64x, eff=33%]
  ✓ random_100k (16T) [21.8ms, 29MB, speedup=2.07x, eff=13%]
--- Performance: Performance.HistogramWordCounts ---
  T1=3.69s  Tp=819.1ms  Speedup=4.50x  Efficiency=28%
  ✓ perf_70M (1T) [3.69s, 1494MB, speedup=1.00x, eff=100%]
  ✓ perf_70M (2T) [2.10s, 1497MB, speedup=1.76x, eff=88%]
  ✓ perf_70M (4T) [1.37s, 1504MB, speedup=2.70x, eff=68%]
  ✓ perf_70M (8T) [815.6ms, 1511MB, speedup=4.52x, eff=57%]
  ✓ perf_70M (16T) [819.1ms, 1525MB, speedup=4.50x, eff=28%]
--- Summary: Correctness ---
  Tests: 45/45 passed, 0 failed
  Max time: 45.1ms
  Peak RSS: 29MB
  Peak cgroup memory: 28MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |    57.9ms |   1.00x |   100% |     0.09s |     9MB |   9/ 9
          2 |    40.6ms |   1.42x |    71% |     0.10s |    12MB |   9/ 9
          4 |    32.8ms |   1.76x |    44% |     0.13s |    14MB |   9/ 9
          8 |    29.7ms |   1.95x |    24% |     0.21s |    19MB |   9/ 9
         16 |    37.5ms |   1.54x |    10% |     0.50s |    29MB |   9/ 9
--- Summary: Performance ---
  Tests: 5/5 passed, 0 failed
  Max time: 3.69s
  Peak RSS: 1525MB
  Peak cgroup memory: 1528MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |     3.69s |   1.00x |   100% |     4.64s |   1494MB |   1/ 1
          2 |     2.10s |   1.76x |    88% |     4.69s |   1497MB |   1/ 1
          4 |     1.37s |   2.70x |    68% |     4.97s |   1504MB |   1/ 1
          8 |   815.6ms |   4.52x |    57% |     4.71s |   1511MB |   1/ 1
         16 |   819.1ms |   4.50x |    28% |     6.02s |   1525MB |   1/ 1
```
