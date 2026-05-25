#include <index.h>
#include <runner.h>
#include <stdexcept>

RUNNER_MAIN {
    TestData vars = runner::read_object("vars");
    std::string algo = vars.read_string("algo");
    auto text = vars.read_string("text");
    auto doc_start = vars.read_string("doc_start");
    std::string result;

    RUNNER_EXECUTE {
        if(algo == "parallel_build_index") {
            result = student::parallel_build_index(text, doc_start);
        } else {
            throw std::runtime_error("invertedIndex runner: unknown algo '" + algo + "'");
        }
    };

    runner::write_string("formatted", result);
}
