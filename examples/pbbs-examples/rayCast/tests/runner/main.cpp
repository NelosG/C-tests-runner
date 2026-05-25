#include <raycast.h>
#include <runner.h>
#include <stdexcept>

RUNNER_MAIN {
    TestData vars = runner::read_object("vars");
    std::string algo = vars.read_string("algo");
    TestData g = vars.read_object("data");
    auto num_verts = g.read_value<std::int64_t>("num_verts");
    auto verts = g.read_array<double>("verts");
    auto num_tris = g.read_value<std::int64_t>("num_tris");
    auto idx = g.read_array<std::int64_t>("idx");
    auto num_rays = g.read_value<std::int64_t>("num_rays");
    auto ro = g.read_array<double>("ray_origin");
    auto rd = g.read_array<double>("ray_dir");
    std::vector<std::int64_t> hits;

    RUNNER_EXECUTE {
        if(algo == "kdtree_ray_cast") {
            hits = student::kdtree_ray_cast(
                num_verts, verts, num_tris, idx, num_rays, ro, rd);
        } else {
            throw std::runtime_error("rayCast runner: unknown algo '" + algo + "'");
        }
    };

    runner::write_array<std::int64_t>("hits", hits);
}
