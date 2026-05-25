#include <pbbs_histogram_wc.h>
#include <wc.h>

namespace student {

    WordCounts histogram_word_counts(const parlay::sequence<char>& text)
    {
        // Exactly pbbs's timed kernel: parallel histogram-by-key over words.
        return pbbs_histogram_wc::wordCounts(text);
    }

} // namespace student
