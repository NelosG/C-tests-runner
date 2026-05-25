// Student API for rayCast. One pbbsbench parlay variant: SAH-built
// kd-tree (`kdTree`).
#pragma once

#include <cstdint>
#include <vector>

namespace student {

    std::vector<std::int64_t> kdtree_ray_cast(
        std::int64_t num_vertices,
        const std::vector<double>& verts,         // num_vertices x 3
        std::int64_t num_tris,
        const std::vector<std::int64_t>& tri_idx, // num_tris x 3
        std::int64_t num_rays,
        const std::vector<double>& ray_origin,    // num_rays x 3
        const std::vector<double>& ray_dir);      // num_rays x 3

} // namespace student
