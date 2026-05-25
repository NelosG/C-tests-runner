# pbbs-examples/maximalIndependentSet

**Assignment:** pbbs maximalIndependentSet - 2 parlay variants
**Framework:** parlay
**Mode:** all
**Run status:** completed

Benchmark run on WSL (Ubuntu, parlay scheduler) at thread counts: correctness=[1, 2, 4, 8, 16], performance=[1, 2, 4, 8, 16].

## Results

```
--- Correctness: Correctness.NdMIS ---
  ✓ single_vertex (1T) [0.5ms, 5MB, speedup=1.00x, eff=100%]
  ✓ single_vertex (2T) [0.5ms, 5MB, speedup=0.93x, eff=46%]
  ✓ single_vertex (4T) [0.5ms, 5MB, speedup=0.99x, eff=25%]
  ✓ single_vertex (8T) [0.5ms, 5MB, speedup=0.98x, eff=12%]
  ✓ single_vertex (16T) [0.5ms, 5MB, speedup=0.89x, eff=6%]
  ✓ isolated_5 (1T) [0.5ms, 5MB, speedup=1.00x, eff=100%]
  ✓ isolated_5 (2T) [0.6ms, 5MB, speedup=0.93x, eff=46%]
  ✓ isolated_5 (4T) [0.6ms, 5MB, speedup=0.88x, eff=22%]
  ✓ isolated_5 (8T) [0.7ms, 5MB, speedup=0.75x, eff=9%]
  ✓ isolated_5 (16T) [0.8ms, 5MB, speedup=0.70x, eff=4%]
  ✓ single_edge (1T) [0.5ms, 5MB, speedup=1.00x, eff=100%]
  ✓ single_edge (2T) [0.5ms, 5MB, speedup=1.01x, eff=51%]
  ✓ single_edge (4T) [0.6ms, 5MB, speedup=0.96x, eff=24%]
  ✓ single_edge (8T) [0.6ms, 5MB, speedup=0.90x, eff=11%]
  ✓ single_edge (16T) [0.6ms, 5MB, speedup=0.93x, eff=6%]
  ✓ triangle (1T) [0.5ms, 5MB, speedup=1.00x, eff=100%]
  ✓ triangle (2T) [0.7ms, 5MB, speedup=0.82x, eff=41%]
  ✓ triangle (4T) [0.6ms, 5MB, speedup=0.88x, eff=22%]
  ✓ triangle (8T) [0.7ms, 5MB, speedup=0.75x, eff=9%]
  ✓ triangle (16T) [0.8ms, 5MB, speedup=0.69x, eff=4%]
  ✓ path_5 (1T) [0.7ms, 5MB, speedup=1.00x, eff=100%]
  ✓ path_5 (2T) [0.8ms, 5MB, speedup=0.94x, eff=47%]
  ✓ path_5 (4T) [0.7ms, 5MB, speedup=0.99x, eff=25%]
  ✓ path_5 (8T) [0.8ms, 5MB, speedup=0.92x, eff=12%]
  ✓ path_5 (16T) [0.9ms, 5MB, speedup=0.77x, eff=5%]
  ✓ star_5 (1T) [0.7ms, 5MB, speedup=1.00x, eff=100%]
  ✓ star_5 (2T) [0.7ms, 5MB, speedup=0.95x, eff=48%]
  ✓ star_5 (4T) [0.7ms, 5MB, speedup=0.97x, eff=24%]
  ✓ star_5 (8T) [0.9ms, 5MB, speedup=0.77x, eff=10%]
  ✓ star_5 (16T) [0.9ms, 5MB, speedup=0.75x, eff=5%]
  ✓ cycle_6 (1T) [0.6ms, 5MB, speedup=1.00x, eff=100%]
  ✓ cycle_6 (2T) [0.5ms, 5MB, speedup=1.14x, eff=57%]
  ✓ cycle_6 (4T) [0.6ms, 5MB, speedup=1.03x, eff=26%]
  ✓ cycle_6 (8T) [0.7ms, 5MB, speedup=0.92x, eff=12%]
  ✓ cycle_6 (16T) [0.8ms, 5MB, speedup=0.81x, eff=5%]
  ✓ random_100_300 (1T) [0.6ms, 5MB, speedup=1.00x, eff=100%]
  ✓ random_100_300 (2T) [0.8ms, 5MB, speedup=0.86x, eff=43%]
  ✓ random_100_300 (4T) [0.6ms, 5MB, speedup=1.05x, eff=26%]
  ✓ random_100_300 (8T) [0.7ms, 5MB, speedup=0.97x, eff=12%]
  ✓ random_100_300 (16T) [0.8ms, 5MB, speedup=0.80x, eff=5%]
  ✓ random_1k_3k (1T) [1.3ms, 5MB, speedup=1.00x, eff=100%]
  ✓ random_1k_3k (2T) [1.0ms, 5MB, speedup=1.24x, eff=62%]
  ✓ random_1k_3k (4T) [1.0ms, 5MB, speedup=1.35x, eff=34%]
  ✓ random_1k_3k (8T) [0.9ms, 5MB, speedup=1.52x, eff=19%]
  ✓ random_1k_3k (16T) [1.0ms, 5MB, speedup=1.26x, eff=8%]
  ✓ random_10k_30k (1T) [7.7ms, 6MB, speedup=1.00x, eff=100%]
  ✓ random_10k_30k (2T) [4.9ms, 7MB, speedup=1.57x, eff=79%]
  ✓ random_10k_30k (4T) [3.5ms, 6MB, speedup=2.19x, eff=55%]
  ✓ random_10k_30k (8T) [2.7ms, 6MB, speedup=2.87x, eff=36%]
  ✓ random_10k_30k (16T) [4.1ms, 6MB, speedup=1.90x, eff=12%]
--- Correctness: Correctness.IncrementalMIS ---
  ✓ single_vertex (1T) [0.7ms, 5MB, speedup=1.00x, eff=100%]
  ✓ single_vertex (2T) [0.6ms, 5MB, speedup=1.05x, eff=53%]
  ✓ single_vertex (4T) [0.6ms, 5MB, speedup=1.05x, eff=26%]
  ✓ single_vertex (8T) [0.6ms, 5MB, speedup=1.03x, eff=13%]
  ✓ single_vertex (16T) [0.7ms, 5MB, speedup=0.98x, eff=6%]
  ✓ isolated_5 (1T) [0.6ms, 5MB, speedup=1.00x, eff=100%]
  ✓ isolated_5 (2T) [0.6ms, 5MB, speedup=0.93x, eff=47%]
  ✓ isolated_5 (4T) [0.6ms, 5MB, speedup=0.98x, eff=24%]
  ✓ isolated_5 (8T) [0.6ms, 5MB, speedup=0.99x, eff=12%]
  ✓ isolated_5 (16T) [0.7ms, 5MB, speedup=0.86x, eff=5%]
  ✓ single_edge (1T) [0.6ms, 5MB, speedup=1.00x, eff=100%]
  ✓ single_edge (2T) [0.6ms, 5MB, speedup=1.06x, eff=53%]
  ✓ single_edge (4T) [0.6ms, 5MB, speedup=1.03x, eff=26%]
  ✓ single_edge (8T) [0.6ms, 5MB, speedup=0.95x, eff=12%]
  ✓ single_edge (16T) [0.6ms, 5MB, speedup=0.97x, eff=6%]
  ✓ triangle (1T) [0.6ms, 5MB, speedup=1.00x, eff=100%]
  ✓ triangle (2T) [0.6ms, 5MB, speedup=1.03x, eff=52%]
  ✓ triangle (4T) [0.6ms, 5MB, speedup=0.98x, eff=25%]
  ✓ triangle (8T) [0.6ms, 5MB, speedup=0.97x, eff=12%]
  ✓ triangle (16T) [0.6ms, 5MB, speedup=0.95x, eff=6%]
  ✓ path_5 (1T) [0.7ms, 5MB, speedup=1.00x, eff=100%]
  ✓ path_5 (2T) [0.7ms, 5MB, speedup=1.05x, eff=53%]
  ✓ path_5 (4T) [0.8ms, 5MB, speedup=0.87x, eff=22%]
  ✓ path_5 (8T) [0.8ms, 5MB, speedup=0.85x, eff=11%]
  ✓ path_5 (16T) [0.8ms, 5MB, speedup=0.91x, eff=6%]
  ✓ star_5 (1T) [0.7ms, 5MB, speedup=1.00x, eff=100%]
  ✓ star_5 (2T) [0.7ms, 5MB, speedup=0.99x, eff=50%]
  ✓ star_5 (4T) [0.7ms, 5MB, speedup=1.02x, eff=26%]
  ✓ star_5 (8T) [0.7ms, 5MB, speedup=1.00x, eff=12%]
  ✓ star_5 (16T) [0.7ms, 5MB, speedup=0.95x, eff=6%]
  ✓ cycle_6 (1T) [0.7ms, 5MB, speedup=1.00x, eff=100%]
  ✓ cycle_6 (2T) [0.7ms, 5MB, speedup=1.01x, eff=50%]
  ✓ cycle_6 (4T) [0.7ms, 5MB, speedup=0.92x, eff=23%]
  ✓ cycle_6 (8T) [0.7ms, 5MB, speedup=0.97x, eff=12%]
  ✓ cycle_6 (16T) [0.7ms, 5MB, speedup=0.93x, eff=6%]
  ✓ random_100_300 (1T) [1.1ms, 6MB, speedup=1.00x, eff=100%]
  ✓ random_100_300 (2T) [1.0ms, 6MB, speedup=1.04x, eff=52%]
  ✓ random_100_300 (4T) [1.1ms, 6MB, speedup=0.95x, eff=24%]
  ✓ random_100_300 (8T) [1.1ms, 6MB, speedup=0.94x, eff=12%]
  ✓ random_100_300 (16T) [1.2ms, 6MB, speedup=0.86x, eff=5%]
  ✓ random_1k_3k (1T) [1.8ms, 6MB, speedup=1.00x, eff=100%]
  ✓ random_1k_3k (2T) [1.7ms, 6MB, speedup=1.10x, eff=55%]
  ✓ random_1k_3k (4T) [1.5ms, 6MB, speedup=1.19x, eff=30%]
  ✓ random_1k_3k (8T) [1.6ms, 6MB, speedup=1.13x, eff=14%]
  ✓ random_1k_3k (16T) [1.8ms, 6MB, speedup=1.01x, eff=6%]
  ✓ random_10k_30k (1T) [7.1ms, 8MB, speedup=1.00x, eff=100%]
  ✓ random_10k_30k (2T) [5.2ms, 8MB, speedup=1.37x, eff=69%]
  ✓ random_10k_30k (4T) [4.1ms, 8MB, speedup=1.76x, eff=44%]
  ✓ random_10k_30k (8T) [3.7ms, 8MB, speedup=1.94x, eff=24%]
  ✓ random_10k_30k (16T) [4.2ms, 8MB, speedup=1.70x, eff=11%]
--- Performance: Performance.NdMIS ---
  T1=490.3ms  Tp=78.9ms  Speedup=6.21x  Efficiency=39%
  ✓ perf_8M_50M (1T) [490.3ms, 3299MB, speedup=1.00x, eff=100%]
  ✓ perf_8M_50M (2T) [248.1ms, 3299MB, speedup=1.98x, eff=99%]
  ✓ perf_8M_50M (4T) [139.0ms, 3299MB, speedup=3.53x, eff=88%]
  ✓ perf_8M_50M (8T) [86.8ms, 3299MB, speedup=5.65x, eff=71%]
  ✓ perf_8M_50M (16T) [78.9ms, 3299MB, speedup=6.21x, eff=39%]
--- Performance: Performance.IncrementalMIS ---
  T1=420.4ms  Tp=68.8ms  Speedup=6.11x  Efficiency=38%
  ✓ perf_8M_50M (1T) [420.4ms, 3299MB, speedup=1.00x, eff=100%]
  ✓ perf_8M_50M (2T) [224.4ms, 3299MB, speedup=1.87x, eff=94%]
  ✓ perf_8M_50M (4T) [139.2ms, 3299MB, speedup=3.02x, eff=76%]
  ✓ perf_8M_50M (8T) [96.4ms, 3299MB, speedup=4.36x, eff=55%]
  ✓ perf_8M_50M (16T) [68.8ms, 3299MB, speedup=6.11x, eff=38%]
--- Summary: Correctness ---
  Tests: 100/100 passed, 0 failed
  Max time: 7.7ms
  Peak RSS: 8MB
  Peak cgroup memory: 8MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |    28.3ms |   1.00x |   100% |     0.10s |     8MB |  20/20
          2 |    23.3ms |   1.22x |    61% |     0.10s |     8MB |  20/20
          4 |    20.7ms |   1.37x |    34% |     0.12s |     8MB |  20/20
          8 |    20.1ms |   1.40x |    18% |     0.18s |     8MB |  20/20
         16 |    23.2ms |   1.22x |     8% |     0.32s |     8MB |  20/20
--- Summary: Performance ---
  Tests: 10/10 passed, 0 failed
  Max time: 490.3ms
  Peak RSS: 3299MB
  Peak cgroup memory: 3306MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |   910.7ms |   1.00x |   100% |     8.21s |   3299MB |   2/ 2
          2 |   472.5ms |   1.93x |    96% |     8.11s |   3299MB |   2/ 2
          4 |   278.2ms |   3.27x |    82% |     8.69s |   3299MB |   2/ 2
          8 |   183.2ms |   4.97x |    62% |     8.50s |   3299MB |   2/ 2
         16 |   147.7ms |   6.16x |    39% |     9.01s |   3299MB |   2/ 2
```
