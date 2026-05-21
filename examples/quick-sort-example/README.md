# quick-sort-example - parallel quicksort, three solutions

Sample assignment: implement a parallel `qsort` over `std::vector<long long>`.
The same teacher tests run against solutions written in three different
parallel frameworks.

`tests/config.json` declares all three allowed frameworks:
```
"allowedFrameworks": ["openmp", "parlay", "cilk"]
```

Available solutions:

| Directory           | Framework detected | How                                                            |
|---------------------|--------------------|----------------------------------------------------------------|
| `solution/`         | `openmp`           | links `parallel_lib`, uses `OMP_*` macros from `par/pragma.h`  |
| `solution-omp-raw/` | `openmp`           | raw `#pragma omp task/single/parallel`, `find_package(OpenMP)` |
| `solution-parlay/`  | `parlay`           | `parlay::par_do` recursive split, header-only                  |
| `solution-cilk/`    | `cilk`             | `cilk_spawn` / `cilk_sync`, `-fopencilk`                       |

All solutions implement the same interface:
```cpp
namespace parallel {
    void qsort(std::vector<long long>& array);
}
```

Pick one solution at submission time by pointing `solutionSource` at the
appropriate directory. The engine auto-detects the framework from that
solution's `CMakeLists.txt` and ensures it is in the allowed list.

The `tests/` directory contains:
- `src/*.cpp` - C++ test plugins (correctness, edge cases, stability, scalability)
- `cases/*.json` - JSON test scenarios loaded by `JsonScenarioLoader`
- `runner/main.cpp` - thin wrapper that reads `array`, calls `parallel::qsort`,
  writes `result`
