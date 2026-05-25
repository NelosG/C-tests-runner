// Vendored from pbbsbench/benchmarks/rayCast/kdTree/{kdTree.h,
// rayTriangleIntersect.h, ray.C}. (MIT licensed, (c) Guy Blelloch and
// the PBBS team.) The original split across .h + .C is collapsed into
// a single header so the whole algorithm fits in one TU - same parlay
// scheduler thread_local ODR constraint we hit everywhere else.
#pragma once

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <parlay/delayed.h>
#include <parlay/internal/get_time.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>

#include <geometry.h>

namespace pbbs_kdtree_raycast {

using parlay::sequence;
using parlay::tabulate;
using parlay::parallel_for;
using parlay::par_do;
using parlay::pack;
using parlay::sort_inplace;
namespace delayed = parlay::delayed;

using index_t = int;
using coord = double;
using point = point3d<coord>;
using vect = point::vector;
using triangs = triangles<point>;
using ray_t = ray<point>;

// ----- kdTree.h (verbatim) ----------------------------------------------------

struct event {
  float v;
  index_t p;
  event(float value, index_t index, bool type)
    : v(value), p((index << 1) + type) {}
  event() {}
};

#define KDT_START 0
#define KDT_IS_START(_event) (!(_event.p & 1))
#define KDT_END 1
#define KDT_IS_END(_event) ((_event.p & 1))
#define KDT_GET_INDEX(_event) (_event.p >> 1)

struct range {
  float min;
  float max;
  range(float _min, float _max) : min(_min), max(_max) {}
  range() {}
};

using Boxes = std::array<sequence<range>, 3>;
using Events = std::array<sequence<event>, 3>;
using BoundingBox = std::array<range, 3>;

struct cutInfo {
  float cost;
  float cutOff;
  index_t numLeft;
  index_t numRight;
  cutInfo(float _cost, float _cutOff, index_t nl, index_t nr)
    : cost(_cost), cutOff(_cutOff), numLeft(nl), numRight(nr) {}
  cutInfo() {}
};

struct treeNode {
  treeNode *left;
  treeNode *right;
  BoundingBox box;
  int cutDim;
  float cutOff;
  sequence<index_t> triangleIndices;
  index_t n;
  index_t leaves;

  bool isLeaf() {return left == nullptr;}

  treeNode(treeNode* L, treeNode* R,
           int _cutDim, float _cutOff, BoundingBox B)
    : left(L), right(R), cutDim(_cutDim), cutOff(_cutOff) {
    for(int i = 0; i < 3; i++) box[i] = B[i];
    n = L->n + R->n;
    leaves = L->leaves + R->leaves;
  }

  treeNode(Events E, index_t _n, BoundingBox B)
    : left(NULL), right(NULL) {
    event* events = E[0].begin();
    triangleIndices = sequence<index_t>(_n/2);
    index_t k = 0;
    for(index_t i = 0; i < _n; i++)
      if(KDT_IS_START(events[i]))
        triangleIndices[k++] = KDT_GET_INDEX(events[i]);
    n = _n / 2;
    leaves = 1;
    for(int i = 0; i < 3; i++) box[i] = B[i];
  }

  static parlay::type_allocator<treeNode> node_allocator;

  template <typename... Arguments>
  static treeNode* newNode(Arguments... args) {
    treeNode* r = (treeNode*) node_allocator.alloc();
    new (r) treeNode(args...);
    return r;
  }

  static void delete_tree(treeNode* T) {
    if(T != nullptr) {
      node_allocator.free(T);
    }
  }

  ~treeNode() {
    parlay::par_do_if(n > 1000,
                      [&]() { delete_tree(left); },
                      [&]() { delete_tree(right); });
  }
};

inline parlay::type_allocator<treeNode> treeNode::node_allocator;

// ----- rayTriangleIntersect.h (Moller-Trumbore only) -------------------------

#define KDT_EPSILON 0.00000001

template <class floatT>
inline floatT rayTriangleIntersect(::ray<point3d<floatT>> R,
                                   point3d<floatT> m[]) {
    using pointT = point3d<floatT>;
    using vectT = vector3d<floatT>;
    pointT o = R.o;
    vectT d = R.d;
    vectT e1 = m[1] - m[0];
    vectT e2 = m[2] - m[0];
    vectT pvec = d.cross(e2);
    floatT det = e1.dot(pvec);
    if(det > -KDT_EPSILON && det < KDT_EPSILON) return 0;
    floatT invDet = 1.0 / det;
    vectT tvec = o - m[0];
    floatT u = tvec.dot(pvec) * invDet;
    if(u < 0.0 || u > 1.0) return 0;
    vectT qvec = tvec.cross(e1);
    floatT v = d.dot(qvec) * invDet;
    if(v < 0.0 || u + v > 1.0) return 0;
    floatT t = e2.dot(qvec) * invDet;
    return t;
}

// ----- ray.C (top-level kd-tree build + ray traversal) ----------------------

inline constexpr float CT = 6.0f;
inline constexpr float CL = 1.25f;
inline constexpr float maxExpand = 1.6f;
inline constexpr int maxRecursionDepth = 25;
inline constexpr int minParallelSize = 1000;

inline float boxSurfaceArea(BoundingBox B) {
    float r0 = B[0].max - B[0].min;
    float r1 = B[1].max - B[1].min;
    float r2 = B[2].max - B[2].min;
    return 2 * (r0*r1 + r1*r2 + r0*r2);
}

inline range fixRange(float minv, float maxv) {
    constexpr float epsilon = 0.0000001f;
    return (minv == maxv) ? range(minv, minv + epsilon) : range(minv, maxv);
}

inline float inBox(point p, BoundingBox B) {
    constexpr float epsilon = 0.0000001f;
    return (p.x >= (B[0].min - epsilon) && p.x <= (B[0].max + epsilon) &&
            p.y >= (B[1].min - epsilon) && p.y <= (B[1].max + epsilon) &&
            p.z >= (B[2].min - epsilon) && p.z <= (B[2].max + epsilon));
}

inline cutInfo bestCutSerial(sequence<event> const &E, range r,
                             range r1, range r2) {
    double flt_max = std::numeric_limits<double>::max();
    index_t n = E.size();
    if(r.max - r.min == 0.0) return cutInfo(flt_max, r.min, n, n);
    float area = 2 * (r1.max - r1.min) * (r2.max - r2.min);
    float orthoPerimeter = 2 * ((r1.max - r1.min) + (r2.max - r2.min));
    index_t inLeft = 0;
    index_t inRight = n / 2;
    float minCost = flt_max;
    index_t k = 0;
    index_t rn = inLeft;
    index_t ln = inRight;
    for(index_t i = 0; i < n; i++) {
        if(KDT_IS_END(E[i])) inRight--;
        float leftLength = E[i].v - r.min;
        float leftSurfaceArea = area + orthoPerimeter * leftLength;
        float rightLength = r.max - E[i].v;
        float rightSurfaceArea = area + orthoPerimeter * rightLength;
        float cost = leftSurfaceArea * inLeft + rightSurfaceArea * inRight;
        if(cost < minCost) {
            rn = inRight; ln = inLeft; minCost = cost; k = i;
        }
        if(KDT_IS_START(E[i])) inLeft++;
    }
    return cutInfo(minCost, E[k].v, ln, rn);
}

inline cutInfo bestCut(sequence<event> const &E, range r,
                       range r1, range r2) {
    index_t n = E.size();
    if(n < minParallelSize) return bestCutSerial(E, r, r1, r2);
    double flt_max = std::numeric_limits<double>::max();
    if(r.max - r.min == 0.0) return cutInfo(flt_max, r.min, n, n);
    float orthogArea = 2 * ((r1.max - r1.min) * (r2.max - r2.min));
    float orthoPerimeter = 2 * ((r1.max - r1.min) + (r2.max - r2.min));

    auto is_end = delayed::tabulate(n, [&](index_t i) -> index_t {
        return KDT_IS_END(E[i]);
    });
    auto end_counts = delayed::scan_inclusive(is_end);

    using rtype = std::tuple<float, index_t, index_t>;
    auto cost_f = [&](std::tuple<index_t, index_t> ni) -> rtype {
        auto [num_ends, i] = ni;
        index_t num_ends_before = num_ends - KDT_IS_END(E[i]);
        index_t inLeft = i - num_ends_before;
        index_t inRight = n/2 - num_ends;
        float leftLength = E[i].v - r.min;
        float leftSurfaceArea = orthogArea + orthoPerimeter * leftLength;
        float rightLength = r.max - E[i].v;
        float rightSurfaceArea = orthogArea + orthoPerimeter * rightLength;
        float cost = leftSurfaceArea * inLeft + rightSurfaceArea * inRight;
        return rtype(cost, num_ends_before, i);
    };
    auto costs = delayed::map(
        delayed::zip(end_counts, parlay::iota<index_t>(n)), cost_f);

    auto min_f = [&](rtype a, rtype b) {
        return (std::get<0>(a) < std::get<0>(b)) ? a : b;
    };
    rtype identity(std::numeric_limits<float>::max(), 0, 0);
    auto [cost, num_ends_before, i] =
        delayed::reduce(costs, parlay::make_monoid(min_f, identity));

    index_t ln = i - num_ends_before;
    index_t rn = n/2 - (num_ends_before + KDT_IS_END(E[i]));
    return cutInfo(cost, E[i].v, ln, rn);
}

using eventsPair = std::pair<sequence<event>, sequence<event>>;

inline eventsPair splitEvents(sequence<range> const &boxes,
                              sequence<event> const &events,
                              float cutOff) {
    index_t n = events.size();
    auto lower = sequence<bool>::uninitialized(n);
    auto upper = sequence<bool>::uninitialized(n);
    parallel_for(0, n, [&](index_t i) {
        index_t b = KDT_GET_INDEX(events[i]);
        lower[i] = boxes[b].min < cutOff;
        upper[i] = boxes[b].max > cutOff;
    });
    return eventsPair(pack(events, lower), pack(events, upper));
}

inline treeNode* generateNode(Boxes &boxes, Events events,
                              BoundingBox B, size_t maxDepth) {
    index_t n = events[0].size();
    if(n <= 2 || maxDepth == 0)
        return treeNode::newNode(std::move(events), n, B);

    cutInfo cuts[3];
    parallel_for(0, 3, [&](size_t d) {
        cuts[d] = bestCut(events[d], B[d], B[(d+1)%3], B[(d+2)%3]);
    }, 10000/n + 1);

    int cutDim = 0;
    for(int d = 1; d < 3; d++)
        if(cuts[d].cost < cuts[cutDim].cost) cutDim = d;

    float cutOff = cuts[cutDim].cutOff;
    float area = boxSurfaceArea(B);
    float bestCost = CT + CL * cuts[cutDim].cost / area;
    float origCost = (float)(n / 2);
    if(bestCost >= origCost ||
        cuts[cutDim].numLeft + cuts[cutDim].numRight > maxExpand * n/2)
        return treeNode::newNode(std::move(events), n, B);

    BoundingBox BBL;
    for(int i = 0; i < 3; i++) BBL[i] = B[i];
    BBL[cutDim] = range(BBL[cutDim].min, cutOff);
    std::array<sequence<event>, 3> leftEvents;

    BoundingBox BBR;
    for(int i = 0; i < 3; i++) BBR[i] = B[i];
    BBR[cutDim] = range(cutOff, BBR[cutDim].max);
    std::array<sequence<event>, 3> rightEvents;

    parallel_for(0, 3, [&](size_t d) {
        eventsPair X = splitEvents(boxes[cutDim], events[d], cutOff);
        leftEvents[d] = std::move(X.first);
        rightEvents[d] = std::move(X.second);
    }, 10000/n + 1);

    index_t nl = leftEvents[0].size();
    index_t nr = rightEvents[0].size();
    for(int d = 1; d < 3; d++) {
        if(leftEvents[d].size() != nl || rightEvents[d].size() != nr) {
            std::cout << "kdTree: mismatched lengths" << std::endl;
            std::abort();
        }
    }

    for(int i = 0; i < 3; i++) events[i] = sequence<event>();
    treeNode *L;
    treeNode *R;
    par_do([&]() { L = generateNode(boxes, std::move(leftEvents),
                                    BBL, maxDepth - 1); },
           [&]() { R = generateNode(boxes, std::move(rightEvents),
                                    BBR, maxDepth - 1); });
    return treeNode::newNode(L, R, cutDim, cutOff, B);
}

inline index_t findRay(ray_t r, sequence<index_t> const &I,
                       triangles<point> const &Tri, BoundingBox B) {
    index_t n = I.size();
    coord tMin = std::numeric_limits<double>::max();
    index_t k = -1;
    for(size_t i = 0; i < n; i++) {
        index_t j = I[i];
        point m[3] = {Tri.P[Tri.T[j][0]], Tri.P[Tri.T[j][1]],
                      Tri.P[Tri.T[j][2]]};
        coord t = rayTriangleIntersect(r, m);
        if(t > 0.0 && t < tMin && inBox(r.o + r.d*t, B)) {
            tMin = t; k = j;
        }
    }
    return k;
}

inline index_t findRay(ray_t r, treeNode* TN, triangs const &Tri) {
    if(TN->isLeaf())
        return findRay(r, TN->triangleIndices, Tri, TN->box);
    point o = r.o;
    vect d = r.d;
    coord oo[3] = {o.x, o.y, o.z};
    coord dd[3] = {d.x, d.y, d.z};
    int k0 = TN->cutDim;
    int k1 = (k0 == 2) ? 0 : k0 + 1;
    int k2 = (k0 == 0) ? 2 : k0 - 1;
    point2d<coord> o_p(oo[k1], oo[k2]);
    vector2d<coord> d_p(dd[k1], dd[k2]);
    coord scale = (TN->cutOff - oo[k0]) / dd[k0];
    point2d<coord> p_i = o_p + d_p * scale;
    range rx = TN->box[k1];
    range ry = TN->box[k2];
    coord d_0 = dd[k0];

    enum { LEFT, RIGHT, BOTH };
    int recurseTo = LEFT;
    if(p_i.x < rx.min)      { if(d_p.x * d_0 > 0) recurseTo = RIGHT; }
    else if(p_i.x > rx.max) { if(d_p.x * d_0 < 0) recurseTo = RIGHT; }
    else if(p_i.y < ry.min) { if(d_p.y * d_0 > 0) recurseTo = RIGHT; }
    else if(p_i.y > ry.max) { if(d_p.y * d_0 < 0) recurseTo = RIGHT; }
    else recurseTo = BOTH;

    if(recurseTo == RIGHT) return findRay(r, TN->right, Tri);
    if(recurseTo == LEFT)  return findRay(r, TN->left,  Tri);
    if(d_0 > 0) {
        index_t t = findRay(r, TN->left, Tri);
        return (t >= 0) ? t : findRay(r, TN->right, Tri);
    } else {
        index_t t = findRay(r, TN->right, Tri);
        return (t >= 0) ? t : findRay(r, TN->left, Tri);
    }
}

inline sequence<index_t> rayCast(triangles<point> const &Tri,
                                 sequence<ray<point>> const &rays) {
    index_t numRays = rays.size();
    Boxes boxes;
    index_t n = Tri.T.size();
    for(int d = 0; d < 3; d++)
        boxes[d] = sequence<range>::uninitialized(n);

    parallel_for(0, n, [&](size_t i) {
        point p0 = Tri.P[Tri.T[i][0]];
        point p1 = Tri.P[Tri.T[i][1]];
        point p2 = Tri.P[Tri.T[i][2]];
        boxes[0][i] = fixRange(std::min(p0.x, std::min(p1.x, p2.x)),
                                std::max(p0.x, std::max(p1.x, p2.x)));
        boxes[1][i] = fixRange(std::min(p0.y, std::min(p1.y, p2.y)),
                                std::max(p0.y, std::max(p1.y, p2.y)));
        boxes[2][i] = fixRange(std::min(p0.z, std::min(p1.z, p2.z)),
                                std::max(p0.z, std::max(p1.z, p2.z)));
    });

    Events events;
    BoundingBox boundingBox;
    for(int d = 0; d < 3; d++) {
        events[d] = tabulate(2 * n, [&](size_t i) -> event {
            return ((i % 2 == 0)
                ? event(boxes[d][i/2].min, i/2, KDT_START)
                : event(boxes[d][i/2].max, i/2, KDT_END));
        });
        sort_inplace(events[d],
                     [](event a, event b) { return a.v < b.v; });
        boundingBox[d] = range(events[d][0].v, events[d][2*n - 1].v);
    }

    size_t recursionDepth = std::min<size_t>(maxRecursionDepth,
                                              parlay::log2_up(n) - 1);
    treeNode* R = generateNode(boxes, std::move(events),
                               boundingBox, recursionDepth);

    auto results = tabulate(numRays, [&](size_t i) -> index_t {
        return findRay(rays[i], R, Tri);
    });

    treeNode::delete_tree(R);
    return results;
}

} // namespace pbbs_kdtree_raycast
