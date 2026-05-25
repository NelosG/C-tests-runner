// The character sequence is materialised outside RUNNER_EXECUTE; only
// suffixArray() is timed (matches pbbs's SATime.C, which times R =
// suffixArray(ss) and writes the result afterwards). The filter + int64
// materialisation happen in result(), also outside the timed region.

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <parlay/sequence.h>

#include <runner.h>
#include <runner_parlay.h>
#include <sa.h>

RUNNER_MAIN {
    TestData vars = runner::read_object("vars");
    std::string algo = vars.read_string("algo");
    auto s = runner::read_parlay_chars<unsigned char>(vars, "s");

    if(algo != "parallel_ks") {
        throw std::runtime_error("suffixArray runner: unknown algo '" + algo + "'");
    }

    auto ctx = student::build_sa(s);

    RUNNER_EXECUTE {
        ctx->run();
    };

    auto result = ctx->result();
    runner::write_array<std::int64_t>("sa", result);
}
