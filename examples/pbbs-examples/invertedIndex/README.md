# pbbs-examples/invertedIndex

**Assignment:** pbbs invertedIndex - parallel variant
**Framework:** parlay
**Mode:** all
**Run status:** completed

Benchmark run on WSL (Ubuntu, parlay scheduler) at thread counts: correctness=[1, 2, 4, 8, 16], performance=[1, 2, 4, 8, 16].

## Results

```
--- Correctness: Correctness.ParallelBuildIndex ---
  ✓ two_docs_simple (1T) [1.6ms, 8MB, speedup=1.00x, eff=100%]
  ✓ two_docs_simple (2T) [1.5ms, 8MB, speedup=1.03x, eff=52%]
  ✓ two_docs_simple (4T) [2.0ms, 10MB, speedup=0.77x, eff=19%]
  ✓ two_docs_simple (8T) [2.0ms, 9MB, speedup=0.78x, eff=10%]
  ✓ two_docs_simple (16T) [2.3ms, 9MB, speedup=0.70x, eff=4%]
  ✓ three_docs_with_repeats (1T) [1.5ms, 8MB, speedup=1.00x, eff=100%]
  ✓ three_docs_with_repeats (2T) [1.5ms, 8MB, speedup=1.01x, eff=51%]
  ✓ three_docs_with_repeats (4T) [2.6ms, 11MB, speedup=0.58x, eff=14%]
  ✓ three_docs_with_repeats (8T) [2.9ms, 11MB, speedup=0.53x, eff=7%]
  ✓ three_docs_with_repeats (16T) [3.1ms, 12MB, speedup=0.49x, eff=3%]
  ✓ mixed_case_punctuation (1T) [1.5ms, 8MB, speedup=1.00x, eff=100%]
  ✓ mixed_case_punctuation (2T) [1.5ms, 8MB, speedup=0.97x, eff=48%]
  ✓ mixed_case_punctuation (4T) [2.5ms, 11MB, speedup=0.58x, eff=15%]
  ✓ mixed_case_punctuation (8T) [2.5ms, 11MB, speedup=0.60x, eff=8%]
  ✓ mixed_case_punctuation (16T) [3.1ms, 10MB, speedup=0.47x, eff=3%]
  ✓ random_10_docs (1T) [3.2ms, 9MB, speedup=1.00x, eff=100%]
  ✓ random_10_docs (2T) [3.2ms, 11MB, speedup=0.99x, eff=50%]
  ✓ random_10_docs (4T) [3.5ms, 14MB, speedup=0.92x, eff=23%]
  ✓ random_10_docs (8T) [4.8ms, 20MB, speedup=0.66x, eff=8%]
  ✓ random_10_docs (16T) [8.4ms, 26MB, speedup=0.38x, eff=2%]
  ✓ random_50_docs (1T) [11.6ms, 9MB, speedup=1.00x, eff=100%]
  ✓ random_50_docs (2T) [7.9ms, 11MB, speedup=1.46x, eff=73%]
  ✓ random_50_docs (4T) [6.2ms, 15MB, speedup=1.88x, eff=47%]
  ✓ random_50_docs (8T) [7.6ms, 23MB, speedup=1.53x, eff=19%]
  ✓ random_50_docs (16T) [12.1ms, 38MB, speedup=0.96x, eff=6%]
  ✓ random_200_docs (1T) [52.9ms, 11MB, speedup=1.00x, eff=100%]
  ✓ random_200_docs (2T) [30.6ms, 13MB, speedup=1.73x, eff=86%]
  ✓ random_200_docs (4T) [19.2ms, 16MB, speedup=2.75x, eff=69%]
  ✓ random_200_docs (8T) [16.0ms, 25MB, speedup=3.31x, eff=41%]
  ✓ random_200_docs (16T) [18.8ms, 39MB, speedup=2.82x, eff=18%]
--- Performance: Performance.ParallelBuildIndex ---
  T1=7.25s  Tp=704.2ms  Speedup=10.30x  Efficiency=64%
  ✓ perf_15k_docs (1T) [7.25s, 1403MB, speedup=1.00x, eff=100%]
  ✓ perf_15k_docs (2T) [3.53s, 1407MB, speedup=2.06x, eff=103%]
  ✓ perf_15k_docs (4T) [1.83s, 1412MB, speedup=3.97x, eff=99%]
  ✓ perf_15k_docs (8T) [964.4ms, 1422MB, speedup=7.52x, eff=94%]
  ✓ perf_15k_docs (16T) [704.2ms, 1440MB, speedup=10.30x, eff=64%]
--- Summary: Correctness ---
  Tests: 30/30 passed, 0 failed
  Max time: 52.9ms
  Peak RSS: 39MB
  Peak cgroup memory: 38MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |    72.3ms |   1.00x |   100% |     0.09s |    11MB |   6/ 6
          2 |    46.4ms |   1.56x |    78% |     0.10s |    13MB |   6/ 6
          4 |    36.1ms |   2.00x |    50% |     0.13s |    16MB |   6/ 6
          8 |    35.8ms |   2.02x |    25% |     0.22s |    25MB |   6/ 6
         16 |    47.8ms |   1.51x |     9% |     0.49s |    39MB |   6/ 6
--- Summary: Performance ---
  Tests: 5/5 passed, 0 failed
  Max time: 7.25s
  Peak RSS: 1440MB
  Peak cgroup memory: 1444MB
  Scalability:
    Threads | Time      | Speedup | Eff   | CPU       | Memory  | Compared
    --------|-----------|---------|-------|-----------|---------|---------
          1 |     7.25s |   1.00x |   100% |     7.54s |   1403MB |   1/ 1
          2 |     3.53s |   2.06x |   103% |     7.25s |   1407MB |   1/ 1
          4 |     1.83s |   3.97x |    99% |     7.41s |   1412MB |   1/ 1
          8 |   964.4ms |   7.52x |    94% |     7.70s |   1422MB |   1/ 1
         16 |   704.2ms |   10.30x |    64% |     8.76s |   1440MB |   1/ 1
```
