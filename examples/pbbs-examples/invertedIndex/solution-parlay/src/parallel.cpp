#include <index.h>
#include <pbbs_parallel_index.h>

namespace student {

    std::string parallel_build_index(const std::string& text,
                                     const std::string& doc_start) {
        pbbs_parallel_index::charseq s(text.begin(), text.end());
        pbbs_parallel_index::charseq ds(doc_start.begin(), doc_start.end());
        auto out = pbbs_parallel_index::build_index(s, ds);
        return std::string(out.begin(), out.end());
    }

} // namespace student
