// Wrapper: 2D points -> pbbs octree -> per-point range_search.
// Output is pbbs's flat result layout (see rqkd.h header).

#include <pbbs_octtree_range.h>
#include <rqkd.h>

namespace student {

    namespace {

        // pbbs's bench vertex type; KK=0 means no fixed-size ngh array.
        template<class PT, int KK>
        struct vertex {
            using pointT = PT;
            int identifier;
            pointT pt;
            vertex* ngh[KK > 0 ? KK : 1];
            vertex(pointT p, int id) : pt(p), identifier(id) {}
            vertex() : identifier(0), pt() {}
            size_t counter = 0;
            size_t counter2 = 0;
        };

    } // namespace

    std::vector<long long> range_neighbors(
        const std::vector<double>& xs,
        const std::vector<double>& ys,
        double rad)
    {
        using namespace pbbs_octtree_range;
        using point = point2d<double>;
        using vtx = vertex<point, 0>;

        std::int64_t n = static_cast<std::int64_t>(xs.size());
        if(n == 0) {
            std::vector<long long> empty(1, 0);
            return empty;
        }

        parlay::sequence<vtx> vv = parlay::tabulate(n, [&](std::size_t i) {
            return vtx(point(xs[i], ys[i]), (int)i);
        });
        parlay::sequence<vtx*> v = parlay::tabulate(n,
            [&](std::size_t i) { return &vv[i]; });

        using knn_tree = k_nearest_neighbors<vtx, 1>;
        knn_tree T(v);
        auto* root = T.tree.get();

        parlay::sequence<parlay::sequence<int>> answers(n);
        parlay::parallel_for(0, n, [&](std::size_t i) {
            answers[i] = range_search_for<vtx>(root, v[i], rad);
        });

        // pbbs flat result layout:
        //   [n, c_0, c_1, ..., c_{n-1}, id_0_0, id_0_1, ..., id_1_0, ...]
        // Build with parlay::scan over per-point sizes for the id offsets.
        auto sizes = parlay::tabulate(n,
            [&](std::size_t i) -> std::int64_t { return answers[i].size(); });
        auto [offsets, total_ids] = parlay::scan(sizes);
        std::vector<long long> out(1 + n + total_ids);
        long long* dst = out.data();
        dst[0] = n;
        parlay::parallel_for(0, n, [&](std::size_t i) {
            dst[1 + i] = (long long)answers[i].size();
        });
        long long* ids_base = dst + 1 + n;
        parlay::parallel_for(0, n, [&](std::size_t i) {
            long long off = offsets[i];
            const auto& a = answers[i];
            for(std::size_t j = 0; j < a.size(); ++j)
                ids_base[off + j] = (long long)a[j];
        });
        return out;
    }

} // namespace student
