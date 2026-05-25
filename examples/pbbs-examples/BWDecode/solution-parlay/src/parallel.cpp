#include <bw.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <pbbs_list_rank.h>

namespace student {

    std::string list_rank_bw_decode(const parlay::sequence<unsigned char>& encoded) {
        auto out = pbbs_list_rank::bw_decode(encoded);
        std::string result(out.size(), '\0');
        char* dst = result.data();
        parlay::parallel_for(0, out.size(), [&](std::size_t i) {
            dst[i] = static_cast<char>(out[i]);
        });
        return result;
    }

} // namespace student
