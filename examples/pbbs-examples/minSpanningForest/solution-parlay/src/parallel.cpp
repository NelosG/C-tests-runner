#include <mst.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <pbbs_parallel_kruskal.h>

namespace student {

    std::vector<std::int64_t> parallel_kruskal_mst(
        std::int64_t n,
        const std::vector<std::int64_t>& us,
        const std::vector<std::int64_t>& vs,
        const std::vector<float>& weights)
    {
        const std::int64_t* up = us.data();
        const std::int64_t* vp = vs.data();
        const float* wp = weights.data();
        parlay::sequence<wghEdge<int, float>> ev = parlay::tabulate(us.size(),
            [&](std::size_t i) {
                wghEdge<int, float> e;
                e.u = static_cast<int>(up[i]);
                e.v = static_cast<int>(vp[i]);
                e.weight = wp[i];
                return e;
            });
        wghEdgeArray<int, float> E(std::move(ev), static_cast<int>(n));
        auto idx = pbbs_parallel_kruskal::mst(E);
        std::vector<std::int64_t> out(idx.size());
        std::int64_t* dst = out.data();
        parlay::parallel_for(0, idx.size(), [&](std::size_t i) {
            dst[i] = static_cast<std::int64_t>(idx[i]);
        });
        return out;
    }

} // namespace student
