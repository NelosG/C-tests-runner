// Vendored from pbbsbench/benchmarks/delaunayRefine/incrementalRefine plus
// common/{geometry.h, atomics.h, topology.h, topology_from_triangles.h}.
// (MIT licensed, (c) Guy Blelloch and the PBBS team.)  Single TU in a
// namespace to keep parlay scheduler thread_local ODR happy.
#pragma once

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <math.h>
#include <vector>
#include <parlay/hash_table.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <parlay/internal/get_time.h>

namespace pbbs_inc_refine {

using namespace std;
using parlay::sequence;
using parlay::tabulate;
using parlay::parallel_for;
using parlay::pack;
using parlay::pack_index;
using parlay::hash64;
using parlay::hashtable;
using timer = parlay::internal::timer;

using coord = double;
// point/triangles<> defined inside geometry.h below.

// ===== common/geometry.h =====
#ifndef PBBS_GEOMETRY_H_REFINE_
#define PBBS_GEOMETRY_H_REFINE_
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
#ifndef PBBS_ATOMICS_H_REFINE_
#define PBBS_ATOMICS_H_REFINE_

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

#ifndef _TOPOLOGY_INCLUDED_REFINE_
#define _TOPOLOGY_INCLUDED_REFINE_


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

#endif // _TOPOLOGY_INCLUDED_REFINE_

// ===== refine.h (free type alias for `point`) =====
// Must come BEFORE topology_from_triangles.h which uses `point`.
using point = point2d<coord>;
using vect = typename point::vector;

// ===== common/topology_from_triangles.h =====
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


using parlay::parallel_for;
using parlay::hash64;
using parlay::sequence;
using parlay::tabulate;
using parlay::hashtable;

using std::pair;
using std::cout;
using std::endl;
using std::less;

using triang_t = triangle<point>;
using vertex_t = vertex<point>;
using simplex_t = simplex<point>;
using index_t = int;
using index_pair = pair<index_t,index_t>;
using edge = pair<index_pair, triang_t*>;

// Hash table to store skinny triangles
struct hashEdges {
  using kType = index_pair;
  using eType = edge*;
  eType empty() {return NULL;}
  kType getKey(eType v) { return v->first;}
  size_t hash(kType s) { return hash64(s.first)+3*(hash64(s.second)); }
  int cmp(kType s1, kType s2) {
    return ((s1.first > s2.first) ? 1 : 
	    (s1.first < s2.first) ? -1 : 
	    (s1.second > s2.second) ? 1 :
	    (s1.second < s2.second) ? -1 : 0);
  }
  bool cas(eType* p, eType o, eType n) {
    return pbbs::atomic_compare_and_swap(p, o, n);
  }
  bool replaceQ(eType s, eType s2) {return 0;}
};

using EdgeTable = hashtable<hashEdges>;

EdgeTable makeEdgeTable(size_t m) {
  return EdgeTable(m,hashEdges());}

std::pair<sequence<triang_t>,sequence<vertex_t>>
topology_from_triangles(triangles<point> &Tri, size_t extra_points = 0) {
  size_t n = Tri.numPoints();
  size_t m = Tri.numTriangles();

  auto V = tabulate(n + extra_points, [&] (size_t i) {
    return (i < n) ? vertex_t(Tri.P[i], i) : vertex_t();});

  sequence<triang_t> Triangs(m + 2 * extra_points);
  sequence<edge> E(m*3);
  EdgeTable ET = makeEdgeTable(m*6);
  parallel_for (0, m, [&] (size_t i) {
    for (int j=0; j<3; j++) {
      E[i*3 + j] = edge(index_pair(Tri.T[i][j], Tri.T[i][(j+1)%3]), &Triangs[i]);
      ET.insert(&E[i*3+j]);
      Triangs[i].vtx[(j+2)%3] = &V[Tri.T[i][j]];
    }});

  parallel_for (0, m, [&] (size_t i) {
    Triangs[i].id = i;
    Triangs[i].initialized = 1;
    Triangs[i].bad = 0;
    for (int j=0; j<3; j++) {
      index_pair key = {Tri.T[i][(j+1)%3], Tri.T[i][j]};
      edge *Ed = ET.find(key);
      if (Ed != NULL) Triangs[i].ngh[j] = Ed->second;
      else {
	Triangs[i].ngh[j] = NULL;
	//Triangs[i].vtx[j]->boundary = 1;
	//Triangs[i].vtx[(j+2)%3]->boundary = 1;
      }
    }
  });
  return std::pair(std::move(Triangs),std::move(V));
}

// Note that this is not currently a complete test of correctness
// For example it would allow a set of disconnected triangles, or even no
// triangles
bool check_delaunay(sequence<triang_t> &Triangles, size_t boundary_size) {
  size_t n = Triangles.size();
  sequence<size_t> boundary_count(n, 0);
  size_t insideOutError = n;
  size_t inCircleError = n;
  parallel_for (0, n, [&] (size_t i) {
    if (Triangles[i].initialized) {
      simplex_t t = simplex(&Triangles[i],0);
      for (int j=0; j < 3; j++) {
	simplex_t a = t.across();
	if (a.valid()) {
	  vertex_t* v = a.rotClockwise().firstVertex();

          // Check that the neighbor is outside the triangle
	  if (!t.outside(v)) {
	    double vz = triAreaNormalized(t.t->vtx[(t.o+2)%3]->pt, 
					  v->pt, t.t->vtx[t.o]->pt);
	    // allow for small error
	    if (vz < -1e-10) pbbs::write_min(&insideOutError, i, less<size_t>());
	  }

          // Check that the neighbor is not in circumcircle of the triangle
	  if (t.inCirc(v)) {
	    double vz = inCircleNormalized(t.t->vtx[0]->pt, t.t->vtx[1]->pt, 
					   t.t->vtx[2]->pt, v->pt);
	    // allow for small error
	    if (vz > 1e-10) pbbs::write_min(&inCircleError, i, less<size_t>());
	  }
	} else boundary_count[i]++;
	t = t.rotClockwise();
      }
    }
  });
  // if (boundary_size != reduce(boundary_count))
  //   cout << "Wrong boundary size: should be " << boundary_size 
  // 	 << " is " << reduce(boundary_count) << endl;

  if (insideOutError < n) {
    cout << "delaunayCheck: neighbor inside triangle at triangle " 
	 << inCircleError << endl;
    return 1;
  }
  if (inCircleError < n) {
    cout << "In Circle Violation at triangle " << inCircleError << endl;
    return 1;
  }

  return 0;
}

// ===== incrementalRefine/refine.C =====

struct Qs {
  vector<vertex<point>*> vertexQ;
  vector<simplex<point>> simplexQ;
  Qs() {
    vertexQ.reserve(50);
    simplexQ.reserve(50);
    // check that actually called
  }
};

using vertexQs = sequence<Qs>;

// *************************************************************
//   PARALLEL HASH TABLE TO STORE WORK QUEUE OF SKINNY TRIANGLES
// *************************************************************

struct hashTriangles {
  typedef triang_t* eType;
  typedef triang_t* kType;
  eType empty() {return NULL;}
  kType getKey(eType v) { return v;}
  size_t hash(kType s) { return hash64(s->id); }
  int cmp(kType s, kType s2) {
    return (s->id > s2->id) ? 1 : ((s->id == s2->id) ? 0 : -1);
  }
  bool cas(eType* p, eType o, eType n) {
    return pbbs::atomic_compare_and_swap(p, o, n);
  }
  bool replaceQ(eType s, eType s2) {return 0;}
};

typedef hashtable<hashTriangles> TriangleTable;
TriangleTable makeTriangleTable(size_t m) {
  return TriangleTable(m,hashTriangles());}

// *************************************************************
//   THESE ARE TAKEN FROM delaunay.C
//   Perhaps should be #included
// *************************************************************

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
void findCavity(simplex_t t, vertex_t *p, Qs *q) {
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
void reserve_for_insert(vertex_t *v, simplex_t t, Qs *q) {
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

// *************************************************************
//   DEALING WITH THE CAVITY
// *************************************************************

inline bool skinnyTriangle(triang_t *t) {
  double minAngle = 30;
  if (minAngleCheck(t->vtx[0]->pt, t->vtx[1]->pt, t->vtx[2]->pt, minAngle))
    return 1;
  return 0;
}

inline bool obtuse(simplex_t t) {
  int o = t.o;
  point p0 = t.t->vtx[(o+1)%3]->pt;
  vect v1 = t.t->vtx[o]->pt - p0;
  vect v2 = t.t->vtx[(o+2)%3]->pt - p0;
  return (v1.dot(v2) < 0.0);
}

inline point circumcenter(simplex_t t) {
  if (t.isTriangle())
    return triangleCircumcenter(t.t->vtx[0]->pt, t.t->vtx[1]->pt, t.t->vtx[2]->pt);
  else { // t.isBoundary()
    point p0 = t.t->vtx[(t.o+2)%3]->pt;
    point p1 = t.t->vtx[t.o]->pt;
    return p0 + (p1-p0)/2.0;
  }
}

// this side affects the simplex_t by moving it into the right orientation
// and setting the boundary if the circumcenter encroaches on a boundary
inline bool checkEncroached(simplex_t& t) {
  if (t.isBoundary()) return 0;
  int i;
  for (i=0; i < 3; i++) {
    if (t.across().isBoundary() && (t.farAngle() > 45.0)) break;
    t = t.rotClockwise();
  }
  if (i < 3) return t.boundary = 1;
  else return 0;
}

bool findAndReserveCavity(vertex_t* v, simplex_t& t, Qs* q) {
  t = simplex_t(v->badT,0);
  if (t.t == NULL) {cout << "refine: nothing in badT" << endl; abort();}
  if (t.t->bad == 0) return 0;

  // if there is an obtuse angle then move across to opposite triangle, repeat
  if (obtuse(t)) t = t.across();
  while (t.isTriangle()) {
    int i;
    for (i=0; i < 2; i++) {
      t = t.rotClockwise();
      if (obtuse(t)) { t = t.across(); break; } 
    }
    if (i==2) break;
  }

  // if encroaching on boundary, move to boundary
  checkEncroached(t);

  // use circumcenter to add (if it is a boundary then its middle)
  v->pt = circumcenter(t);
  reserve_for_insert(v, t, q);
  return 1;
}

// checks if v "won" on all adjacent vertices and inserts point if so
// returns true if "won" and cavity was updated
bool addCavity(vertex_t *v, simplex_t t, Qs *q, TriangleTable& TT) {
  bool flag = 1;
  for (size_t i = 0; i < q->vertexQ.size(); i++) {
    vertex_t* u = (q->vertexQ)[i];
    if (u->reserve == v->id) u->reserve = -1; // reset to -1
    else flag = 0; // someone else with higher priority reserved u
  }
  if (flag) {
    triang_t* t0 = t.t;
    triang_t* t1 = v->t;  // the memory for the two new triang_tangles
    triang_t* t2 = t1 + 1;  
    t1->initialized = 1;
    if (t.isBoundary()) t.splitBoundary(v, t1);
    else {
      t2->initialized = 1;
      t.split(v, t1, t2);
    }

    // update the cavity
    for (size_t i = 0; i<q->simplexQ.size(); i++) 
      (q->simplexQ)[i].flip();
    q->simplexQ.push_back(simplex_t(t0,0));
    q->simplexQ.push_back(simplex_t(t1,0));
    if (!t.isBoundary()) q->simplexQ.push_back(simplex_t(t2,0));

    for (size_t i = 0; i<q->simplexQ.size(); i++) {
      triang_t* t = (q->simplexQ)[i].t;
      if (skinnyTriangle(t)) {
	TT.insert(t); 
	t->bad = 1;}
      else t->bad = 0;
    }
    v->badT = NULL;
  } 
  q->simplexQ.clear();
  q->vertexQ.clear();
  return flag;
}


// *************************************************************
//    MAIN REFINEMENT LOOP
// *************************************************************

// Insert a set of vertices to refine the mesh 
// TT is an initially empty table used to store all the bad
// triangles that are created when inserting vertices
template <typename Slice>
size_t addRefiningVertices(Slice &V, TriangleTable &TT, vertexQs& VQ) {
  size_t n = V.size();
  size_t size = min(VQ.size(), n);
  
  sequence<simplex_t> t(size);
  sequence<bool> flags(size);
  
  size_t top = n;
  size_t num_failed = 0;

  // process all vertices starting just below the top
  while(top > 0) {
    size_t cnt = min<size_t>(size, top);
    size_t offset = top-cnt;

    parallel_for (0, cnt, [&] (size_t j) {
      flags[j] = findAndReserveCavity(V[j+offset], t[j], &VQ[j]);});

    parallel_for (0, cnt, [&] (size_t j) {
      flags[j] = flags[j] && !addCavity(V[j+offset], t[j], &VQ[j], TT);});

    // Pack the failed vertices back onto Q
    auto remain = pack(V.cut(offset,offset+cnt), flags.cut(0,cnt));
    parallel_for (0, remain.size(), [&] (size_t j) {V[j+offset] = remain[j];});
    num_failed += remain.size();
    top = top-cnt+remain.size(); // adjust top, accounting for failed vertices
  }
  return num_failed;
}

// *************************************************************
//    DRIVER
// *************************************************************

#define QSIZE 20000

triangles<point> refineInternal(triangles<point>& Tri) {
  timer t("Delaunay Refine");
  int expandFactor = 100; // bumped from 4 - small inputs need many Steiner points to reach 30deg
  size_t n = Tri.numPoints();
  size_t m = Tri.numTriangles();
  size_t extraVertices = expandFactor*n;
  size_t totalVertices = n + extraVertices;
  size_t totalTriangles = m + 2 * extraVertices;

  sequence<vertex_t> Vertices;
  sequence<triang_t> Triangles;
  sequence<vertex_t*> V(extraVertices);
  
  std::tie(Triangles, Vertices) = topology_from_triangles(Tri, extraVertices);
  t.next("from Triangles");
  
  //  set up extra triangles
  parallel_for (m, totalTriangles, [&] (size_t i) {
    Triangles[i].id = i;
    Triangles[i].initialized = 0;
  });

  //  set up extra vertices
  parallel_for (0, extraVertices, [&] (size_t i) {
    V[i] = new (&Vertices[i+n]) vertex_t(point(0,0), i+n);
    // give each one a pointer to two triangles to use
    V[i]->t = &Triangles[m + 2*i];
  });
  t.next("initializing");

  // these will increase as more are added
  size_t numTriangs = m;
  size_t numPoints = n;

  TriangleTable workQ = makeTriangleTable(numTriangs);
  parallel_for(0, numTriangs, [&] (size_t i) {
    if (skinnyTriangle(&Triangles[i])) {
      workQ.insert(&Triangles[i]);
      Triangles[i].bad = 1;
    }
  });

  vertexQs VQ(QSIZE);
  t.next("Start");

  // Each iteration processes all bad triangles from the workQ while
  // adding new bad triangles to a new queue
  while (1) {
    sequence<triang_t*> badTT = workQ.entries();

    // packs out triangles that are no longer bad
    auto flags = tabulate(badTT.size(), [&] (size_t i) -> bool {
      return badTT[i]->bad;});
    auto badT = pack(badTT, flags);
    size_t numBad = badT.size();

    if(false) cout << "numBad = " << numBad << endl;
    if (numBad == 0) break;
    if (numPoints + numBad > totalVertices) {
      if(false) cout << "ran out of vertices" << endl;
      abort();
    }
    size_t offset = numPoints - n;

    // allocate 1 vertex per bad triangle and assign triangle to it
    parallel_for (0, numBad, [&] (size_t i) {
      badT[i]->bad = 2; // used to detect whether touched
      V[i + offset]->badT = badT[i];
    });

    // the new empty work queue
    workQ = makeTriangleTable(numBad);

    // This does all the work adding new vertices, and any new bad
    // triangles to the workQ
    auto Vtx = V.cut(offset, offset+numBad);
    addRefiningVertices(Vtx, workQ, VQ);

    // push any bad triangles that were left untouched onto the Q
    parallel_for (0, numBad, [&] (size_t i) {
      if (badT[i]->bad==2) workQ.insert(badT[i]);});

    numPoints += numBad;
    numTriangs += 2*numBad;
  }

  t.next("refinement");
  if(false) std::cout << numTriangs << " : " << Vertices.size() << " : " << numPoints << std::endl;
  
  // Extract Vertices for result
  auto flag = tabulate(numPoints, [&] (size_t i) -> bool {
    return (Vertices[i].badT == NULL);});

  if(false) std::cout << "here" << std::endl;
  sequence<size_t> I = pack_index(flag);
  size_t n0 = I.size();
  sequence<point> rp(n0);

  if(false) std::cout << "here2" << std::endl;
  parallel_for (0, n0, [&] (size_t i) {
    Vertices[I[i]].id = i;
    rp[i] = Vertices[I[i]].pt;
  });
  if(false) cout << "total points = " << n0 << endl;

  // Extract Triangles for result
  I = pack_index(tabulate(numTriangs, [&] (size_t i) -> bool {
	 return Triangles[i].initialized;}));
							  
  auto rt = tabulate(I.size(), [&] (size_t i) -> tri {
    auto t = Triangles[I[i]];
    tri r = {t.vtx[0]->id, t.vtx[1]->id, t.vtx[2]->id};
    return r;});

  if(false) cout << "total triangles = " << I.size() << endl;
  t.next("finish");
  return triangles<point>(std::move(rp), std::move(rt));
}

triangles<point> refine(triangles<point> &Tri) {
  return refineInternal(Tri);
}

} // namespace pbbs_inc_refine
