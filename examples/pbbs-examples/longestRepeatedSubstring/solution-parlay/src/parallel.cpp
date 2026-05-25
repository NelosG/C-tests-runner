#include <lrs.h>
#include <pbbs_doubling_lrs.h>

namespace student {

    std::tuple<std::int64_t,std::int64_t,std::int64_t>
    doubling_lrs(const parlay::sequence<unsigned char>& s) {
        auto r = pbbs_doubling_lrs::lrs(s);
        return std::make_tuple(
            static_cast<std::int64_t>(std::get<0>(r)),
            static_cast<std::int64_t>(std::get<1>(r)),
            static_cast<std::int64_t>(std::get<2>(r)));
    }

} // namespace student
