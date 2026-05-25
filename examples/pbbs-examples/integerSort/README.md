# pbbs-examples/integerSort

**Assignment:** pbbs integerSort - parallel radix sort
**Framework:** parlay
**Mode:** all
**Run status:** completed

Benchmark run on WSL (Ubuntu, parlay scheduler) at thread counts: correctness=[1, 2, 4, 8, 16], performance=[1, 2, 4, 8, 16].

## Results

```
--- Correctness: Correctness.ParallelRadixSort ---
  ✓ single (1T) [0.7ms, 6MB, speedup=1.00x, eff=100%]
  ✓ single (2T) [0.7ms, 6MB, speedup=0.97x, eff=48%]
  ✓ single (4T) [0.7ms, 6MB, speedup=1.00x, eff=25%]
  ✓ single (8T) [0.7ms, 6MB, speedup=0.99x, eff=12%]
  ✓ single (16T) [0.7ms, 6MB, speedup=0.99x, eff=6%]
  ✓ sorted_small (1T) [0.6ms, 6MB, speedup=1.00x, eff=100%]
  ✓ sorted_small (2T) [0.5ms, 6MB, speedup=1.06x, eff=53%]
  ✓ sorted_small (4T) [0.5ms, 6MB, speedup=1.06x, eff=27%]
  ✓ sorted_small (8T) [0.6ms, 6MB, speedup=0.98x, eff=12%]
  ✓ sorted_small (16T) [0.5ms, 6MB, speedup=1.04x, eff=7%]
  ✓ already_sorted (1T) [0.8ms, 6MB, speedup=1.00x, eff=100%]
  ✓ already_sorted (2T) [0.5ms, 6MB, speedup=1.59x, eff=80%]
  ✓ already_sorted (4T) [0.6ms, 6MB, speedup=1.33x, eff=33%]
  ✓ already_sorted (8T) [0.5ms, 6MB, speedup=1.48x, eff=18%]
  ✓ already_sorted (16T) [0.5ms, 6MB, speedup=1.45x, eff=9%]
  ✓ reverse_sorted (1T) [0.5ms, 6MB, speedup=1.00x, eff=100%]
  ✓ reverse_sorted (2T) [0.5ms, 6MB, speedup=1.02x, eff=51%]
  ✓ reverse_sorted (4T) [0.5ms, 6MB, speedup=0.94x, eff=24%]
  ✓ reverse_sorted (8T) [0.5ms, 6MB, speedup=0.90x, eff=11%]
  ✓ reverse_sorted (16T) [0.6ms, 6MB, speedup=0.83x, eff=5%]
  ✓ all_duplicates (1T) [0.6ms, 6MB, speedup=1.00x, eff=100%]
  ✓ all_duplicates (2T) [0.6ms, 6MB, speedup=1.07x, eff=53%]
  ✓ all_duplicates (4T) [0.5ms, 6MB, speedup=1.28x, eff=32%]
  ✓ all_duplicates (8T) [0.5ms, 6MB, speedup=1.17x, eff=15%]
  ✓ all_duplicates (16T) [0.5ms, 6MB, speedup=1.18x, eff=7%]
  ✓ extreme_values (1T) [0.5ms, 6MB, speedup=1.00x, eff=100%]
  ✓ extreme_values (2T) [0.5ms, 6MB, speedup=1.04x, eff=52%]
  ✓ extreme_values (4T) [0.6ms, 6MB, speedup=0.97x, eff=24%]
  ✓ extreme_values (8T) [0.5ms, 6MB, speedup=1.02x, eff=13%]
  ✓ extreme_values (16T) [0.5ms, 6MB, speedup=0.98x, eff=6%]
  ✓ random_10k_24b (1T) [2.3ms, 6MB, speedup=1.00x, eff=100%]
  ✓ random_10k_24b (2T) [2.2ms, 6MB, speedup=1.05x, eff=52%]
  ✓ random_10k_24b (4T) [2.3ms, 6MB, speedup=0.98x, eff=24%]
  ✓ random_10k_24b (8T) [2.4ms, 6MB, speedup=0.97x, eff=12%]
  ✓ random_10k_24b (16T) [2.6ms, 6MB, speedup=0.88x, eff=5%]
  ✓ random_100k_24b (1T) [19.2ms, 8MB, speedup=1.00x, eff=100%]
  ✓ random_100k_24b (2T) [18.0ms, 8MB, speedup=1.07x, eff=53%]
  ✓ random_100k_24b (4T) [17.8ms, 8MB, speedup=1.08x, eff=27%]
  ✓ random_100k_24b (8T) [17.6ms, 8MB, speedup=1.09x, eff=14%]
  ✓ random_100k_24b (16T) [17.9ms, 8MB, speedup=1.07x, eff=7%]
  ✓ random_1M_24b (1T) [176.0ms, 32MB, speedup=1.00x, eff=100%]
  ✓ random_1M_24b (2T) [100.8ms, 33MB, speedup=1.75x, eff=87%]
  ✓ random_1M_24b (4T) [52.2ms, 34MB, speedup=3.37x, eff=84%]
  ✓ random_1M_24b (8T) [29.4ms, 36MB, speedup=5.99x, eff=75%]
  ✓ random_1M_24b (16T) [21.2ms, 39MB, speedup=8.29x, eff=52%]
--- Performance: Performance.ParallelRadixSort ---
  T1=7.63s  Tp=2.40s  Speedup=3.18x  Efficiency=20%
  ✓ perf_500M_24b (1T) [7.63s, 9541MB, speedup=1.00x, eff=100%]
  ✓ perf_500M_24b (2T) [4.43s, 9588MB, speedup=1.72x, eff=86%]
  ✓ perf_500M_24b (4T) [2.97s, 9590MB, speedup=2.57x, eff=64%]
  ✓ perf_500M_24b (8T) [2.39s, 9593MB, speedup=3.20x, eff=40%]
  ✓ perf_500M_24b (16T) [2.40s, 9600MB, speedup=3.18x, eff=20%]
--- Summary: Correctness ---
  Tests: 45/45 passed, 0 failed
  Max time: 176.0ms
  Peak RSS: 39MB
  Peak cgroup memory: 38MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |   201.1ms |   1.00x |   100% |     0.27s |    32MB |   9/ 9
          2 |   124.2ms |   1.62x |    81% |     0.30s |    33MB |   9/ 9
          4 |    75.7ms |   2.66x |    66% |     0.32s |    34MB |   9/ 9
          8 |    52.8ms |   3.81x |    48% |     0.39s |    36MB |   9/ 9
         16 |    45.2ms |   4.45x |    28% |     0.61s |    39MB |   9/ 9
--- Summary: Performance ---
  Tests: 5/5 passed, 0 failed
  Max time: 7.63s
  Peak RSS: 9600MB
  Peak cgroup memory: 9619MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |     7.63s |   1.00x |   100% |    18.66s |   9541MB |   1/ 1
          2 |     4.43s |   1.72x |    86% |    15.16s |   9588MB |   1/ 1
          4 |     2.97s |   2.57x |    64% |    15.93s |   9590MB |   1/ 1
          8 |     2.39s |   3.20x |    40% |    18.25s |   9593MB |   1/ 1
         16 |     2.40s |   3.18x |    20% |    24.81s |   9600MB |   1/ 1
```
