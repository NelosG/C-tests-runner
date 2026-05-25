// Vendored subset of pbbsbench/common/geometry.h
// (MIT licensed, (c) Guy Blelloch and the PBBS team).
// Only the 2d-point + triArea infrastructure used by quickHull.
#ifndef PBBS_GEOMETRY_H_
#define PBBS_GEOMETRY_H_

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <math.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>

template <class Coord>
class point2d;

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
    vector operator+(vector op2) {return vector(x + op2.x, y + op2.y);}
    vector operator-(vector op2) {return vector(x - op2.x, y - op2.y);}
    point operator+(point op2);
    vector operator*(coord s) {return vector(x * s, y * s);}
    vector operator/(coord s) {return vector(x / s, y / s);}
    coord operator[] (int i) {return (i==0) ? x : y;};
    coord dot(vector v) {return x * v.x + y * v.y;}
    coord cross(vector v) { return x*v.y - y*v.x; }
    coord maxDim() {return std::max(x,y);}
    coord Length(void) { return sqrt(x*x+y*y);}
    coord sqLength(void) { return x*x+y*y;}
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
    vector operator-(point op2) {return vector(x - op2.x, y - op2.y);}
    point operator+(vector op2) {return point(x + op2.x, y + op2.y);}
    coord operator[] (int i) {return (i==0) ? x : y;};
    point minCoords(point b) { return point(std::min(x,b.x),std::min(y,b.y)); }
    point maxCoords(point b) { return point(std::max(x,b.x),std::max(y,b.y)); }
    static const int dim = 2;
};

template <class coord>
inline point2d<coord> vector2d<coord>::operator+(point2d<coord> op2) {
    return point2d<coord>(x + op2.x, y + op2.y);}

template <class coord>
inline vector2d<coord>::vector2d(point2d<coord> p) { x = p.x; y = p.y;}

// Returns twice the signed area of the oriented triangle (a, b, c).
template <class coord>
inline coord triArea(point2d<coord> a, point2d<coord> b, point2d<coord> c) {
    return (b-a).cross(c-a);
}

#endif
