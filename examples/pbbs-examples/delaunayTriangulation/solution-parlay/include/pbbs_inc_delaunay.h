// Vendored from pbbsbench/benchmarks/delaunayTriangulation/incrementalDelaunay
// plus common/{geometry.h, atomics.h, topology.h} (MIT licensed, (c)
// Guy Blelloch and the PBBS team). Concatenated into a single TU and
// wrapped in namespace so parlay scheduler thread_local ODR is happy.
#pragma once

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <math.h>
#include <queue>
#include <vector>
#include <parlay/alloc.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/internal/get_time.h>

namespace pbbs_inc_delaunay {

using namespace std;
using parlay::sequence;
using parlay::delayed_seq;
using parlay::tabulate;
using parlay::reduce;
using parlay::pack;
using parlay::make_monoid;
using parlay::random_permutation;
using parlay::parallel_for;
using parlay::internal::pack_out;
using timer = parlay::internal::timer;

// ===== common/geometry.h =====
#ifndef PBBS_GEOMETRY_H_
#define PBBS_GEOMETRY_H_
using namespace std;

// *************************************************************
//    POINTS AND VECTORS (3d),  2d is below
// *************************************************************


  template <class Coord>
  class point3d;

  template <class Coord>
  class vector3d {
  public:
    using coord = Coord;
    using vector = vector3d;
    using point = point3d<coord>;
    coord x;
    coord y;
    coord z;
    vector3d(coord x, coord y, coord z) : x(x), y(y), z(z) {}
    vector3d() :x(0), y(0), z(0) {}
    vector3d(point p);
    vector3d(parlay::slice<coord*,coord*> p) : x(p[0]), y(p[1]), z(p[2]) {};
    vector operator+(vector op2) {
      return vector(x + op2.x, y + op2.y, z + op2.z);}
    vector operator-(vector op2) {
      return vector(x - op2.x, y - op2.y, z - op2.z);}
    point operator+(point op2);
    vector operator*(coord s) {return vector(x * s, y * s, z * s);}
    vector operator/(coord s) {return vector(x / s, y / s, z / s);}
    coord& operator[] (int i) {return (i==0) ? x : (i==1) ? y : z;}
    coord dot(vector v) {return x * v.x + y * v.y + z * v.z;}
    vector cross(vector v) {
      return vector(y*v.z - z*v.y, z*v.x - x*v.z, x*v.y - y*v.x);
    }
    coord maxDim() {return max(x,max(y,z));}
    void print() {cout << std::setprecision(10) << ":(" << x << "," << y << "," << z << "):";}
    coord Length(void) { return sqrt(x*x+y*y+z*z);}
    coord sqLength(void) { return x*x+y*y+z*z;}
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
    point3d(parlay::slice<coord*,coord*> p) : x(p[0]), y(p[1]), z(p[2]) {};
    void print() {cout << ":(" << x << "," << y << "," << z << "):";}
    vector operator-(point op2) {
      return vector(x - op2.x, y - op2.y, z - op2.z);}
    point operator+(vector op2) {
      return point(x + op2.x, y + op2.y, z + op2.z);}
    point minCoords(point b) {
      return point(min(x,b.x),min(y,b.y),min(z,b.z)); }
    point maxCoords(point b) { 
      return point(max(x,b.x),max(y,b.y),max(z,b.z)); }
    coord& operator[] (int i) {return (i==0) ? x : (i==1) ? y : z;}
    int quadrant(point center) {
      int index = 0;
      if (x > center.x) index += 1;
      if (y > center.y) index += 2;
      if (z > center.z) index += 4;
      return index;
    }
    // returns a point offset by offset in one of 8 directions 
    // depending on dir (an integer from [0..7])
    point offsetPoint(int dir, coord offset) {
      coord xx = x + ((dir & 1) ? offset : -offset);
      coord yy = y + ((dir & 2) ? offset : -offset);
      coord zz = z + ((dir & 4) ? offset : -offset);
      return point(xx, yy, zz);
    }
    point changeCoords(std::vector<coord> v){
      return point(v[0], v[1], v[2]);
    }
    // checks if pt is outside of a box centered at this point with
    // radius hsize
    bool outOfBox(point pt, coord hsize) { 
      return ((x - hsize > pt.x) || (x + hsize < pt.x) ||
	      (y - hsize > pt.y) || (y + hsize < pt.y) ||
	      (z - hsize > pt.z) || (z + hsize < pt.z));
    }
    static const int dim = 3;
  };

  template <class coord>
  inline point3d<coord> vector3d<coord>::operator+(point3d<coord> op2) {
    return point3d<coord>(x + op2.x, y + op2.y, z + op2.z);}

  template <class coord>
  inline vector3d<coord>::vector3d(point3d<coord> p) { x = p.x; y = p.y; z = p.z;}

  // *************************************************************
  //    POINTS AND VECTORS (2d)
  // *************************************************************

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
    vector2d(parlay::slice<coord*,coord*> p) : x(p[0]), y(p[1]) {};
    vector operator+(vector op2) {return vector(x + op2.x, y + op2.y);}
    vector operator-(vector op2) {return vector(x - op2.x, y - op2.y);}
    point operator+(point op2);
    vector operator*(coord s) {return vector(x * s, y * s);}
    vector operator/(coord s) {return vector(x / s, y / s);}
    coord operator[] (int i) {return (i==0) ? x : y;};
    coord dot(vector v) {return x * v.x + y * v.y;}
    coord cross(vector v) { return x*v.y - y*v.x; }  
    coord maxDim() {return max(x,y);}
    void print() {cout << ":(" << x << "," << y << "):";}
    coord Length(void) { return sqrt(x*x+y*y);}
    coord sqLength(void) { return x*x+y*y;}
    static const int dim = 2;
  };

  template <class coord>
  static std::ostream& operator<<(std::ostream& os, const vector3d<coord> v) {
    return os << v.x << " " << v.y << " " << v.z; }

  template <class coord>
  static std::ostream& operator<<(std::ostream& os, const point3d<coord> v) {
    return os << v.x << " " << v.y << " " << v.z;
  }

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
    point2d(parlay::slice<coord*,coord*> p) : x(p[0]), y(p[1]) {};
    void print() {cout << ":(" << x << "," << y << "):";}
    vector operator-(point op2) {return vector(x - op2.x, y - op2.y);}
    point operator+(vector op2) {return point(x + op2.x, y + op2.y);}
    coord operator[] (int i) {return (i==0) ? x : y;};
    point minCoords(point b) { return point(min(x,b.x),min(y,b.y)); }
    point maxCoords(point b) { return point(max(x,b.x),max(y,b.y)); }
    int quadrant(point center) {
      int index = 0;
      if (x > center.x) index += 1;
      if (y > center.y) index += 2;
      return index;
    }
    // returns a point offset by offset in one of 4 directions 
    // depending on dir (an integer from [0..3])
    point offsetPoint(int dir, coord offset) {
      coord xx = x + ((dir & 1) ? offset : -offset);
      coord yy = y + ((dir & 2) ? offset : -offset);
      return point(xx,yy);
    }
    bool outOfBox(point pt, coord hsize) { 
      return ((x - hsize > pt.x) || (x + hsize < pt.x) ||
	      (y - hsize > pt.y) || (y + hsize < pt.y));
    }
    static const int dim = 2;
  };

  template <class coord>
  inline point2d<coord> vector2d<coord>::operator+(point2d<coord> op2) {
    return point2d<coord>(x + op2.x, y + op2.y);}

  template <class coord>
  inline vector2d<coord>::vector2d(point2d<coord> p) { x = p.x; y = p.y;}

  template <class coord>
  static std::ostream& operator<<(std::ostream& os, const vector2d<coord> v) {
    return os << v.x << " " << v.y;}

  template <class coord>
  static std::ostream& operator<<(std::ostream& os, const point2d<coord> v) {
    return os << v.x << " " << v.y; }

  // *************************************************************
  //    GEOMETRY
  // *************************************************************

  // Returns twice the area of the oriented triangle (a, b, c)
  template <class coord>
  inline coord triArea(point2d<coord> a, point2d<coord> b, point2d<coord> c) {
    return (b-a).cross(c-a);
  }

  template <class coord>
  inline coord triAreaNormalized(point2d<coord> a, point2d<coord> b, point2d<coord> c) {
    return triArea(a,b,c)/((b-a).Length()*(c-a).Length());
  }

  // Returns TRUE if the points a, b, c are in a counterclockise order
  template <class coord>
  inline bool counterClockwise(point2d<coord> a, point2d<coord> b, point2d<coord> c) {
    return (b-a).cross(c-a) > 0.0;
  }

  template <class coord>
  inline vector3d<coord> onParabola(vector2d<coord> v) {
    return vector3d<coord>(v.x, v.y, v.x*v.x + v.y*v.y);}

  // Returns TRUE if the point d is inside the circle defined by the
  // points a, b, c. 
  // Projects a, b, c onto a parabola centered with d at the origin
  //   and does a plane side test (tet volume > 0 test)
  template <class coord>
  inline bool inCircle(point2d<coord> a, point2d<coord> b, 
		       point2d<coord> c, point2d<coord> d) {
    vector3d<coord> ad = onParabola(a-d);
    vector3d<coord> bd = onParabola(b-d);
    vector3d<coord> cd = onParabola(c-d);
    return (ad.cross(bd)).dot(cd) > 0.0;
  }

  // returns a number between -1 and 1, such that -1 is out at infinity,
  // positive numbers are on the inside, and 0 is at the boundary
  template <class coord>
  inline double inCircleNormalized(point2d<coord> a, point2d<coord> b, 
				   point2d<coord> c, point2d<coord> d) {
    vector3d<coord> ad = onParabola(a-d);
    vector3d<coord> bd = onParabola(b-d);
    vector3d<coord> cd = onParabola(c-d);
    return (ad.cross(bd)).dot(cd)/(ad.Length()*bd.Length()*cd.Length());
  }

  // *************************************************************
  //    TRIANGLES
  // *************************************************************

  using tri = std::array<int,3>;

  template <class point>
  struct triangles {
    size_t numPoints() {return P.size();};
    size_t numTriangles() {return T.size();}
    parlay::sequence<point> P;
    parlay::sequence<tri> T;
    triangles() {}
    triangles(parlay::sequence<point> P, parlay::sequence<tri> T) 
      : P(std::move(P)), T(std::move(T)) {}
  };

  template <class point>
  struct ray {
    using vector = typename point::vector;
    point o;
    vector d;
    ray(point _o, vector _d) : o(_o), d(_d) {}
    ray() {}
  };

  template<class coord>
  inline coord angle(point2d<coord> a, point2d<coord> b, point2d<coord> c) {
    vector2d<coord> ba = (b-a);
    vector2d<coord> ca = (c-a);
    coord lba = ba.Length();
    coord lca = ca.Length();
    coord pi = 3.14159;
    return 180/pi*acos(ba.dot(ca)/(lba*lca));
  }

  template<class coord>
  inline coord minAngleCheck(point2d<coord> a, point2d<coord> b, point2d<coord> c, coord angle) {
    vector2d<coord> ba = (b-a);
    vector2d<coord> ca = (c-a);
    vector2d<coord> cb = (c-b);
    coord lba = ba.Length();
    coord lca = ca.Length();
    coord lcb = cb.Length();
    coord pi = 3.14159;
    coord co = cos(angle*pi/180.);
    return (ba.dot(ca)/(lba*lca) > co || ca.dot(cb)/(lca*lcb) > co || 
	    -ba.dot(cb)/(lba*lcb) > co);
  }

  template<class coord>
  inline point2d<coord> triangleCircumcenter(point2d<coord> a, point2d<coord> b, point2d<coord> c) {
    vector2d<coord> v1 = b-a;
    vector2d<coord> v2 = c-a;
    vector2d<coord> v11 = v1 * v2.dot(v2);
    vector2d<coord> v22 = v2 * v1.dot(v1);
    return a + vector2d<coord>(v22.y - v11.y, v11.x - v22.x)/(2.0 * v1.cross(v2));
  }
#endif

// ===== common/atomics.h =====
#ifndef PBBS_ATOMICS_H_
#define PBBS_ATOMICS_H_

namespace pbbs {

  template <typename ET>
  inline bool atomic_compare_and_swap(ET* a, ET oldval, ET newval) {
    static_assert(sizeof(ET) <= 8, "Bad CAS length");
    if (sizeof(ET) == 1) {
      uint8_t r_oval, r_nval;
      std::memcpy(&r_oval, &oldval, sizeof(ET));
      std::memcpy(&r_nval, &newval, sizeof(ET));
      return __sync_bool_compare_and_swap(reinterpret_cast<uint8_t*>(a), r_oval, r_nval);
    } else if (sizeof(ET) == 4) {
      uint32_t r_oval, r_nval;
      std::memcpy(&r_oval, &oldval, sizeof(ET));
      std::memcpy(&r_nval, &newval, sizeof(ET));
      return __sync_bool_compare_and_swap(reinterpret_cast<uint32_t*>(a), r_oval, r_nval);
    } else { // if (sizeof(ET) == 8) {
      uint64_t r_oval, r_nval;
      std::memcpy(&r_oval, &oldval, sizeof(ET));
      std::memcpy(&r_nval, &newval, sizeof(ET));
      return __sync_bool_compare_and_swap(reinterpret_cast<uint64_t*>(a), r_oval, r_nval);
    } 
  }

  template <typename E, typename EV>
  inline E fetch_and_add(E *a, EV b) {
    volatile E newV, oldV;
    do {oldV = *a; newV = oldV + b;}
    while (!atomic_compare_and_swap(a, oldV, newV));
    return oldV;
  }

  template <typename E, typename EV>
  inline void write_add(E *a, EV b) {
    //volatile E newV, oldV;
    E newV, oldV;
    do {oldV = *a; newV = oldV + b;}
    while (!atomic_compare_and_swap(a, oldV, newV));
  }

  template <typename E, typename EV>
  inline void write_add(std::atomic<E> *a, EV b) {
    //volatile E newV, oldV;
    E newV, oldV;
    do {oldV = a->load(); newV = oldV + b;}
    while (!std::atomic_compare_exchange_strong(a, &oldV, newV));
  }

  template <typename ET, typename F>
  inline bool write_min(ET *a, ET b, F less) {
    ET c; bool r=0;
    do c = *a;
    while (less(b,c) && !(r=atomic_compare_and_swap(a,c,b)));
    return r;
  }

  template <typename ET, typename F>
  inline bool write_min(std::atomic<ET> *a, ET b, F less) {
    ET c; bool r=0;
    do c = a->load();
    while (less(b,c) && !(r=std::atomic_compare_exchange_strong(a, &c, b)));
    return r;
  }

  template <typename ET, typename F>
  inline bool write_max(ET *a, ET b, F less) {
    ET c; bool r=0;
    do c = *a;
    while (less(c,b) && !(r=atomic_compare_and_swap(a,c,b)));
    return r;
  }

  template <typename ET, typename F>
  inline bool write_max(std::atomic<ET> *a, ET b, F less) {
    ET c; bool r=0;
    do c = a->load();
    while (less(c,b) && !(r=std::atomic_compare_exchange_strong(a, &c, b)));
    return r;
  }
}
#endif

// ===== common/topology.h =====
// This code is part of the Problem Based Benchmark Suite (PBBS)
// Copyright (c) 2011 Guy Blelloch and the PBBS team
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights (to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef _TOPOLOGY_INCLUDED
#define _TOPOLOGY_INCLUDED


using namespace std;

// *************************************************************
//    TOPOLOGY
// *************************************************************

template <typename point>
struct vertex;

// an unoriented triangle with its three neighbors and 3 vertices
//          vtx[1]
//           o 
//           | \ -> ngh[1]
// ngh[2] <- |   o vtx[0]
//           | / -> ngh[0]
//           o
//         vtx[2]
template <typename point>
struct triangle {
  using tri_t = triangle<point>;
  using vtx_t = vertex<point>;
  tri_t *ngh [3];
  vtx_t *vtx [3];
  size_t id;
  bool initialized;
  char bad;  // used to mark badly shaped triangles
  void setT(tri_t *t1, tri_t *t2, tri_t* t3) {
    ngh[0] = t1; ngh[1] = t2; ngh[2] = t3; }
  void setV(vtx_t *v1, vtx_t *v2, vtx_t *v3) {
    vtx[0] = v1; vtx[1] = v2; vtx[2] = v3; }
  int locate(tri_t *t) {
    for (int i=0; i < 3; i++) {
      //cout << t << ", " << ngh[i] << endl;
      if (ngh[i] == t) return i;
    }
    cout<<"did not locate back pointer in triangulation\n";
    abort(); // did not find
  }
  void update(tri_t *t, tri_t *tn) {
    for (int i=0; i < 3; i++)
      if (ngh[i] == t) {ngh[i] = tn; return;}
    cout<<"did not update\n";
    abort(); // did not find
  }
};

// a vertex pointing to an arbitrary triangle to which it belongs (if any)
template <typename point>
struct vertex {
  using point_t = point;
  using tri = triangle<point>;
  point pt;
  tri *t;
  tri *badT;
  int id;
  int reserve;
  size_t counter;
  void print() {
    cout << id << " (" << pt.x << "," << pt.y << ") " << endl;
  }
  vertex(point p, size_t i) : pt(p), id(i), reserve(-1)
			    , badT(NULL)
  {}
  vertex() {}
};

inline int mod3(int i) {return (i>2) ? i-3 : i;}

// a simplex is just an oriented triangle.  An integer (o)
// is used to indicate which of 3 orientations it is in (0,1,2)
// If boundary is set then it represents the edge through t.ngh[o],
// which is a NULL pointer.
template <typename point>
struct simplex {
  using vtx_t = vertex<point>;
  using tri_t = triangle<point>;
  tri_t *t;
  int o;
  bool boundary;
  simplex(tri_t *tt, int oo) : t(tt), o(oo), boundary(0) {}
  simplex(tri_t *tt, int oo, bool _b) : t(tt), o(oo), boundary(_b) {}
  simplex(vtx_t *v1, vtx_t *v2, vtx_t *v3, tri_t *tt) {
    t = tt;
    t->ngh[0] = t->ngh[1] = t->ngh[2] = NULL;
    t->vtx[0] = v1; v1->t = t;
    t->vtx[1] = v2; v2->t = t;
    t->vtx[2] = v3; v3->t = t;
    o = 0;
    boundary = 0;
  }
  simplex() : t(nullptr), o(0), boundary(false) {}

  void print() {
    if (t == NULL) cout << "NULL simp" << endl;
    else {
      cout << "vtxs=";
      for (int i=0; i < 3; i++) 
	if (t->vtx[mod3(i+o)] != NULL)
	  cout << t->vtx[mod3(i+o)]->id << " (" <<
	    t->vtx[mod3(i+o)]->pt.x << "," <<
	    t->vtx[mod3(i+o)]->pt.y << ") ";
	else cout << "NULL ";
      cout << endl;
    }
  }

  simplex across() {
    tri_t *to = t->ngh[o];
    if (to != NULL) return simplex(to,to->locate(t));
    else return simplex(t,o,1);
  }

  // depending on initial triangle this could be counterclockwise
  simplex rotClockwise() { return simplex(t,mod3(o+1));}

  bool valid() {return (!boundary);}
  bool isTriangle() {return (!boundary);}
  bool isBoundary() {return boundary;}
  
  vtx_t *firstVertex() {return t->vtx[o];}

  bool inCirc(vtx_t *v) {
    if (boundary || t == NULL) return 0;
    return inCircle(t->vtx[0]->pt, t->vtx[1]->pt, 
		    t->vtx[2]->pt, v->pt);
  }

  // the angle facing the across edge
  double farAngle() {
    return angle(t->vtx[mod3(o+1)]->pt,
		 t->vtx[o]->pt,
		 t->vtx[mod3(o+2)]->pt);
  }

  bool outside(vtx_t *v) {
    if (boundary || t == NULL) return 0;
    return counterClockwise(t->vtx[mod3(o+2)]->pt, v->pt, t->vtx[o]->pt);
  }

  // flips two triangles and adjusts neighboring triangles
  void flip() { 
    simplex s = across();
    int o1 = mod3(o+1);
    int os1 = mod3(s.o+1);

    tri_t *t1 = t->ngh[o1];
    tri_t *t2 = s.t->ngh[os1];
    vtx_t *v1 = t->vtx[o1];
    vtx_t *v2 = s.t->vtx[os1];

    t->vtx[o]->t = s.t;
    t->vtx[o] = v2;
    t->ngh[o] = t2;
    if (t2 != NULL) t2->update(s.t,t);
    t->ngh[o1] = s.t;

    s.t->vtx[s.o]->t = t;
    s.t->vtx[s.o] = v1;
    s.t->ngh[s.o] = t1;
    if (t1 != NULL) t1->update(t,s.t);
    s.t->ngh[os1] = t;
  }

  // splits the triangle into three triangles with new vertex v in the middle
  // updates all neighboring simplices
  // ta0 and ta0 are pointers to the memory to use for the two new triangles
  void split(vtx_t* v, tri_t* ta0, tri_t* ta1) {
    v->t = t;
    tri_t *t1 = t->ngh[0]; tri_t *t2 = t->ngh[1]; tri_t *t3 = t->ngh[2];
    vtx_t *v1 = t->vtx[0]; vtx_t *v2 = t->vtx[1]; vtx_t *v3 = t->vtx[2];
    t->ngh[1] = ta0;        t->ngh[2] = ta1;
    t->vtx[1] = v;
    ta0->setT(t2,ta1,t);  ta0->setV(v2,v,v1);
    ta1->setT(t3,t,ta0);  ta1->setV(v3,v,v2);
    if (t2 != NULL) t2->update(t,ta0);      
    if (t3 != NULL) t3->update(t,ta1);
    v2->t = ta0;
  }

  // splits one of the boundaries of a triangle to form two triangles
  // the orientation dictates which edge to split (i.e., t.ngh[o])
  // ta is a pointer to memory to use for the new triangle
  void splitBoundary(vtx_t* v, tri_t* ta) {
    int o1 = mod3(o+1);
    int o2 = mod3(o+2);
    if (t->ngh[o] != NULL) {
      cout << "simplex::splitBoundary: not boundary" << endl; abort();}
    v->t = t;
    tri_t *t2 = t->ngh[o2];
    vtx_t *v1 = t->vtx[o1]; vtx_t *v2 = t->vtx[o2];
    t->ngh[o2] = ta;   t->vtx[o2] = v;
    ta->setT(t2,NULL,t);  ta->setV(v2,v,v1);
    if (t2 != NULL) t2->update(t,ta);      
    v2->t = t;
  }

  // given a vtx v, extends a boundary edge (t.ngh[o]) with an extra 
  // triangle on that edge with apex v.  
  // ta is used as the memory for the triangle
  simplex extend(vtx_t* v, tri_t* ta) {
    if (t->ngh[o] != NULL) {
      cout << "simplex::extend: not boundary" << endl; abort();}
    t->ngh[o] = ta;
    ta->setV(t->vtx[o], t->vtx[mod3(o+2)], v);
    ta->setT(NULL,t,NULL);
    v->t = ta;
    return simplex(ta,0);
  }

};

// this might or might not be needed
// void topologyFromTriangles(triangles<point2d> Tri, vtx** vr, tri** tr);

#endif // _TOPOLOGY_INCLUDED

// ===== oct_tree.h (variant-local) =====
// This code is part of the Problem Based Benchmark Suite (PBBS)
// Copyright (c) 2011 Guy Blelloch and the PBBS team
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights (to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


// vtx must support v->pt
// and v->pt must support pt.dimension(), pt[i],
//    (pt1 - pt2).Length(), pt1 + (pt2 - pt3)
//    pt1.minCoords(pt2), pt1.maxCoords(pt2),
template <typename vtx>
struct oct_tree {

  using point = typename vtx::point_t;
  using uint = unsigned int;
  using box = std::pair<point,point>;
  using indexed_point = std::pair<size_t,vtx*>;
  using slice_t = decltype(make_slice(parlay::sequence<indexed_point>()));
  using slice_v = decltype(make_slice(parlay::sequence<vtx*>()));

  struct node {

  public:
    using leaf_seq = parlay::sequence<vtx*>;
    point center() {return centerv;}
    box Box() {return b;}
    size_t size() {return n;}
    bool is_leaf() {return L == nullptr;}
    node* Left() {return L;}
    node* Right() {return R;}
    node* Parent() {return parent;}
    leaf_seq& Vertices() {return P;};

    // construct a leaf node with a sequence of points directly in it
    node(slice_t Pts) {
      n = Pts.size();
      parent = nullptr;

      // strips off the integer tag, no longer needed
      P = leaf_seq(n);
      for (int i = 0; i < n; i++)
	P[i] = Pts[i].second;  
      L = R = nullptr;
      b = get_box(P);
      set_center();
    }

    // construct an internal binary node
    node(node* L, node* R) : L(L), R(R) {
      parent = nullptr;
      b = box(L->b.first.minCoords(R->b.first),
	      L->b.second.maxCoords(R->b.second));
      n = L->size() + R->size();
      set_center();
    }
    
    static node* new_leaf(slice_t Pts) {
      node* r = alloc_node();
      new (r) node(Pts);
      return r;
    }

    static node* new_node(node* L, node* R) {
      node* nd = alloc_node();
      new (nd) node(L, R);
      // both children point to this node as their parent
      L->parent = R->parent = nd;
      return nd;
    }
    
    ~node() {
      // need to collect in parallel
      parlay::par_do_if(n > 1000,
			[&] () { delete_tree(L);},
			[&] () { delete_tree(R);});
    }

    parlay::sequence<vtx*> flatten() {
      parlay::sequence<vtx*> r(n);
      flatten_rec(this, parlay::make_slice(r));
      return r;
    }

    // map a function f(p,node_ptr) over the points, passing
    // in the point, and a pointer to the leaf node it is in.
    // f should return void
    template <typename F>
    void map(F f) {
      if (is_leaf())
	for (int i=0; i < size(); i++) f(P[i],this);
      else {
	parlay::par_do_if(n > 1000,
			  [&] () {L->map(f);},
			  [&] () {R->map(f);});
      }
    }

    size_t depth() {
      if (is_leaf()) return 0;
      else {
	size_t l, r;
	parlay::par_do_if(n > 1000,
			  [&] () {l = L->depth();},
			  [&] () {r = R->depth();});
	return 1 + std::max(l,r);
      }
    }

    // recursively frees the tree
    static void delete_tree(node* T) {
      if (T != nullptr) {
	T->~node();
	node::free_node(T);
      }
    }

    // disable copy and move constructors/assignment since
    // they are dangerous with with free.
    node(const node&) = delete;
    node(node&&) = delete;
    node& operator=(node const&) = delete;
    node& operator=(node&&) = delete;

    static node* alloc_node();
    static void free_node(node* T);

  private:

    size_t n;
    node *parent;
    node *L;
    node *R;
    box b;
    point centerv;
    leaf_seq P;

    void set_center() {			   
      centerv = b.first + (b.second-b.first)/2;
    }

    static void flatten_rec(node *T, slice_v R) {
      if (T->is_leaf())
	for (int i=0; i < T->size(); i++)
	  R[i] = T->P[i];
      else {
	size_t n_left = T->L->size();
	size_t n = T->size();
	parlay::par_do_if(n > 1000,
	  [&] () {flatten_rec(T->L, R.cut(0, n_left));},
	  [&] () {flatten_rec(T->R, R.cut(n_left, n));});
      }
    }
  };
  
  // A unique pointer to a tree node to ensure the tree is
  // destructed when the pointer is, and that  no copies are made.
  struct delete_tree {void operator() (node *T) const {node::delete_tree(T);}};
  using tree_ptr = std::unique_ptr<node,delete_tree>;

  // build a tre given a sequence of pointers to points
  template <typename Seq>
  static tree_ptr build(Seq &P) {
    timer t("oct_tree",false);
    int dims = (P[0]->pt).dimension();
    auto pts = tag_points(P);
    t.next("tag");
    node* r = build_recursive(make_slice(pts), dims*(key_bits/dims));
    t.next("build");
    return tree_ptr(r);
  }

private:
  constexpr static int key_bits = 64;
  
  // takes a point, rounds each coordinate to an integer, and interleaves
  // the bits into "key_bits" total bits.
  // min_point is the minimmum x,y,z coordinate for all points
  // delta is the largest range of any of the three dimensions
  static size_t interleave_bits(point p, point min_point, double delta) {
    int dim = p.dimension();
    int bits = key_bits/dim;
    uint maxval = (((size_t) 1) << bits) - 1;
    uint ip[dim];
    for (int i = 0; i < dim; i++) 
      ip[i] = floor(maxval * (p[i] - min_point[i])/delta);

    size_t r = 0;
    int loc = 0;
    for (int i =0; i < bits; i++)
      for (int d = 0; d < dim; d++) 
	r = r | (((ip[d] >> i) & (size_t) 1) << (loc++));
    return r;
  }

  // generates a box consisting of a lower left corner,
  // and an upper right corner.
  template <typename Seq>
  static box get_box(Seq &V) { // parlay::sequence<vtx*> &V) {
    size_t n = V.size();
    auto minmax = [&] (box x, box y) {
      return box(x.first.minCoords(y.first),
		 x.second.maxCoords(y.second));};

    // uses a delayed sequence to avoid making a copy
    auto pts = parlay::delayed_seq<box>(n, [&] (size_t i) {
	return box(V[i]->pt, V[i]->pt);});
    box identity = pts[0];
    return parlay::reduce(pts, parlay::make_monoid(minmax,identity));
  }

  // tags each point (actually a pointer to it), with an interger
  // consisting of the interleaved bits for the x,y,z coordinates.
  // Also sorts based the integer.
  static parlay::sequence<indexed_point> tag_points(parlay::sequence<vtx*> &V) {
    timer t("tag",false);
    size_t n = V.size();
    int dims = (V[0]->pt).dimension();

    // find box around points, and size along largest axis
    box b = get_box(V);
    double Delta = 0;
    for (int i = 0; i < dims; i++) 
      Delta = std::max(Delta, b.second[i] - b.first[i]);
    t.next("get box");
    
    auto points = parlay::delayed_seq<indexed_point>(n, [&] (size_t i) -> indexed_point {
	return std::pair(interleave_bits(V[i]->pt, b.first, Delta), V[i]);
      });
    
    auto less = [] (indexed_point a, indexed_point b) {
      return a.first < b.first;};
    
    auto x = parlay::sort(points, less);
    t.next("tabulate and sort");
    return x;
  }

  // each point is a pair consisting of an interleave integer along with
  // the pointer to the point.   The bit specifies which bit of the integer
  // we are working on (starts at top, and goes down).
  static node* build_recursive(slice_t Pts, int bit) {
    size_t n = Pts.size();
    if (n == 0) abort();
    int cutoff = 20;

    // if run out of bit, or small then generate a leaf
    if (bit == 0 || n < cutoff) {
      return node::new_leaf(Pts);
    } else {

      // the following defines a less based on the bit
      size_t val = ((size_t) 1) << (bit - 1);
      size_t mask = (bit == 64) ? ~((size_t) 0) : ~(~((size_t) 0) << bit);
      auto less = [&] (indexed_point x) {
	return (x.first & mask) < val;
      };
      // and then we binary search for the cut point
      size_t pos = parlay::internal::binary_search(Pts, less);

      // if all points are on one side, then move onto the next bit
      if (pos == 0 || pos == n) 
	return build_recursive(Pts, bit - 1);

      // otherwise recurse on the two parts, also moving to next bit
      else {
	node *L, *R;
	parlay::par_do_if(n > 1000,
           [&] () {L = build_recursive(Pts.cut(0, pos), bit - 1);},
	   [&] () {R = build_recursive(Pts.cut(pos, n), bit - 1);});
	return node::new_node(L,R);
      }
    }
  }
  
};

template <typename vtx>
parlay::type_allocator<typename oct_tree<vtx>::node> node_allocator;

template <typename vtx>
typename oct_tree<vtx>::node* oct_tree<vtx>::node::alloc_node() { return node_allocator<vtx>.alloc();}

template <typename vtx>
void oct_tree<vtx>::node::free_node(node* T) { node_allocator<vtx>.free(T);}
  

// ===== neighbors.h (variant-local) =====
// This code is part of the Problem Based Benchmark Suite (PBBS)
// Copyright (c) 2011 Guy Blelloch and the PBBS team
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights (to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#define report_stats false

// A k-nearest neighbor structure
// requires vertexT to have pointT and vectT typedefs
template <class vtx, int max_k>
struct k_nearest_neighbors {
  using point = typename vtx::point_t;
  using fvect = typename point::vector;
  using o_tree = oct_tree<vtx>;
  using node = typename o_tree::node;
  using tree_ptr = typename o_tree::tree_ptr;

  tree_ptr tree;

  // generates the search structure
  k_nearest_neighbors(parlay::sequence<vtx*> &V) {
    tree = o_tree::build(V);
  }

  // returns the vertices in the search structure, in an
  //  order that has spacial locality
  parlay::sequence<vtx*> vertices() {
    return tree->flatten();
  }

  struct kNN {
    vtx *vertex;  // the vertex for which we are trying to find a NN
    vtx *neighbors[max_k];  // the current k nearest neighbors (nearest last)
    double distances[max_k]; // distance to current k nearest neighbors
    int k;
    int dimensions;
    size_t leaf_cnt;
    size_t internal_cnt;
    kNN() {}

    // returns the ith smallest element (0 is smallest) up to k-1
    vtx* operator[] (const int i) { return neighbors[k-i-1]; }

    kNN(vtx *p, int kk) {
      if (kk > max_k) {
	std::cout << "k too large in kNN" << std::endl;
	abort();}
      k = kk;
      vertex = p;
      dimensions = p->pt.dimension();
      leaf_cnt = internal_cnt = 0;
      // initialize nearest neighbors to point to Null with
      // distance "infinity".
      for (int i=0; i<k; i++) {
	neighbors[i] = (vtx*) NULL; 
	distances[i] = numeric_limits<double>::max();
      }
    }

    // if p is closer than neighbors[0] then swap it in
    void update_nearest(vtx *other) { 
      auto dist = (vertex->pt - other->pt).Length();
      if (dist < distances[0]) {
	neighbors[0] = other;
	distances[0] = dist;
	for (int i = 1;
	     i < k && distances[i-1] < distances[i];
	     i++) {
	  swap(distances[i-1], distances[i]);
	  swap(neighbors[i-1], neighbors[i]); }
      }
    }

    bool within_epsilon_box(node* T, double epsilon) {
      auto box = T->Box();
      bool result = true;
      for (int i = 0; i < dimensions; i++) {
	result = (result &&
		  (box.first[i] - epsilon < vertex->pt[i]) &&
		  (box.second[i] + epsilon > vertex->pt[i]));
      }
      return result;
    }

    double distance(node* T) {
      return (T->center() - vertex->pt).Length();
    }
    
    // looks for nearest neighbors for this->vertex in Tree node T
    void k_nearest_rec(node* T) {
      if (report_stats) internal_cnt++;
      if (within_epsilon_box(T, distances[0])) {
	if (T->is_leaf()) {
	  if (report_stats) leaf_cnt++;
	  auto &Vtx = T->Vertices();
	  for (int i = 0; i < T->size(); i++)
	    if (Vtx[i] != vertex) update_nearest(Vtx[i]);
	} else if (distance(T->Left()) < distance(T->Right())) {
	  k_nearest_rec(T->Left());
	  k_nearest_rec(T->Right());
	} else {
	  k_nearest_rec(T->Right());
	  k_nearest_rec(T->Left());
	}
      }
    }

    // finds a point that is vaguely near
    void near_rec(node* T) {
      if (T->is_leaf()) {
	auto &Vtx = T->Vertices();
	for (int i = 0; i < T->size(); i++)
	  if (Vtx[i] != vertex) update_nearest(Vtx[i]);
      } else if (distance(T->Left()) < distance(T->Right())) {
	near_rec(T->Left());
      } else {
	near_rec(T->Right());
      }
    }

  };

  void k_nearest(vtx *p, int k) {
    kNN nn(p,k);
    nn.k_nearest_rec(tree.get());
    if (report_stats) p->counter = nn.internal_cnt;
    for (int i=0; i < k; i++)
      p->ngh[i] = nn[i];
  }
  
  vtx* nearest(vtx *p) {
    kNN nn(p,1);
    nn.k_nearest_rec(tree.get());
    if (report_stats) p->counter = nn.internal_cnt;
    return nn[0];
  }

  vtx* near(vtx *p) {
    kNN nn(p,1);
    nn.near_rec(tree.get());
    return nn[0];
  }

};

// find the k nearest neighbors for all points in tree
// places pointers to them in the .ngh field of each vertex
template <int max_k, class vtx>
void ANN(parlay::sequence<vtx*> &v, int k) {
  timer t("ANN",report_stats);

  {
    using knn_tree = k_nearest_neighbors<vtx, max_k>;
    knn_tree T(v);
    t.next("build tree");

    if (report_stats) 
      std::cout << "depth = " << T.tree->depth() << std::endl;

    // this reorders the vertices for locality
    parlay::sequence<vtx*> vr = T.vertices();
    t.next("flatten tree");
  
    // find nearest k neighbors for each point
    parlay::parallel_for (0, v.size(), [&] (size_t i) {
					 T.k_nearest(vr[i], k);}, 1);

    t.next("try all");
    if (report_stats) {
      auto s = parlay::delayed_seq<size_t>(v.size(), [&] (size_t i) {return v[i]->counter;});
      size_t i = parlay::max_element(s) - s.begin();
      size_t sum = parlay::reduce(s);
      std::cout << "max internal = " << s[i] 
		<< ", average internal = " << sum/((double) v.size()) << std::endl;
      t.next("stats");
    }
  }
  t.next("delete tree");
}

// ===== delaunay.C (variant body) =====
// Type alias `point` lives in our namespace so we can drive the
// algorithm with point2d<double>.
using coord = double;
using point = point2d<coord>;

// if on verifies the Delaunay is correct 
#define CHECK 0

using vertex_t = vertex<point>;
using simplex_t = simplex<point>;
using triang_t = triangle<point>;
using vect = typename point::vector;

template <typename point>
struct Qs {
  vector<vertex<point>*> vertexQ;
  vector<simplex<point>> simplexQ;
  Qs() {
    vertexQ.reserve(50);
    simplexQ.reserve(50);
  }
};
using Qs_t = Qs<point>;

// *************************************************************
//    ROUTINES FOR FINDING AND INSERTING A NEW POINT
// *************************************************************

// Finds a vertex (p) in a mesh starting at any triangle (start)
// Requires that the mesh is properly connected and convex
simplex_t find(vertex_t *p, simplex_t start) {
  simplex_t t = start;
  while (1) {
    int i;
    for (i=0; i < 3; i++) {
      t = t.rotClockwise();
      if (t.outside(p)) {t = t.across(); break;}
    }
    if (i==3) return t;
    if (!t.valid()) return t;
  }
}

// Recursive routine for finding a cavity across an edge with
// respect to a vertex p.
// The simplex has orientation facing the direction it is entered.
//
//         a
//         | \ --> recursive call
//   p --> |T c 
// enter   | / --> recursive call
//         b
//
//  If p is in circumcircle of T then 
//     add T to simplexQ, c to vertexQ, and recurse
void findCavity(simplex_t t, vertex_t *p, Qs_t *q) {
  if (t.inCirc(p)) {
    q->simplexQ.push_back(t);
    t = t.rotClockwise();
    findCavity(t.across(), p, q);
    q->vertexQ.push_back(t.firstVertex());
    t = t.rotClockwise();
    findCavity(t.across(), p, q);
  }
}

// Finds the cavity for v and tries to reserve vertices on the 
// boundary (v must be inside of the simplex t)
// The boundary vertices are pushed onto q->vertexQ and
// simplices to be deleted on q->simplexQ (both initially empty)
// It makes no side effects to the mesh other than to X->reserve
void reserve_for_insert(vertex_t *v, simplex_t t, Qs_t *q) {
  // each iteration searches out from one edge of the triangle
  for (int i=0; i < 3; i++) {
    q->vertexQ.push_back(t.firstVertex());
    findCavity(t.across(), v, q);
    t = t.rotClockwise();
  }
  // the maximum id new vertex that tries to reserve a boundary vertex 
  // will have its id written.  reserve starts out as -1
  for (size_t i = 0; i < q->vertexQ.size(); i++) {
    //cout << "trying to reserve: " << (q->vertexQ)[i]->reserve << ", " << v->id << endl;
    pbbs::write_max(&((q->vertexQ)[i]->reserve), v->id, std::less<int>());
  }
}

// checks if v "won" on all adjacent vertices and inserts point if so
bool insert(vertex_t *v, simplex_t t, Qs_t *q) {
  bool flag = 0;
  for (size_t i = 0; i < q->vertexQ.size(); i++) {
    vertex_t* u = (q->vertexQ)[i];
    //cout << u->id << ", " << u->reserve << " : " << v->id << endl;
    if (u->reserve == v->id) u->reserve = -1; // reset to -1
    else flag = 1; // someone else with higher priority reserved u
  }
  if (!flag) {
    triang_t* t1 = v->t;  // the memory for the two new triangles
    triang_t* t2 = t1 + 1;  
    // the following 3 lines do all the side effects to the mesh.
    t.split(v, t1, t2);
    //cout << "just split: " << q->simplexQ.size() << endl;
    for (size_t i = 0; i<q->simplexQ.size(); i++) {
      //cout << "flipping: " << i << endl;
      (q->simplexQ)[i].flip();
    }
  }
  q->simplexQ.clear();
  q->vertexQ.clear();
  return flag;
}

// *************************************************************
//    CHECKING THE TRIANGULATION
// *************************************************************

void check_delaunay(sequence<triang_t> &Triangles, size_t boundary_size) {
  size_t n = Triangles.size();
  sequence<size_t> boundary_count(n, 0);
  parallel_for (0, n, [&] (size_t i) {
    if (Triangles[i].initialized >= 0) {
      simplex_t t = simplex(&Triangles[i], 0);
      for (int i=0; i < 3; i++) {
	simplex_t a = t.across();
	if (a.valid()) {
	  vertex_t* v = a.rotClockwise().firstVertex();
	  if (!t.outside(v)) {
	    if(false) cout << "Inside Out: "; v->pt.print(); t.print();}
	  if (t.inCirc(v)) {
	    if(false) cout << "In Circle Violation: "; v->pt.print(); t.print(); }
	} else boundary_count[i]++;
	t = t.rotClockwise();
      }
    } });
  if (boundary_size != reduce(boundary_count))
    if(false) cout << "Wrong boundary size: should be " << boundary_size 
	 << " is " << reduce(boundary_count) << endl;
}

// *************************************************************
//    CREATING A BOUNDING CIRCULAR REGION AND FILL WITH INITIAL SIMPLICES
// *************************************************************

// P is the set of points to bound and n the number
// boundary_size is the number of points to put on the boundary
// V is a sequence of vertices, which the new vertices are added to, at end
// T is a sequence of triangles, which the new triangles are added to, at end
// one of the triangles is returned as an ordered simplex
void generate_boundary(sequence<point> const &P,
		       size_t boundary_size,
		       sequence<vertex_t> &V,
		       sequence<triang_t> &T) {

  size_t n = P.size();
  auto min = [] (point x, point y) { return x.minCoords(y);};
  auto max = [] (point x, point y) { return x.maxCoords(y);};
  point identity = P[0];
  point min_corner = reduce(P, make_monoid(min, identity));
  point max_corner = reduce(P, make_monoid(max, identity));
  double size = (max_corner-min_corner).Length();
  double stretch = 10.0;
  double radius = stretch*size;
  point center = max_corner + (max_corner-min_corner)/2.0;
  double pi = 3.14159;

  // Generate the bounding points on a circle far outside the bounding box
  for (size_t i=0; i < boundary_size; i++) {
    double x = radius * cos(2*pi*((float) i)/((float) boundary_size));
    double y = radius * sin(2*pi*((float) i)/((float) boundary_size));
    point pt = center + vect(x,y);
    V[i+n] = vertex_t(pt, i + n);
  }

  // Fill with triangles (boundary_size - 2 total)
  simplex_t s = simplex_t(&V[0+n], &V[1+n], &V[2+n], &T[0 + 2*n]); 
  for (size_t i = 3; i < boundary_size; i++)
    s = s.extend(&V[i+n], &T[i - 2 + 2*n]); 
  //return s;
}


// *************************************************************
//    MAIN LOOP
// *************************************************************

void incrementally_add_points(sequence<vertex_t*> v, vertex_t* start) {
  size_t n = v.size();
  
  // various structures needed for each parallel insertion
  size_t max_block_size = (size_t) (n/1000) + 1; // maximum number to try in parallel ??
									  
  sequence<vertex_t*> done(n);  // holds all completed vertices
  sequence<vertex_t*> buffer(max_block_size);// initially empty, holds leftofvers from prev round
  sequence<vertex_t*> remain;  // holds remaining from previous round
  sequence<simplex_t> t(max_block_size);
  sequence<bool> flags(max_block_size);
  auto VQ = tabulate(max_block_size, [&] (size_t i) -> Qs_t {return Qs_t();});
  
  // create a point location structure
  using KNN = k_nearest_neighbors<vertex_t,1>;
  sequence<vertex_t*> init(1,start);
  KNN knn = KNN(init);

  size_t num_done = 0;
  size_t rounds = 0;
  size_t num_failed = 0;
  size_t num_remain = 0;
  size_t num_next_rebuild = 100;
  size_t multiplier = 10;

  while (num_done < n) {
    //if (rounds > 3) abort();

    // every once in a while create a new point location
    // structure using all points inserted so far
    if (num_done >= num_next_rebuild && num_done <= n/multiplier) {
      // cout << "size = " << num_done << endl;
      auto vtxs = parlay::to_sequence(done.cut(0,num_done));
      knn = KNN(vtxs); // should change to pass slice
      num_next_rebuild *= multiplier;
    }

    // determine how many vertices to try in parallel
    size_t num_round = std::min(std::min(1 + num_done/50, n-num_done), max_block_size);
    // 50 is pulled out of a hat
    // cout << "enter loop: " << rounds << ", " << num_round << ", " << num_done << ", " << num_remain << endl;
    
    // for trial vertices find containing triangle, determine cavity 
    // and reserve vertices on boundary of cavity
    parallel_for (0, num_round, [&] (size_t j) {
      buffer[j] = (j < num_remain) ? remain[j] : v[j + num_done];
      vertex_t *u = knn.nearest(buffer[j]);
      t[j] = find(buffer[j], simplex(u->t, 0));
      reserve_for_insert(buffer[j], t[j], &VQ[j]);});
    
    // For trial vertices check if they own their boundary and
    // update mesh if so.  flags[i] is 1 if failed (need to retry)
    parallel_for (0, num_round, [&] (size_t j) {
				  flags[j] = insert(buffer[j], t[j], &VQ[j]);});
    //cout << "here 3: " << flags[0] << endl;

    // Pack failed vertices back onto Q and successful
    // ones up above (needed for point location structure)
    remain = pack(buffer.cut(0,num_round), flags.cut(0,num_round));
    num_remain = remain.size();
    size_t num_done_in_round = num_round - num_remain;
    //cout << "finished " << num_done_in_round << " in round " << rounds << ", " << num_remain << endl;
    auto not_flags = delayed_seq<bool>(num_round, [&] (size_t i) -> bool {return !flags[i];});
    //auto not_flags = tabulate(num_round, [&] (size_t i) -> bool {return !flags[i];});
    pack_out(buffer.cut(0,num_round), not_flags, done.cut(num_done, num_done + num_done_in_round));

    num_failed += num_remain;
    num_done += num_done_in_round;
    rounds++;
  }

  //cout << "n=" << n << "  Total retries=" << failed
  //     << "  Total rounds=" << rounds << endl;
}


// *************************************************************
//    DRIVER
// *************************************************************

triangles<point> delaunay(sequence<point> &P) {
  timer t("delaunay", false);
  t.start();
  size_t boundary_size = 10;
  size_t n = P.size();

  // All vertices needed
  size_t num_vertices = n + boundary_size;
  auto Vertices = sequence<vertex_t>(num_vertices);

  // All triangles needed
  size_t boundary_triangles = (boundary_size - 2);
  size_t num_triangles = 2 * n + boundary_triangles;
  auto Triangles = sequence<triang_t>(num_triangles); 

  // random permutation to put points in a random order
  sequence<size_t> perm = random_permutation<size_t>(n);
  parallel_for(0, n, [&] (size_t i) {
    Vertices[perm[i]] = vertex_t(P[i], i);});

  // give two triangles to each non-boundary vertex
  parallel_for (0, n, [&] (size_t i) {
    Vertices[i].t = &Triangles[2*i];});
  
  // generate boundary points and fill with simplices
  // The boundary points and simplices go at the end,
  // starting at n of Vertices, and 2n of Triangles
  generate_boundary(P, boundary_size, Vertices, Triangles);

  // pointers to first n vertices
  auto V = tabulate(n, [&] (size_t i) -> vertex_t* {
			 return &Vertices[i];});
  vertex_t* v0 = &Vertices[n];
  
  t.next("initialize");
  // main loop to add all points

  incrementally_add_points(V, v0);
  t.next("add points");

  if (CHECK) check_delaunay(Triangles, boundary_size);

  // just the three corner ids for each triangle
  auto result_triangles = tabulate(num_triangles, [&] (size_t i) -> tri {
    vertex_t** vtx = Triangles[i].vtx;
    tri r = {(int) vtx[0]->id, (int) vtx[1]->id, (int) vtx[2]->id};
    return r;});

  // just the points, including the added boundary points
  auto result_points = tabulate(num_vertices, [&] (size_t i) {
    point r = (i < n) ? P[i] : Vertices[i].pt;
    //cout << r[0] << ", " << r[1] << endl;
    return r;});

  t.next("generate output");

  return triangles<point>(result_points, result_triangles);
}

} // namespace pbbs_inc_delaunay
