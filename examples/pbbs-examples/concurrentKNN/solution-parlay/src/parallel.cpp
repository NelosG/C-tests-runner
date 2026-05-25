// Same wrapper as nearestNeighbors: thin std::vector -> pbbs vertex
// translation, then pbbs ANN<max_k>(v, k).
#include <cknn.h>
#include <pbbs_octtree_nn.h>

namespace {

    template <typename point_t, int max_k>
    struct vertex {
        using pointT = point_t;
        int identifier;
        pointT pt;
        vertex* ngh[max_k];
        std::size_t counter = 0;
        std::size_t counter2 = 0;
        vertex(pointT p, int id) : identifier(id), pt(p) {}
    };

    template <typename point_t, int max_k>
    std::vector<std::int64_t> run(
        int k, std::int64_t dim,
        const std::vector<double>& points)
    {
        using vtx = vertex<point_t, max_k>;
        std::int64_t n = points.size() / dim;
        auto vv = parlay::tabulate(n, [&](std::size_t i) -> vtx {
            point_t p;
            for(int d = 0; d < dim; ++d) p[d] = points[i * dim + d];
            return vtx(p, (int)i);
        });
        auto v = parlay::tabulate(n, [&](std::size_t i) -> vtx* {
            return &vv[i];
        });

        pbbs_octtree_nn::ANN<max_k>(v, k);

        std::vector<std::int64_t> out(n * k);
        for(std::int64_t i = 0; i < n; ++i) {
            for(int j = 0; j < k; ++j) {
                vtx* n_ptr = v[i]->ngh[j];
                out[i * k + j] = n_ptr ? (std::int64_t)n_ptr->identifier : -1;
            }
        }
        return out;
    }

} // namespace

namespace student {

    std::vector<std::int64_t> octtree_knn(
        std::int64_t k,
        std::int64_t dim,
        const std::vector<double>& points)
    {
        constexpr int max_k = 32;
        if(dim == 2)
            return run<point2d<double>, max_k>((int)k, dim, points);
        else
            return run<point3d<double>, max_k>((int)k, dim, points);
    }

} // namespace student
