# pbbs-examples/spanningForest

**Assignment:** pbbs spanningForest - 2 parlay variants
**Framework:** parlay
**Mode:** all
**Run status:** completed

Benchmark run on WSL (Ubuntu, parlay scheduler) at thread counts: correctness=[1, 2, 4, 8, 16], performance=[1, 2, 4, 8, 16].

## Results

```
--- Correctness: Correctness.NdST ---
  ✓ tree_already (1T) [0.9ms, 5MB, speedup=1.00x, eff=100%]
  ✓ tree_already (2T) [0.8ms, 5MB, speedup=1.07x, eff=54%]
  ✓ tree_already (4T) [0.8ms, 5MB, speedup=1.05x, eff=26%]
  ✓ tree_already (8T) [0.9ms, 5MB, speedup=0.96x, eff=12%]
  ✓ tree_already (16T) [1.0ms, 5MB, speedup=0.87x, eff=5%]
  ✓ single_cycle (1T) [0.8ms, 5MB, speedup=1.00x, eff=100%]
  ✓ single_cycle (2T) [0.8ms, 5MB, speedup=0.98x, eff=49%]
  ✓ single_cycle (4T) [0.8ms, 5MB, speedup=1.00x, eff=25%]
  ✓ single_cycle (8T) [0.9ms, 5MB, speedup=0.95x, eff=12%]
  ✓ single_cycle (16T) [1.0ms, 5MB, speedup=0.83x, eff=5%]
  ✓ two_components (1T) [0.8ms, 5MB, speedup=1.00x, eff=100%]
  ✓ two_components (2T) [0.7ms, 5MB, speedup=1.12x, eff=56%]
  ✓ two_components (4T) [0.8ms, 5MB, speedup=1.08x, eff=27%]
  ✓ two_components (8T) [0.8ms, 5MB, speedup=0.99x, eff=12%]
  ✓ two_components (16T) [0.9ms, 5MB, speedup=0.89x, eff=6%]
  ✓ complete_4 (1T) [1.5ms, 5MB, speedup=1.00x, eff=100%]
  ✓ complete_4 (2T) [0.8ms, 5MB, speedup=1.88x, eff=94%]
  ✓ complete_4 (4T) [0.8ms, 5MB, speedup=1.83x, eff=46%]
  ✓ complete_4 (8T) [0.8ms, 5MB, speedup=1.75x, eff=22%]
  ✓ complete_4 (16T) [0.9ms, 5MB, speedup=1.61x, eff=10%]
  ✓ random_100 (1T) [1.0ms, 5MB, speedup=1.00x, eff=100%]
  ✓ random_100 (2T) [0.9ms, 6MB, speedup=1.12x, eff=56%]
  ✓ random_100 (4T) [1.0ms, 5MB, speedup=0.99x, eff=25%]
  ✓ random_100 (8T) [1.1ms, 6MB, speedup=0.93x, eff=12%]
  ✓ random_100 (16T) [1.1ms, 6MB, speedup=0.91x, eff=6%]
  ✓ random_1k (1T) [1.5ms, 5MB, speedup=1.00x, eff=100%]
  ✓ random_1k (2T) [1.2ms, 6MB, speedup=1.24x, eff=62%]
  ✓ random_1k (4T) [1.1ms, 5MB, speedup=1.36x, eff=34%]
  ✓ random_1k (8T) [1.2ms, 5MB, speedup=1.20x, eff=15%]
  ✓ random_1k (16T) [1.5ms, 5MB, speedup=1.03x, eff=6%]
  ✓ random_10k (1T) [7.0ms, 6MB, speedup=1.00x, eff=100%]
  ✓ random_10k (2T) [4.8ms, 7MB, speedup=1.45x, eff=73%]
  ✓ random_10k (4T) [3.4ms, 6MB, speedup=2.07x, eff=52%]
  ✓ random_10k (8T) [4.0ms, 7MB, speedup=1.75x, eff=22%]
  ✓ random_10k (16T) [2.6ms, 7MB, speedup=2.73x, eff=17%]
--- Correctness: Correctness.IncrementalST ---
  ✓ tree_already (1T) [0.9ms, 5MB, speedup=1.00x, eff=100%]
  ✓ tree_already (2T) [0.9ms, 5MB, speedup=1.02x, eff=51%]
  ✓ tree_already (4T) [0.9ms, 5MB, speedup=1.00x, eff=25%]
  ✓ tree_already (8T) [1.0ms, 5MB, speedup=0.94x, eff=12%]
  ✓ tree_already (16T) [1.2ms, 5MB, speedup=0.76x, eff=5%]
  ✓ single_cycle (1T) [0.9ms, 5MB, speedup=1.00x, eff=100%]
  ✓ single_cycle (2T) [0.9ms, 5MB, speedup=1.04x, eff=52%]
  ✓ single_cycle (4T) [0.8ms, 5MB, speedup=1.12x, eff=28%]
  ✓ single_cycle (8T) [0.9ms, 5MB, speedup=0.98x, eff=12%]
  ✓ single_cycle (16T) [1.2ms, 5MB, speedup=0.77x, eff=5%]
  ✓ two_components (1T) [0.9ms, 5MB, speedup=1.00x, eff=100%]
  ✓ two_components (2T) [0.9ms, 5MB, speedup=0.95x, eff=48%]
  ✓ two_components (4T) [0.9ms, 5MB, speedup=1.02x, eff=25%]
  ✓ two_components (8T) [1.0ms, 5MB, speedup=0.91x, eff=11%]
  ✓ two_components (16T) [1.0ms, 5MB, speedup=0.83x, eff=5%]
  ✓ complete_4 (1T) [0.8ms, 5MB, speedup=1.00x, eff=100%]
  ✓ complete_4 (2T) [0.8ms, 5MB, speedup=1.03x, eff=51%]
  ✓ complete_4 (4T) [0.9ms, 5MB, speedup=0.92x, eff=23%]
  ✓ complete_4 (8T) [0.9ms, 5MB, speedup=0.91x, eff=11%]
  ✓ complete_4 (16T) [1.0ms, 5MB, speedup=0.77x, eff=5%]
  ✓ random_100 (1T) [1.4ms, 6MB, speedup=1.00x, eff=100%]
  ✓ random_100 (2T) [1.2ms, 6MB, speedup=1.14x, eff=57%]
  ✓ random_100 (4T) [1.2ms, 6MB, speedup=1.13x, eff=28%]
  ✓ random_100 (8T) [1.3ms, 6MB, speedup=1.02x, eff=13%]
  ✓ random_100 (16T) [1.4ms, 6MB, speedup=1.00x, eff=6%]
  ✓ random_1k (1T) [2.4ms, 6MB, speedup=1.00x, eff=100%]
  ✓ random_1k (2T) [2.3ms, 6MB, speedup=1.07x, eff=53%]
  ✓ random_1k (4T) [2.2ms, 6MB, speedup=1.10x, eff=28%]
  ✓ random_1k (8T) [3.1ms, 6MB, speedup=0.79x, eff=10%]
  ✓ random_1k (16T) [2.6ms, 6MB, speedup=0.94x, eff=6%]
  ✓ random_10k (1T) [12.3ms, 8MB, speedup=1.00x, eff=100%]
  ✓ random_10k (2T) [8.9ms, 8MB, speedup=1.39x, eff=69%]
  ✓ random_10k (4T) [6.8ms, 8MB, speedup=1.81x, eff=45%]
  ✓ random_10k (8T) [6.2ms, 8MB, speedup=1.99x, eff=25%]
  ✓ random_10k (16T) [7.3ms, 8MB, speedup=1.70x, eff=11%]
--- Performance: Performance.NdST ---
  T1=1.03s  Tp=144.2ms  Speedup=7.15x  Efficiency=45%
  ✓ perf_8M_50M (1T) [1.03s, 3055MB, speedup=1.00x, eff=100%]
  ✓ perf_8M_50M (2T) [511.8ms, 3055MB, speedup=2.02x, eff=101%]
  ✓ perf_8M_50M (4T) [279.1ms, 3055MB, speedup=3.70x, eff=92%]
  ✓ perf_8M_50M (8T) [179.8ms, 3055MB, speedup=5.74x, eff=72%]
  ✓ perf_8M_50M (16T) [144.2ms, 3055MB, speedup=7.15x, eff=45%]
--- Performance: Performance.IncrementalST ---
  T1=1.97s  Tp=325.0ms  Speedup=6.07x  Efficiency=38%
  ✓ perf_8M_50M (1T) [1.97s, 3055MB, speedup=1.00x, eff=100%]
  ✓ perf_8M_50M (2T) [1.07s, 3055MB, speedup=1.84x, eff=92%]
  ✓ perf_8M_50M (4T) [602.4ms, 3055MB, speedup=3.27x, eff=82%]
  ✓ perf_8M_50M (8T) [399.4ms, 3055MB, speedup=4.94x, eff=62%]
  ✓ perf_8M_50M (16T) [325.0ms, 3055MB, speedup=6.07x, eff=38%]
--- Summary: Correctness ---
  Tests: 70/70 passed, 0 failed
  Max time: 12.3ms
  Peak RSS: 8MB
  Peak cgroup memory: 8MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |    33.1ms |   1.00x |   100% |     0.09s |     8MB |  14/14
          2 |    25.9ms |   1.28x |    64% |     0.10s |     8MB |  14/14
          4 |    22.4ms |   1.48x |    37% |     0.12s |     8MB |  14/14
          8 |    24.2ms |   1.37x |    17% |     0.20s |     8MB |  14/14
         16 |    24.7ms |   1.34x |     8% |     0.38s |     8MB |  14/14
--- Summary: Performance ---
  Tests: 10/10 passed, 0 failed
  Max time: 1.97s
  Peak RSS: 3055MB
  Peak cgroup memory: 3060MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |     3.00s |   1.00x |   100% |    10.32s |   3055MB |   2/ 2
          2 |     1.58s |   1.90x |    95% |     9.78s |   3055MB |   2/ 2
          4 |   881.5ms |   3.41x |    85% |     9.89s |   3055MB |   2/ 2
          8 |   579.2ms |   5.19x |    65% |    11.41s |   3055MB |   2/ 2
         16 |   469.2ms |   6.40x |    40% |    13.59s |   3055MB |   2/ 2
```
