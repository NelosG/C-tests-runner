#include <runner.h>
#include <sum.h>

RUNNER_MAIN {
    auto array = runner::read_array<long long>("array");
    bool require_positive = runner::read_value<bool>("require_positive");

    long long sum = 0;
    long long max_val = 0;
    bool ok = true;

    RUNNER_EXECUTE{
        student::analyze(array, require_positive, sum, max_val, ok);
    
    
    
    };

    runner::write_value<long long>("sum", sum);
    runner::write_value<long long>("max", max_val);
    runner::write_value<bool>("ok", ok);
}
