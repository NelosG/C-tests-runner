#include <matching.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <pbbs_incremental_matching.h>
#include <pbbs_nd_matching.h>

namespace {

    template <typename Edges>
    Edges make_edges(std::int64_t n,
                     const std::vector<std::int64_t>& us,
                     const std::vector<std::int64_t>& vs) {
        const std::int64_t* up = us.data();
        const std::int64_t* vp = vs.data();
        parlay::sequence<edge<unsigned int>> ev = parlay::tabulate(us.size(),
            [&](std::size_t i) {
                edge<unsigned int> e;
                e.u = static_cast<unsigned int>(up[i]);
                e.v = static_cast<unsigned int>(vp[i]);
                return e;
            });
        return Edges(std::move(ev),
                     static_cast<std::size_t>(n),
                     static_cast<std::size_t>(n));
    }

    template <typename SeqT>
    std::vector<std::int64_t> to_int64_vec(const SeqT& s) {
        std::vector<std::int64_t> out(s.size());
        std::int64_t* dst = out.data();
        parlay::parallel_for(0, s.size(), [&](std::size_t i) {
            dst[i] = static_cast<std::int64_t>(s[i]);
        });
        return out;
    }

}

namespace student {

    std::vector<std::int64_t> nd_matching(
        std::int64_t n,
        const std::vector<std::int64_t>& us,
        const std::vector<std::int64_t>& vs)
    {
        auto E = make_edges<pbbs_nd_matching::edges>(n, us, vs);
        auto idx = pbbs_nd_matching::maximalMatching(E);
        return to_int64_vec(idx);
    }

    std::vector<std::int64_t> incremental_matching(
        std::int64_t n,
        const std::vector<std::int64_t>& us,
        const std::vector<std::int64_t>& vs)
    {
        auto E = make_edges<pbbs_incremental_matching::edges>(n, us, vs);
        auto idx = pbbs_incremental_matching::maximalMatching(E);
        return to_int64_vec(idx);
    }

} // namespace student
