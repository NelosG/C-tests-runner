# pbbs-examples/breadthFirstSearch

**Assignment:** pbbs breadthFirstSearch - 3 parlay variants
**Framework:** parlay
**Mode:** all
**Run status:** completed

Benchmark run on WSL (Ubuntu, parlay scheduler) at thread counts: correctness=[1, 2, 4, 8, 16], performance=[1, 2, 4, 8, 16].

## Results

```
--- Correctness: Correctness.SimpleBFS ---
  ✓ single_vertex (1T) [0.8ms, 6MB, speedup=1.00x, eff=100%]
  ✓ single_vertex (2T) [0.7ms, 6MB, speedup=1.06x, eff=53%]
  ✓ single_vertex (4T) [0.8ms, 6MB, speedup=0.99x, eff=25%]
  ✓ single_vertex (8T) [0.7ms, 6MB, speedup=1.03x, eff=13%]
  ✓ single_vertex (16T) [0.8ms, 6MB, speedup=0.98x, eff=6%]
  ✓ single_edge (1T) [0.8ms, 6MB, speedup=1.00x, eff=100%]
  ✓ single_edge (2T) [0.9ms, 7MB, speedup=0.91x, eff=45%]
  ✓ single_edge (4T) [0.8ms, 7MB, speedup=1.01x, eff=25%]
  ✓ single_edge (8T) [0.8ms, 7MB, speedup=1.02x, eff=13%]
  ✓ single_edge (16T) [0.9ms, 7MB, speedup=0.97x, eff=6%]
  ✓ triangle (1T) [1.1ms, 7MB, speedup=1.00x, eff=100%]
  ✓ triangle (2T) [1.0ms, 7MB, speedup=1.01x, eff=51%]
  ✓ triangle (4T) [1.1ms, 7MB, speedup=0.97x, eff=24%]
  ✓ triangle (8T) [1.1ms, 7MB, speedup=0.92x, eff=11%]
  ✓ triangle (16T) [1.2ms, 7MB, speedup=0.85x, eff=5%]
  ✓ path_5 (1T) [0.9ms, 6MB, speedup=1.00x, eff=100%]
  ✓ path_5 (2T) [0.9ms, 7MB, speedup=0.96x, eff=48%]
  ✓ path_5 (4T) [0.9ms, 6MB, speedup=1.01x, eff=25%]
  ✓ path_5 (8T) [0.9ms, 6MB, speedup=0.93x, eff=12%]
  ✓ path_5 (16T) [1.0ms, 6MB, speedup=0.86x, eff=5%]
  ✓ disconnected (1T) [0.8ms, 6MB, speedup=1.00x, eff=100%]
  ✓ disconnected (2T) [0.8ms, 7MB, speedup=1.05x, eff=53%]
  ✓ disconnected (4T) [0.9ms, 6MB, speedup=0.95x, eff=24%]
  ✓ disconnected (8T) [0.9ms, 6MB, speedup=0.94x, eff=12%]
  ✓ disconnected (16T) [1.0ms, 6MB, speedup=0.81x, eff=5%]
  ✓ random_100 (1T) [1.9ms, 8MB, speedup=1.00x, eff=100%]
  ✓ random_100 (2T) [1.6ms, 8MB, speedup=1.17x, eff=59%]
  ✓ random_100 (4T) [1.6ms, 8MB, speedup=1.15x, eff=29%]
  ✓ random_100 (8T) [1.7ms, 8MB, speedup=1.13x, eff=14%]
  ✓ random_100 (16T) [1.9ms, 8MB, speedup=0.99x, eff=6%]
  ✓ random_1k (1T) [3.2ms, 8MB, speedup=1.00x, eff=100%]
  ✓ random_1k (2T) [2.8ms, 9MB, speedup=1.16x, eff=58%]
  ✓ random_1k (4T) [2.9ms, 8MB, speedup=1.12x, eff=28%]
  ✓ random_1k (8T) [3.0ms, 8MB, speedup=1.08x, eff=13%]
  ✓ random_1k (16T) [3.3ms, 9MB, speedup=0.99x, eff=6%]
  ✓ random_10k (1T) [16.0ms, 11MB, speedup=1.00x, eff=100%]
  ✓ random_10k (2T) [11.4ms, 13MB, speedup=1.41x, eff=70%]
  ✓ random_10k (4T) [8.8ms, 15MB, speedup=1.81x, eff=45%]
  ✓ random_10k (8T) [7.7ms, 16MB, speedup=2.07x, eff=26%]
  ✓ random_10k (16T) [7.8ms, 17MB, speedup=2.06x, eff=13%]
--- Correctness: Correctness.DeterministicBFS ---
  ✓ single_vertex (1T) [0.7ms, 6MB, speedup=1.00x, eff=100%]
  ✓ single_vertex (2T) [0.7ms, 6MB, speedup=0.96x, eff=48%]
  ✓ single_vertex (4T) [0.7ms, 6MB, speedup=0.94x, eff=23%]
  ✓ single_vertex (8T) [0.8ms, 6MB, speedup=0.88x, eff=11%]
  ✓ single_vertex (16T) [0.8ms, 6MB, speedup=0.85x, eff=5%]
  ✓ single_edge (1T) [0.7ms, 6MB, speedup=1.00x, eff=100%]
  ✓ single_edge (2T) [0.7ms, 6MB, speedup=0.90x, eff=45%]
  ✓ single_edge (4T) [0.7ms, 6MB, speedup=0.92x, eff=23%]
  ✓ single_edge (8T) [0.8ms, 6MB, speedup=0.84x, eff=10%]
  ✓ single_edge (16T) [0.8ms, 6MB, speedup=0.83x, eff=5%]
  ✓ triangle (1T) [0.8ms, 6MB, speedup=1.00x, eff=100%]
  ✓ triangle (2T) [0.9ms, 7MB, speedup=0.97x, eff=49%]
  ✓ triangle (4T) [0.9ms, 7MB, speedup=0.96x, eff=24%]
  ✓ triangle (8T) [1.0ms, 6MB, speedup=0.86x, eff=11%]
  ✓ triangle (16T) [1.1ms, 6MB, speedup=0.76x, eff=5%]
  ✓ path_5 (1T) [0.8ms, 6MB, speedup=1.00x, eff=100%]
  ✓ path_5 (2T) [0.9ms, 7MB, speedup=0.88x, eff=44%]
  ✓ path_5 (4T) [0.8ms, 6MB, speedup=0.97x, eff=24%]
  ✓ path_5 (8T) [1.0ms, 7MB, speedup=0.80x, eff=10%]
  ✓ path_5 (16T) [1.3ms, 6MB, speedup=0.64x, eff=4%]
  ✓ disconnected (1T) [0.8ms, 6MB, speedup=1.00x, eff=100%]
  ✓ disconnected (2T) [0.9ms, 7MB, speedup=0.92x, eff=46%]
  ✓ disconnected (4T) [0.8ms, 6MB, speedup=0.96x, eff=24%]
  ✓ disconnected (8T) [0.9ms, 6MB, speedup=0.85x, eff=11%]
  ✓ disconnected (16T) [1.0ms, 6MB, speedup=0.76x, eff=5%]
  ✓ random_100 (1T) [1.5ms, 8MB, speedup=1.00x, eff=100%]
  ✓ random_100 (2T) [1.6ms, 8MB, speedup=0.96x, eff=48%]
  ✓ random_100 (4T) [1.7ms, 8MB, speedup=0.88x, eff=22%]
  ✓ random_100 (8T) [1.7ms, 8MB, speedup=0.89x, eff=11%]
  ✓ random_100 (16T) [2.0ms, 8MB, speedup=0.78x, eff=5%]
  ✓ random_1k (1T) [3.1ms, 8MB, speedup=1.00x, eff=100%]
  ✓ random_1k (2T) [2.7ms, 9MB, speedup=1.15x, eff=58%]
  ✓ random_1k (4T) [2.6ms, 9MB, speedup=1.22x, eff=30%]
  ✓ random_1k (8T) [2.4ms, 8MB, speedup=1.32x, eff=16%]
  ✓ random_1k (16T) [2.8ms, 8MB, speedup=1.13x, eff=7%]
  ✓ random_10k (1T) [16.8ms, 11MB, speedup=1.00x, eff=100%]
  ✓ random_10k (2T) [10.6ms, 10MB, speedup=1.58x, eff=79%]
  ✓ random_10k (4T) [8.5ms, 10MB, speedup=1.96x, eff=49%]
  ✓ random_10k (8T) [6.2ms, 10MB, speedup=2.71x, eff=34%]
  ✓ random_10k (16T) [6.2ms, 10MB, speedup=2.69x, eff=17%]
--- Correctness: Correctness.BackForwardBFS ---
  ✓ single_vertex (1T) [0.6ms, 6MB, speedup=1.00x, eff=100%]
  ✓ single_vertex (2T) [0.7ms, 6MB, speedup=0.89x, eff=44%]
  ✓ single_vertex (4T) [0.6ms, 6MB, speedup=1.04x, eff=26%]
  ✓ single_vertex (8T) [0.7ms, 6MB, speedup=0.89x, eff=11%]
  ✓ single_vertex (16T) [0.6ms, 6MB, speedup=0.97x, eff=6%]
  ✓ single_edge (1T) [0.7ms, 6MB, speedup=1.00x, eff=100%]
  ✓ single_edge (2T) [0.7ms, 6MB, speedup=0.93x, eff=47%]
  ✓ single_edge (4T) [0.7ms, 6MB, speedup=0.99x, eff=25%]
  ✓ single_edge (8T) [0.8ms, 6MB, speedup=0.88x, eff=11%]
  ✓ single_edge (16T) [0.7ms, 6MB, speedup=0.95x, eff=6%]
  ✓ triangle (1T) [0.7ms, 6MB, speedup=1.00x, eff=100%]
  ✓ triangle (2T) [0.7ms, 6MB, speedup=1.02x, eff=51%]
  ✓ triangle (4T) [0.7ms, 6MB, speedup=0.90x, eff=22%]
  ✓ triangle (8T) [0.8ms, 6MB, speedup=0.82x, eff=10%]
  ✓ triangle (16T) [0.9ms, 6MB, speedup=0.76x, eff=5%]
  ✓ path_5 (1T) [0.8ms, 6MB, speedup=1.00x, eff=100%]
  ✓ path_5 (2T) [0.8ms, 7MB, speedup=1.03x, eff=52%]
  ✓ path_5 (4T) [0.8ms, 7MB, speedup=0.98x, eff=25%]
  ✓ path_5 (8T) [0.9ms, 6MB, speedup=0.85x, eff=11%]
  ✓ path_5 (16T) [1.0ms, 6MB, speedup=0.75x, eff=5%]
  ✓ disconnected (1T) [0.8ms, 6MB, speedup=1.00x, eff=100%]
  ✓ disconnected (2T) [0.7ms, 7MB, speedup=1.01x, eff=51%]
  ✓ disconnected (4T) [0.9ms, 7MB, speedup=0.87x, eff=22%]
  ✓ disconnected (8T) [1.0ms, 7MB, speedup=0.77x, eff=10%]
  ✓ disconnected (16T) [1.0ms, 6MB, speedup=0.74x, eff=5%]
  ✓ random_100 (1T) [1.3ms, 7MB, speedup=1.00x, eff=100%]
  ✓ random_100 (2T) [1.4ms, 7MB, speedup=0.93x, eff=47%]
  ✓ random_100 (4T) [1.3ms, 7MB, speedup=0.98x, eff=24%]
  ✓ random_100 (8T) [1.4ms, 7MB, speedup=0.92x, eff=12%]
  ✓ random_100 (16T) [1.5ms, 7MB, speedup=0.84x, eff=5%]
  ✓ random_1k (1T) [2.2ms, 7MB, speedup=1.00x, eff=100%]
  ✓ random_1k (2T) [1.9ms, 8MB, speedup=1.17x, eff=58%]
  ✓ random_1k (4T) [1.9ms, 8MB, speedup=1.16x, eff=29%]
  ✓ random_1k (8T) [2.0ms, 7MB, speedup=1.11x, eff=14%]
  ✓ random_1k (16T) [2.0ms, 7MB, speedup=1.11x, eff=7%]
  ✓ random_10k (1T) [9.4ms, 10MB, speedup=1.00x, eff=100%]
  ✓ random_10k (2T) [6.8ms, 10MB, speedup=1.38x, eff=69%]
  ✓ random_10k (4T) [5.7ms, 10MB, speedup=1.63x, eff=41%]
  ✓ random_10k (8T) [4.6ms, 10MB, speedup=2.02x, eff=25%]
  ✓ random_10k (16T) [4.8ms, 10MB, speedup=1.97x, eff=12%]
--- Performance: Performance.SimpleBFS ---
  T1=1.66s  Tp=145.4ms  Speedup=11.39x  Efficiency=71%
  ✓ perf_3M (1T) [1.66s, 1926MB, speedup=1.00x, eff=100%]
  ✓ perf_3M (2T) [687.5ms, 1926MB, speedup=2.41x, eff=120%]
  ✓ perf_3M (4T) [313.9ms, 1926MB, speedup=5.28x, eff=132%]
  ✓ perf_3M (8T) [178.4ms, 1926MB, speedup=9.28x, eff=116%]
  ✓ perf_3M (16T) [145.4ms, 1926MB, speedup=11.39x, eff=71%]
--- Performance: Performance.DeterministicBFS ---
  T1=1.40s  Tp=246.0ms  Speedup=5.70x  Efficiency=36%
  ✓ perf_3M (1T) [1.40s, 2406MB, speedup=1.00x, eff=100%]
  ✓ perf_3M (2T) [800.5ms, 2406MB, speedup=1.75x, eff=88%]
  ✓ perf_3M (4T) [426.3ms, 2406MB, speedup=3.29x, eff=82%]
  ✓ perf_3M (8T) [334.6ms, 2405MB, speedup=4.19x, eff=52%]
  ✓ perf_3M (16T) [246.0ms, 2404MB, speedup=5.70x, eff=36%]
--- Performance: Performance.BackForwardBFS ---
  T1=328.3ms  Tp=60.9ms  Speedup=5.39x  Efficiency=34%
  ✓ perf_3M (1T) [328.3ms, 1926MB, speedup=1.00x, eff=100%]
  ✓ perf_3M (2T) [190.6ms, 1926MB, speedup=1.72x, eff=86%]
  ✓ perf_3M (4T) [104.1ms, 1926MB, speedup=3.15x, eff=79%]
  ✓ perf_3M (8T) [74.6ms, 1926MB, speedup=4.40x, eff=55%]
  ✓ perf_3M (16T) [60.9ms, 1926MB, speedup=5.39x, eff=34%]
--- Summary: Correctness ---
  Tests: 120/120 passed, 0 failed
  Max time: 16.8ms
  Peak RSS: 17MB
  Peak cgroup memory: 16MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |    67.0ms |   1.00x |   100% |     0.16s |    11MB |  24/24
          2 |    52.7ms |   1.27x |    64% |     0.18s |    13MB |  24/24
          4 |    47.2ms |   1.42x |    35% |     0.22s |    15MB |  24/24
          8 |    43.8ms |   1.53x |    19% |     0.32s |    16MB |  24/24
         16 |    46.4ms |   1.44x |     9% |     0.64s |    17MB |  24/24
--- Summary: Performance ---
  Tests: 15/15 passed, 0 failed
  Max time: 1.66s
  Peak RSS: 2406MB
  Peak cgroup memory: 2409MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |     3.39s |   1.00x |   100% |    10.30s |   2406MB |   3/ 3
          2 |     1.68s |   2.02x |   101% |     9.63s |   2406MB |   3/ 3
          4 |   844.3ms |   4.01x |   100% |     9.81s |   2406MB |   3/ 3
          8 |   587.6ms |   5.76x |    72% |    11.15s |   2405MB |   3/ 3
         16 |   452.3ms |   7.49x |    47% |    12.47s |   2404MB |   3/ 3
```
