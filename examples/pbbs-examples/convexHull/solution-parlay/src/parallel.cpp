#include <hull.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <pbbs_quick_hull.h>

namespace {

    struct HullCtxImpl : student::HullContext {
        parlay::sequence<pbbs_quick_hull::point> pts;
        parlay::sequence<pbbs_quick_hull::indexT> idx;

        HullCtxImpl(const parlay::sequence<double>& xs,
                    const parlay::sequence<double>& ys) {
            const double* xp = xs.data();
            const double* yp = ys.data();
            pts = parlay::tabulate(xs.size(), [xp, yp](std::size_t i) {
                return pbbs_quick_hull::point(xp[i], yp[i]);
            });
        }

        void run() override {
            idx = pbbs_quick_hull::hull(pts);
        }

        parlay::sequence<std::int64_t> result() const override {
            return parlay::map(idx, [](pbbs_quick_hull::indexT x) {
                return static_cast<std::int64_t>(x);
            });
        }
    };

}

namespace student {

    std::unique_ptr<HullContext> build_hull(
        const parlay::sequence<double>& xs,
        const parlay::sequence<double>& ys)
    {
        return std::make_unique<HullCtxImpl>(xs, ys);
    }

} // namespace student
