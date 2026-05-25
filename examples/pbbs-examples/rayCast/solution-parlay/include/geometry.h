// Vendored subset of pbbsbench/common/geometry.h
// (MIT licensed, (c) Guy Blelloch and the PBBS team).
// Contains the 2d + 3d point/vector types, ray<point>, and
// triangles<point> needed by the kdTree raycast variant.
#ifndef PBBS_GEOMETRY_H_
#define PBBS_GEOMETRY_H_

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <parlay/parallel.h>
#include <parlay/primitives.h>

template <class Coord> class point3d;

template <class Coord>
class vector3d {
public:
    using coord = Coord;
    using vector = vector3d;
    using point = point3d<coord>;
    coord x; coord y; coord z;
    vector3d(coord x, coord y, coord z) : x(x), y(y), z(z) {}
    vector3d() :x(0), y(0), z(0) {}
    vector3d(point p);
    vector operator+(vector op2) {return vector(x+op2.x, y+op2.y, z+op2.z);}
    vector operator-(vector op2) {return vector(x-op2.x, y-op2.y, z-op2.z);}
    point operator+(point op2);
    vector operator*(coord s) {return vector(x*s, y*s, z*s);}
    vector operator/(coord s) {return vector(x/s, y/s, z/s);}
    coord& operator[](int i) {return (i==0) ? x : (i==1) ? y : z;}
    coord dot(vector v) {return x*v.x + y*v.y + z*v.z;}
    vector cross(vector v) {
        return vector(y*v.z - z*v.y, z*v.x - x*v.z, x*v.y - y*v.x);
    }
    coord Length() { return std::sqrt(x*x + y*y + z*z);}
    coord sqLength() { return x*x + y*y + z*z;}
    static const int dim = 3;
};

template <class Coord>
class point3d {
public:
    using coord = Coord;
    using vector = vector3d<coord>;
    using point = point3d;
    coord x; coord y; coord z;
    int dimension() {return 3;}
    point3d(coord x, coord y, coord z) : x(x), y(y), z(z) {}
    point3d() : x(0), y(0), z(0) {}
    point3d(vector v) : x(v.x), y(v.y), z(v.z) {};
    vector operator-(point op2) {return vector(x-op2.x, y-op2.y, z-op2.z);}
    point operator+(vector op2) {return point(x+op2.x, y+op2.y, z+op2.z);}
    point minCoords(point b) {
        return point(std::min(x, b.x), std::min(y, b.y), std::min(z, b.z));
    }
    point maxCoords(point b) {
        return point(std::max(x, b.x), std::max(y, b.y), std::max(z, b.z));
    }
    coord& operator[](int i) {return (i==0) ? x : (i==1) ? y : z;}
    static const int dim = 3;
};

template <class coord>
inline point3d<coord> vector3d<coord>::operator+(point3d<coord> op2) {
    return point3d<coord>(x+op2.x, y+op2.y, z+op2.z);
}

template <class coord>
inline vector3d<coord>::vector3d(point3d<coord> p) {
    x = p.x; y = p.y; z = p.z;
}

template <class Coord> class point2d;

template <class Coord>
class vector2d {
public:
    using coord = Coord;
    using point = point2d<coord>;
    using vector = vector2d;
    coord x; coord y;
    vector2d(coord x, coord y) : x(x), y(y) {}
    vector2d() : x(0), y(0)  {}
    vector2d(point p);
    vector operator+(vector op2) {return vector(x+op2.x, y+op2.y);}
    vector operator-(vector op2) {return vector(x-op2.x, y-op2.y);}
    point operator+(point op2);
    vector operator*(coord s) {return vector(x*s, y*s);}
    coord operator[](int i) {return (i==0) ? x : y;};
    coord dot(vector v) {return x*v.x + y*v.y;}
    coord cross(vector v) {return x*v.y - y*v.x;}
    coord Length() { return std::sqrt(x*x + y*y);}
    static const int dim = 2;
};

template <class Coord>
class point2d {
public:
    using coord = Coord;
    using vector = vector2d<coord>;
    using point = point2d;
    coord x; coord y;
    int dimension() {return 2;}
    point2d(coord x, coord y) : x(x), y(y) {}
    point2d() : x(0), y(0) {}
    point2d(vector v) : x(v.x), y(v.y) {};
    vector operator-(point op2) {return vector(x-op2.x, y-op2.y);}
    point operator+(vector op2) {return point(x+op2.x, y+op2.y);}
    coord operator[](int i) {return (i==0) ? x : y;};
    static const int dim = 2;
};

template <class coord>
inline point2d<coord> vector2d<coord>::operator+(point2d<coord> op2) {
    return point2d<coord>(x+op2.x, y+op2.y);
}

template <class coord>
inline vector2d<coord>::vector2d(point2d<coord> p) { x = p.x; y = p.y; }

using tri = std::array<int, 3>;

template <class point_t>
struct triangles {
    size_t numPoints() { return P.size(); }
    size_t numTriangles() { return T.size(); }
    parlay::sequence<point_t> P;
    parlay::sequence<tri> T;
    triangles() {}
    triangles(parlay::sequence<point_t> P, parlay::sequence<tri> T)
        : P(std::move(P)), T(std::move(T)) {}
};

template <class point_t>
struct ray {
    using vector = typename point_t::vector;
    point_t o;
    vector d;
    ray(point_t _o, vector _d) : o(_o), d(_d) {}
    ray() {}
};

#endif
