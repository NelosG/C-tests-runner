# pbbs-examples/maximalMatching

**Assignment:** pbbs maximalMatching - 2 parlay variants
**Framework:** parlay
**Mode:** all
**Run status:** completed

Benchmark run on WSL (Ubuntu, parlay scheduler) at thread counts: correctness=[1, 2, 4, 8, 16], performance=[1, 2, 4, 8, 16].

## Results

```
--- Correctness: Correctness.NdMatching ---
  ✓ single_edge (1T) [0.5ms, 5MB, speedup=1.00x, eff=100%]
  ✓ single_edge (2T) [0.5ms, 5MB, speedup=1.00x, eff=50%]
  ✓ single_edge (4T) [0.5ms, 5MB, speedup=1.03x, eff=26%]
  ✓ single_edge (8T) [0.5ms, 5MB, speedup=1.04x, eff=13%]
  ✓ single_edge (16T) [0.5ms, 5MB, speedup=0.98x, eff=6%]
  ✓ triangle (1T) [0.6ms, 5MB, speedup=1.00x, eff=100%]
  ✓ triangle (2T) [0.6ms, 5MB, speedup=0.99x, eff=49%]
  ✓ triangle (4T) [0.7ms, 5MB, speedup=0.96x, eff=24%]
  ✓ triangle (8T) [0.8ms, 5MB, speedup=0.83x, eff=10%]
  ✓ triangle (16T) [0.8ms, 5MB, speedup=0.76x, eff=5%]
  ✓ path_5 (1T) [0.6ms, 5MB, speedup=1.00x, eff=100%]
  ✓ path_5 (2T) [0.6ms, 5MB, speedup=0.95x, eff=47%]
  ✓ path_5 (4T) [0.6ms, 5MB, speedup=0.95x, eff=24%]
  ✓ path_5 (8T) [0.7ms, 5MB, speedup=0.89x, eff=11%]
  ✓ path_5 (16T) [0.8ms, 5MB, speedup=0.77x, eff=5%]
  ✓ star_5 (1T) [0.6ms, 5MB, speedup=1.00x, eff=100%]
  ✓ star_5 (2T) [0.7ms, 5MB, speedup=0.78x, eff=39%]
  ✓ star_5 (4T) [0.6ms, 5MB, speedup=0.89x, eff=22%]
  ✓ star_5 (8T) [0.7ms, 5MB, speedup=0.78x, eff=10%]
  ✓ star_5 (16T) [0.8ms, 5MB, speedup=0.72x, eff=5%]
  ✓ two_paths (1T) [0.6ms, 5MB, speedup=1.00x, eff=100%]
  ✓ two_paths (2T) [0.6ms, 5MB, speedup=0.91x, eff=45%]
  ✓ two_paths (4T) [0.6ms, 5MB, speedup=0.94x, eff=24%]
  ✓ two_paths (8T) [0.7ms, 5MB, speedup=0.80x, eff=10%]
  ✓ two_paths (16T) [0.9ms, 5MB, speedup=0.64x, eff=4%]
  ✓ random_100 (1T) [0.7ms, 5MB, speedup=1.00x, eff=100%]
  ✓ random_100 (2T) [0.8ms, 6MB, speedup=0.90x, eff=45%]
  ✓ random_100 (4T) [0.8ms, 5MB, speedup=0.97x, eff=24%]
  ✓ random_100 (8T) [0.8ms, 6MB, speedup=0.91x, eff=11%]
  ✓ random_100 (16T) [1.0ms, 5MB, speedup=0.73x, eff=5%]
  ✓ random_1k (1T) [1.6ms, 6MB, speedup=1.00x, eff=100%]
  ✓ random_1k (2T) [1.1ms, 6MB, speedup=1.44x, eff=72%]
  ✓ random_1k (4T) [1.1ms, 6MB, speedup=1.47x, eff=37%]
  ✓ random_1k (8T) [1.2ms, 6MB, speedup=1.39x, eff=17%]
  ✓ random_1k (16T) [1.4ms, 6MB, speedup=1.15x, eff=7%]
  ✓ random_10k (1T) [5.3ms, 6MB, speedup=1.00x, eff=100%]
  ✓ random_10k (2T) [3.6ms, 7MB, speedup=1.49x, eff=75%]
  ✓ random_10k (4T) [2.8ms, 6MB, speedup=1.92x, eff=48%]
  ✓ random_10k (8T) [2.3ms, 7MB, speedup=2.33x, eff=29%]
  ✓ random_10k (16T) [2.4ms, 6MB, speedup=2.18x, eff=14%]
--- Correctness: Correctness.IncrementalMatching ---
  ✓ single_edge (1T) [0.5ms, 5MB, speedup=1.00x, eff=100%]
  ✓ single_edge (2T) [0.5ms, 5MB, speedup=1.00x, eff=50%]
  ✓ single_edge (4T) [0.6ms, 5MB, speedup=0.94x, eff=24%]
  ✓ single_edge (8T) [0.5ms, 5MB, speedup=0.99x, eff=12%]
  ✓ single_edge (16T) [0.6ms, 5MB, speedup=0.91x, eff=6%]
  ✓ triangle (1T) [0.6ms, 5MB, speedup=1.00x, eff=100%]
  ✓ triangle (2T) [0.7ms, 5MB, speedup=0.92x, eff=46%]
  ✓ triangle (4T) [0.7ms, 5MB, speedup=0.94x, eff=23%]
  ✓ triangle (8T) [0.7ms, 5MB, speedup=0.87x, eff=11%]
  ✓ triangle (16T) [0.9ms, 5MB, speedup=0.73x, eff=5%]
  ✓ path_5 (1T) [0.7ms, 5MB, speedup=1.00x, eff=100%]
  ✓ path_5 (2T) [0.8ms, 5MB, speedup=0.91x, eff=46%]
  ✓ path_5 (4T) [0.8ms, 5MB, speedup=0.93x, eff=23%]
  ✓ path_5 (8T) [0.9ms, 5MB, speedup=0.84x, eff=10%]
  ✓ path_5 (16T) [1.0ms, 5MB, speedup=0.76x, eff=5%]
  ✓ star_5 (1T) [0.7ms, 5MB, speedup=1.00x, eff=100%]
  ✓ star_5 (2T) [0.8ms, 5MB, speedup=0.89x, eff=44%]
  ✓ star_5 (4T) [0.9ms, 5MB, speedup=0.85x, eff=21%]
  ✓ star_5 (8T) [0.8ms, 5MB, speedup=0.89x, eff=11%]
  ✓ star_5 (16T) [0.9ms, 5MB, speedup=0.80x, eff=5%]
  ✓ two_paths (1T) [0.7ms, 5MB, speedup=1.00x, eff=100%]
  ✓ two_paths (2T) [0.8ms, 5MB, speedup=0.94x, eff=47%]
  ✓ two_paths (4T) [0.8ms, 5MB, speedup=0.93x, eff=23%]
  ✓ two_paths (8T) [0.9ms, 5MB, speedup=0.84x, eff=10%]
  ✓ two_paths (16T) [0.9ms, 5MB, speedup=0.78x, eff=5%]
  ✓ random_100 (1T) [1.3ms, 6MB, speedup=1.00x, eff=100%]
  ✓ random_100 (2T) [1.5ms, 6MB, speedup=0.89x, eff=44%]
  ✓ random_100 (4T) [1.2ms, 6MB, speedup=1.05x, eff=26%]
  ✓ random_100 (8T) [1.4ms, 6MB, speedup=0.95x, eff=12%]
  ✓ random_100 (16T) [1.6ms, 6MB, speedup=0.83x, eff=5%]
  ✓ random_1k (1T) [2.5ms, 7MB, speedup=1.00x, eff=100%]
  ✓ random_1k (2T) [2.3ms, 7MB, speedup=1.06x, eff=53%]
  ✓ random_1k (4T) [2.3ms, 7MB, speedup=1.08x, eff=27%]
  ✓ random_1k (8T) [2.4ms, 7MB, speedup=1.04x, eff=13%]
  ✓ random_1k (16T) [2.6ms, 7MB, speedup=0.95x, eff=6%]
  ✓ random_10k (1T) [12.6ms, 8MB, speedup=1.00x, eff=100%]
  ✓ random_10k (2T) [9.5ms, 9MB, speedup=1.32x, eff=66%]
  ✓ random_10k (4T) [7.5ms, 8MB, speedup=1.67x, eff=42%]
  ✓ random_10k (8T) [6.6ms, 9MB, speedup=1.91x, eff=24%]
  ✓ random_10k (16T) [8.9ms, 9MB, speedup=1.41x, eff=9%]
--- Performance: Performance.NdMatching ---
  T1=398.8ms  Tp=104.9ms  Speedup=3.80x  Efficiency=24%
  ✓ perf_8M_50M (1T) [398.8ms, 3055MB, speedup=1.00x, eff=100%]
  ✓ perf_8M_50M (2T) [218.0ms, 3055MB, speedup=1.83x, eff=91%]
  ✓ perf_8M_50M (4T) [127.5ms, 3055MB, speedup=3.13x, eff=78%]
  ✓ perf_8M_50M (8T) [93.9ms, 3055MB, speedup=4.25x, eff=53%]
  ✓ perf_8M_50M (16T) [104.9ms, 3055MB, speedup=3.80x, eff=24%]
--- Performance: Performance.IncrementalMatching ---
  T1=1.57s  Tp=444.3ms  Speedup=3.54x  Efficiency=22%
  ✓ perf_8M_50M (1T) [1.57s, 3055MB, speedup=1.00x, eff=100%]
  ✓ perf_8M_50M (2T) [935.1ms, 3055MB, speedup=1.68x, eff=84%]
  ✓ perf_8M_50M (4T) [629.4ms, 3055MB, speedup=2.50x, eff=62%]
  ✓ perf_8M_50M (8T) [461.6ms, 3055MB, speedup=3.40x, eff=43%]
  ✓ perf_8M_50M (16T) [444.3ms, 3055MB, speedup=3.54x, eff=22%]
--- Summary: Correctness ---
  Tests: 80/80 passed, 0 failed
  Max time: 12.6ms
  Peak RSS: 9MB
  Peak cgroup memory: 9MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |    30.3ms |   1.00x |   100% |     0.08s |     8MB |  16/16
          2 |    25.6ms |   1.18x |    59% |     0.10s |     9MB |  16/16
          4 |    22.4ms |   1.35x |    34% |     0.12s |     8MB |  16/16
          8 |    21.8ms |   1.39x |    17% |     0.18s |     9MB |  16/16
         16 |    26.1ms |   1.16x |     7% |     0.41s |     9MB |  16/16
--- Summary: Performance ---
  Tests: 10/10 passed, 0 failed
  Max time: 1.57s
  Peak RSS: 3055MB
  Peak cgroup memory: 3060MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |     1.97s |   1.00x |   100% |     8.91s |   3055MB |   2/ 2
          2 |     1.15s |   1.71x |    85% |     8.58s |   3055MB |   2/ 2
          4 |   756.9ms |   2.60x |    65% |     8.37s |   3055MB |   2/ 2
          8 |   555.5ms |   3.55x |    44% |     9.74s |   3055MB |   2/ 2
         16 |   549.3ms |   3.59x |    22% |    12.02s |   3055MB |   2/ 2
```
