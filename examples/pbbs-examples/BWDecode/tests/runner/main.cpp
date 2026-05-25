// The encoded byte sequence is materialised outside RUNNER_EXECUTE so only
// the list-ranking decode body is timed.

#include <stdexcept>
#include <string>

#include <parlay/sequence.h>

#include <bw.h>
#include <runner.h>
#include <runner_parlay.h>

RUNNER_MAIN {
    TestData vars = runner::read_object("vars");
    std::string algo = vars.read_string("algo");
    auto encoded = runner::read_parlay_chars<unsigned char>(vars, "encoded");
    std::string result;

    RUNNER_EXECUTE {
        if(algo == "list_rank_bw_decode") {
            result = student::list_rank_bw_decode(encoded);
        } else {
            throw std::runtime_error("BWDecode runner: unknown algo '" + algo + "'");
        }
    };

    runner::write_string("decoded", result);
}
