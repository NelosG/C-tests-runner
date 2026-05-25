// Dispatcher for the comparisonSort assignment. Input is a `vars` object
// containing the dispatch key "algo" plus the array to sort. The runner
// reads the key, calls the matching student function, and writes back
// the sorted array.
//
// The input sequence is materialised outside RUNNER_EXECUTE so only the
// sort body is timed. comparisonSort keeps warmupIterations=0 (in-place
// mutation), so a single timed pass consuming `arr` is fine. Element type
// is int (4-byte) to match pbbs's comparisonSort.

#include <stdexcept>
#include <string>

#include <parlay/sequence.h>

#include <runner.h>
#include <runner_parlay.h>
#include <sort.h>

RUNNER_MAIN {
    TestData vars = runner::read_object("vars");
    std::string algo = vars.read_string("algo");
    auto arr = runner::read_parlay_sequence<int>(vars, "arr");
    parlay::sequence<int> result;

    RUNNER_EXECUTE {
        if(algo == "parlay_sample_sort") {
            result = student::parlay_sample_sort(arr);
        } else if(algo == "quick_sort") {
            result = student::parallel_quick_sort(arr);
        } else if(algo == "merge_sort") {
            result = student::parallel_merge_sort(arr);
        } else if(algo == "stable_sample_sort") {
            result = student::parallel_stable_sample_sort(arr);
        } else {
            throw std::runtime_error("comparisonSort runner: unknown algo '" + algo + "'");
        }
    };

    runner::write_parlay_sequence<int>("result", result);
}
