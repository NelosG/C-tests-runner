// Parlay-native octTree kNN. Mirrors pbbsbench/benchmarks/nearestNeighbors/
// bench/neighborsTime.C: vertex array built outside the timed region, only
// ANN<max_k>(v, k) timed, neighbor indices extracted afterwards.
#include <nn.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <pbbs_octtree_nn.h>

namespace {

    // Mirrors neighborsTime.C's vertex.
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
    struct KnnCtxImpl : student::KnnContext {
        using vtx = vertex<point_t, max_k>;
        int k;
        std::int64_t n;
        parlay::sequence<vtx> vv;
        parlay::sequence<vtx*> v;

        KnnCtxImpl(int k_, std::int64_t dim, const parlay::sequence<double>& points)
            : k(k_)
        {
            n = static_cast<std::int64_t>(points.size()) / dim;
            const double* src = points.data();
            vv = parlay::tabulate(n, [&, src](std::size_t i) -> vtx {
                point_t p;
                for(int d = 0; d < dim; ++d) p[d] = src[i * dim + d];
                return vtx(p, static_cast<int>(i));
            });
            vtx* base = vv.data();
            v = parlay::tabulate(n, [base](std::size_t i) -> vtx* {
                return base + i;
            });
        }

        void run() override {
            pbbs_octtree_nn::ANN<max_k>(v, k);
        }

        parlay::sequence<std::int64_t> result() const override {
            const int kk = k;
            return parlay::tabulate(static_cast<std::size_t>(n) * kk,
                [this, kk](std::size_t idx) -> std::int64_t {
                    std::size_t i = idx / kk, j = idx % kk;
                    vtx* p = v[i]->ngh[j];
                    return p ? static_cast<std::int64_t>(p->identifier) : -1;
                });
        }
    };

} // namespace

namespace student {

    std::unique_ptr<KnnContext> octtree_knn_build(
        std::int64_t k, std::int64_t dim, const parlay::sequence<double>& points)
    {
        constexpr int max_k = 32;
        if(dim == 2)
            return std::make_unique<KnnCtxImpl<point2d<double>, max_k>>((int)k, dim, points);
        return std::make_unique<KnnCtxImpl<point3d<double>, max_k>>((int)k, dim, points);
    }

} // namespace student
