// Student API for wordCounts. One parallel variant using
// parlay::histogram_by_key (the "histogram" variant from pbbsbench).
//
// Parlay-native: input is a parlay::sequence<char> materialised by the
// runner outside RUNNER_EXECUTE. The timed call returns the raw (word,
// count) pairs - exactly what pbbs's wordCounts(In) returns and times. The
// std::string conversion + std::map assembly happen in the runner main
// OUTSIDE the timed region (pbbs also builds its output after time_loop).

#pragma once

#include <pbbs_histogram_wc.h>

#include <parlay/sequence.h>

namespace student {

    using WordCounts = parlay::sequence<pbbs_histogram_wc::result_type>;

    // Returns (word, count) pairs. word is a parlay::sequence<char>.
    WordCounts histogram_word_counts(const parlay::sequence<char>& text);

} // namespace student
