// The text is materialised into a parlay::sequence<char> outside
// RUNNER_EXECUTE, and only the parallel word-count kernel is timed (matches
// pbbs's time_loop, which times wordCounts(In) and builds the output map
// afterwards). The std::map assembly here is also outside the timed region.

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>

#include <parlay/sequence.h>

#include <runner.h>
#include <runner_parlay.h>
#include <wc.h>

RUNNER_MAIN {
    TestData vars = runner::read_object("vars");
    std::string algo = vars.read_string("algo");
    auto text = runner::read_parlay_chars<char>(vars, "text");
    student::WordCounts pairs;

    RUNNER_EXECUTE {
        if(algo == "histogram_word_counts") {
            pairs = student::histogram_word_counts(text);
        } else {
            throw std::runtime_error("wordCounts runner: unknown algo '" + algo + "'");
        }
    };

    // Output assembly outside the timed region (pbbs builds its output file
    // after time_loop too).
    std::map<std::string, std::int64_t> result;
    for(const auto& kv : pairs) {
        result.emplace(std::string(kv.first.begin(), kv.first.end()),
                       static_cast<std::int64_t>(kv.second));
    }
    using MapT = std::map<std::string, std::int64_t>;
    runner::output().write_map<MapT>("counts", result);
}
