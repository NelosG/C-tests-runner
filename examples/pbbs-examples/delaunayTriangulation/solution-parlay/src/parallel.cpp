// Parlay-native incremental Delaunay. Point sequence built outside the
// timed region (matches pbbs's delaunay bench); only delaunay() is timed.
// Triangle extraction (drop boundary-touching triangles, flatten) is in
// result(), outside the timed region.

#include <delaunay.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <pbbs_inc_delaunay.h>

namespace {

    struct DelaunayCtxImpl : student::DelaunayContext {
        parlay::sequence<pbbs_inc_delaunay::point> P;
        pbbs_inc_delaunay::triangles<pbbs_inc_delaunay::point> tris;
        std::size_t n;

        DelaunayCtxImpl(const parlay::sequence<double>& xs,
                        const parlay::sequence<double>& ys) {
            n = xs.size();
            const double* xp = xs.data();
            const double* yp = ys.data();
            P = parlay::tabulate(n, [xp, yp](std::size_t i) {
                return pbbs_inc_delaunay::point(xp[i], yp[i]);
            });
        }

        void run() override {
            if(n >= 3) tris = pbbs_inc_delaunay::delaunay(P);
        }

        parlay::sequence<std::int64_t> result() const override {
            const std::size_t T = tris.T.size();
            const std::size_t np = n;
            auto blocks = parlay::tabulate(T, [&](std::size_t i) {
                const auto& t = tris.T[i];
                parlay::sequence<std::int64_t> seg;
                if(t[0] >= 0 && t[1] >= 0 && t[2] >= 0
                   && (std::size_t)t[0] < np
                   && (std::size_t)t[1] < np
                   && (std::size_t)t[2] < np) {
                    seg.reserve(3);
                    seg.push_back(t[0]);
                    seg.push_back(t[1]);
                    seg.push_back(t[2]);
                }
                return seg;
            });
            return parlay::flatten(blocks);
        }
    };

}

namespace student {

    std::unique_ptr<DelaunayContext> build_delaunay(
        const parlay::sequence<double>& xs,
        const parlay::sequence<double>& ys)
    {
        return std::make_unique<DelaunayCtxImpl>(xs, ys);
    }

} // namespace student
