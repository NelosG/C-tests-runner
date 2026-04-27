#include <runner.h>
#include <scan.h>

RUNNER_MAIN {
    auto arr = runner::read_array<long long>("array");
    std::vector<long long> result(arr.size());

    RUNNER_EXECUTE{
        parallel::scan(arr, result);
    
    
    };

    runner::write_array("result", result);
}
