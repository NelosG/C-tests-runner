// Wrapper: receives a Delaunay triangulation (points + triangle indices)
// from the test setup and hands it to pbbs's incrementalRefine. Matches
// pbbs/benchmarks/delaunayRefine bench driver, which reads a .tri file
// into `triangles<point>` and calls `refine(T)`.

#include <refine.h>
#include <pbbs_inc_refine.h>

namespace student {

    Refined incremental_refine(
        const std::vector<double>& xs,
        const std::vector<double>& ys,
        const std::vector<std::int64_t>& tris,
        double /*min_angle_deg*/)
    {
        using namespace pbbs_inc_refine;
        std::size_t n = xs.size();
        std::size_t m = tris.size() / 3;

        parlay::sequence<point> P = parlay::tabulate(n, [&](std::size_t i) {
            return point(xs[i], ys[i]);
        });
        parlay::sequence<tri> T = parlay::tabulate(m, [&](std::size_t i) {
            tri r = { (int)tris[i*3], (int)tris[i*3+1], (int)tris[i*3+2] };
            return r;
        });
        triangles<point> Tri(std::move(P), std::move(T));

        triangles<point> Out = refine(Tri);

        Refined result;
        result.xs.reserve(Out.P.size());
        result.ys.reserve(Out.P.size());
        for(std::size_t i = 0; i < Out.P.size(); ++i) {
            result.xs.push_back(Out.P[i].x);
            result.ys.push_back(Out.P[i].y);
        }
        result.triangles.reserve(Out.T.size() * 3);
        for(std::size_t i = 0; i < Out.T.size(); ++i) {
            result.triangles.push_back(Out.T[i][0]);
            result.triangles.push_back(Out.T[i][1]);
            result.triangles.push_back(Out.T[i][2]);
        }
        return result;
    }

} // namespace student
