# cloned-example-multi - single assignment, four solutions

This example demonstrates running the **same teacher tests** against
solutions implemented in different parallel frameworks.

`tests/config.json` declares all three allowed frameworks:
```
"allowedFrameworks": ["openmp", "parlay", "cilk"]
```

Available solutions:

| Directory                | Framework detected | How                                                |
|--------------------------|--------------------|----------------------------------------------------|
| `solution-omp-raw/`      | `openmp`           | `find_package(OpenMP)` + `#pragma omp` directly    |
| `solution-parallel-lib/` | `openmp`           | links `parallel_lib`, uses `OMP_*` macros          |
| `solution-parlay/`       | `parlay`           | parlaylib header-only, declared via comment marker |
| `solution-cilk/`         | `cilk`             | `target_compile_options(... -fopencilk)`           |

> Raw `<omp.h>` is allowed because this assignment's `allowedPackages` lists
> `OpenMP`. If the teacher wants to force `parallel_lib`-only solutions, drop
> `OpenMP` from `allowedPackages` - the engine then activates `shadow_omp` and
> any direct `#include <omp.h>` fails to compile. Note that with raw OMP, the
> engine's `parallelStats` counters stay at zero, since they are populated by
> `parallel_lib`'s hooks.

All solutions implement the same interface:
```cpp
namespace parallel {
    void scan(const std::vector<long long>& array, std::vector<long long>& result);
}
```

Pick one solution at submission time by pointing `solutionSource` at the
appropriate directory. The engine auto-detects the framework from that
solution's `CMakeLists.txt` and ensures it is in the allowed list.
