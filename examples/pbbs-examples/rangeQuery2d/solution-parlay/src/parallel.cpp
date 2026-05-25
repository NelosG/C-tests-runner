// Simplified port. pbbs parallelPlaneSweep uses an augmented persistent
// map (pam_set<point>) over points sorted by y, swept across x. That
// requires the PAM library (cmuparlay/PAM), which is a separate header
// dependency outside the scope of the in-tree port - the policy is to
// not pull in external libs.
//
// What we do instead, matching pbbs's I/O contract: first 2*n_queries
// of the input define query rectangles as pairs of opposite corners;
// the rest are the data points. We sort the data points by x once, then
// for each query find the x-range with binary search and linearly scan
// the slice counting points whose y lies in [y1, y2]. Queries are
// answered in parallel; the total count is summed across queries.

#include <algorithm>
#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <range.h>
#include <vector>

namespace student {

    std::int64_t parallel_range_count(
        const std::vector<double>& all_x,
        const std::vector<double>& all_y,
        std::int64_t n_queries)
    {
        std::int64_t n_total = static_cast<std::int64_t>(all_x.size());
        std::int64_t n_pts = n_total - 2 * n_queries;
        if(n_pts <= 0 || n_queries <= 0) return 0;

        std::int64_t pts_offset = 2 * n_queries;
        const double* xp = all_x.data();
        const double* yp = all_y.data();
        parlay::sequence<std::int64_t> order = parlay::tabulate(n_pts,
            [&](std::size_t i) -> std::int64_t { return pts_offset + i; });
        parlay::sort_inplace(order,
            [&](std::int64_t a, std::int64_t b) { return xp[a] < xp[b]; });
        parlay::sequence<double> sx = parlay::tabulate(n_pts,
            [&](std::size_t i) { return xp[order[i]]; });
        parlay::sequence<double> sy = parlay::tabulate(n_pts,
            [&](std::size_t i) { return yp[order[i]]; });

        parlay::sequence<std::int64_t> per_q = parlay::tabulate(n_queries,
            [&](std::size_t i) -> std::int64_t {
                double x1 = std::min(xp[2*i], xp[2*i+1]);
                double x2 = std::max(xp[2*i], xp[2*i+1]);
                double y1 = std::min(yp[2*i], yp[2*i+1]);
                double y2 = std::max(yp[2*i], yp[2*i+1]);
                auto lo = std::lower_bound(sx.begin(), sx.end(), x1);
                auto hi = std::upper_bound(sx.begin(), sx.end(), x2);
                std::int64_t lo_i = static_cast<std::int64_t>(lo - sx.begin());
                std::int64_t hi_i = static_cast<std::int64_t>(hi - sx.begin());
                std::int64_t c = 0;
                for(std::int64_t j = lo_i; j < hi_i; ++j) {
                    if(sy[j] >= y1 && sy[j] <= y2) ++c;
                }
                return c;
            });
        return parlay::reduce(per_q);
    }

} // namespace student
