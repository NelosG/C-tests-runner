#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <pbbs_kdtree_raycast.h>
#include <raycast.h>

namespace student {

    std::vector<std::int64_t> kdtree_ray_cast(
        std::int64_t num_vertices,
        const std::vector<double>& verts,
        std::int64_t num_tris,
        const std::vector<std::int64_t>& tri_idx,
        std::int64_t num_rays,
        const std::vector<double>& ray_origin,
        const std::vector<double>& ray_dir)
    {
        using namespace pbbs_kdtree_raycast;

        const double* vsrc = verts.data();
        parlay::sequence<point> P = parlay::tabulate(num_vertices,
            [&](std::size_t i) {
                return point(vsrc[i*3], vsrc[i*3+1], vsrc[i*3+2]);
            });

        const std::int64_t* tsrc = tri_idx.data();
        parlay::sequence<tri> T = parlay::tabulate(num_tris,
            [&](std::size_t i) {
                tri t;
                t[0] = static_cast<int>(tsrc[i*3]);
                t[1] = static_cast<int>(tsrc[i*3+1]);
                t[2] = static_cast<int>(tsrc[i*3+2]);
                return t;
            });
        triangles<point> Tri(std::move(P), std::move(T));

        const double* osrc = ray_origin.data();
        const double* dsrc = ray_dir.data();
        parlay::sequence<ray<point>> rays = parlay::tabulate(num_rays,
            [&](std::size_t i) {
                point o(osrc[i*3], osrc[i*3+1], osrc[i*3+2]);
                vect d(dsrc[i*3], dsrc[i*3+1], dsrc[i*3+2]);
                return ray<point>(o, d);
            });

        auto hits = rayCast(Tri, rays);
        std::vector<std::int64_t> out(num_rays);
        std::int64_t* dst = out.data();
        parlay::parallel_for(0, num_rays, [&](std::size_t i) {
            dst[i] = static_cast<std::int64_t>(hits[i]);
        });
        return out;
    }

} // namespace student
