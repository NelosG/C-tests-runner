#pragma once

#include <cstdint>
#include <type_traits>

// Shared POD types used by both the runner main.cpp and the test plugin's
// setup/verify lambdas. Placed in tests/include/ because the engine's runner
// wrapper adds that directory to the runner exe's include path, and the test
// plugin's CMakeLists pulls it in via include_directories(include).
struct Edge {
    std::int32_t u;
    std::int32_t v;
    float w;

    bool operator==(const Edge& other) const {
        return u == other.u && v == other.v && w == other.w;
    }
};
static_assert(std::is_trivially_copyable_v<Edge>);

struct Point2d {
    double x;
    double y;

    bool operator==(const Point2d& other) const {
        return x == other.x && y == other.y;
    }
};
static_assert(std::is_trivially_copyable_v<Point2d>);
