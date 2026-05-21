#include <runner.h>
#include <quick_sort.h>

RUNNER_MAIN {
    auto arr = runner::read_array<long long>("array");

    RUNNER_EXECUTE {
        parallel::qsort(arr);
    };

    runner::write_array("result", arr);
}
