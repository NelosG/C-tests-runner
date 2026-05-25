// Vendored from pbbsbench/benchmarks/nBody/parallelCK + common/geometry.h
//   + bench/spherical.h (parallelCK uses bench/nbody.h via symlink).
// (MIT licensed, (c) Guy Blelloch and the PBBS team.)
// Concatenated into a single TU and wrapped in namespace so parlay
// scheduler thread_local ODR is happy.
//
// We drop the original `nbody()` and `check()`/CHECK noise from nbody.C
// and expose `pbbs_ck_nbody::stepBH(sequence<particle*>&)` directly.
#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <iomanip>
#include <iostream>
#include <utility>
#include <vector>
#include <parlay/alloc.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>

namespace pbbs_ck_nbody {

using namespace std;
using parlay::sequence;
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

// ===== bench/spherical.h =====
// The following code was addapted from the PetFMM code
// to be used as part of the Problem Based Benchmark Suite (PBBS)
// Both codes live under he Gnu general public license
//
// Copyright (c) 2010 PetFMM developer team.
// Copyright (c) 2011 Guy Blelloch and the PBBS team
// 
// Permission to copy and modify this software and its documentation is hereby
// granted, provided that this notice is retained thereon and on all copies or
// modifications. Permission is hereby granted to use, reproduce, prepare
// derivative works, and to redistribute to others, so long as this original
// copyright notice is retained.
// 
// DISCLAIMER
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#ifndef included_Transform_SphericalHarmonic_hh
#define included_Transform_SphericalHarmonic_hh


using coord=double;

  template<int terms, int coefficients = terms*terms>
  struct Transform {
  public:
    typedef point3d<coord> point_type;
    typedef vector3d<coord> vect_type;
    typedef std::complex<double>  coeff_type;
    typedef double     real_type;
    const static int numTerms        = terms;
    const static int numCoefficients = coefficients;
    const static int precompSize     = 2*numTerms*2*numTerms;
    const real_type            eps;
    const std::complex<double> I;
  protected:
    real_type   prefactor[precompSize];  // \sqrt{\frac{(n - |m|)!}{(n + |m|)!}}
    real_type   Anm[precompSize];        // A^n_m = \frac{-1^n}{(n - m)! (n + m)!}
    real_type   AnmI[precompSize];       // 1/A^n_m 
  public:

  Transform() : eps(1e-20), I(0.0, 1.0) {};

  ~Transform() {};

  protected:
    real_type factorial(real_type n) const {
      //assert(n >= 0);
      if (n <= 1) {
        return(1);
      } else {
        n *= factorial(n-1);
        return(n);
      }
    };

    coeff_type complexMult(coeff_type c1, coeff_type c2) {
      double a = c1.real();
      double b = c1.imag();
      double c = c2.real();
      double d = c2.imag();
      return coeff_type(a*c - b*d, a*d + b*c);
    }

    real_type complexMultReal(coeff_type c1, coeff_type c2) {
      return c1.real()*c2.real() - c1.imag()*c2.imag();
    }

    real_type complexMultImag(coeff_type c1, coeff_type c2) {
      return c1.real()*c2.imag() + c1.imag()*c2.real();
    }

  public:
    void precompute() {
      //   Calculate fac^n_m = \sqrt{\frac{(n - |m|)!}{(n + |m|)!}}
      //   Calculate A^n_m = \frac{-1^n}{(n - m)! (n + m)!}
      for(int n = 0, nm = 0; n < 2*numTerms; ++n) {
        for(int m = -n; m <= n; ++m, ++nm){
          //nm = (n*n) + (n+m);
          prefactor[nm] = sqrt(factorial(n - abs(m)) / factorial(n + abs(m)));
          Anm[nm]       = pow(-1.0, n)/sqrt(factorial(n - m) * factorial(n + m));
          AnmI[nm]      = 1/Anm[nm];
        }
      }
    };

    void evaluateMultipole(coeff_type array[], real_type r,real_type cosTheta,coeff_type eiphi) {
      const real_type s2 = sqrt((1-cosTheta)*(1+cosTheta));
      real_type       pn = 1.0;
      real_type powers[numTerms];
      powers[0] = 1.0;
      for(int i=1; i<numTerms; i++) powers[i] = powers[i-1]*r;
      coeff_type eim = coeff_type(1.0,0.0);

      for(int m = 0, fact = 1; m < numTerms; ++m, fact += 2) {
        real_type  p   = pn;
        const int  npn = m*m + 2*m;
        const int  nmn = m*m;
        coeff_type Ynm = prefactor[npn]*p*eim;
        real_type  p1  = p;

        array[npn] = powers[m]*Ynm;
        array[nmn] = powers[m]*std::conj(Ynm);
        p = cosTheta*(2*m+1)*p;
        for(int n = m+1; n < numTerms; ++n) {
          const int       npm = n*n+n+m;
          const int       nmm = n*n+n-m;
          const real_type p2  = p1;

          Ynm = prefactor[npm]*p*eim;
          array[npm] = powers[n]*Ynm;
          array[nmm] = powers[n]*std::conj(Ynm);
          p1 = p;
          p  = (cosTheta*(2*n+1)*p1 - (n+m)*p2)/(n-m+1);
        }
        pn = -pn*fact*s2;
        eim = eim * eiphi;
      }
    };

    void evaluateLocal(coeff_type array[], real_type r, real_type cosTheta, coeff_type eiphi) {
      const real_type s2 = sqrt((1-cosTheta)*(1+cosTheta));
      real_type       pn = 1.0;
      real_type powers[2*numTerms+1];
      powers[0] = 1.0;
      real_type ri = 1.0/r;
      for(int i=1; i<2*numTerms+1; i++) powers[i] = powers[i-1]*ri;
      coeff_type eim = coeff_type(1.0,0.0);

      for(int m = 0, fact = 1; m < 2*numTerms; ++m, fact += 2) {
        real_type  p   = pn;
        const int  npn = m*m + 2*m;
        const int  nmn = m*m;
        coeff_type Ynm = prefactor[npn]*p*eim;
        real_type  p1  = p;

	array[npn] = Ynm*powers[m+1];
        array[nmn] = std::conj(array[npn]);
        p = cosTheta*(2*m+1)*p;
        for(int n = m+1; n < 2*numTerms; ++n) {
          const int       npm = n*n+n+m;
          const int       nmm = n*n+n-m;
          const real_type p2  = p1;

          Ynm = prefactor[npm]*p*eim;
          array[npm] = Ynm*powers[n+1];
          array[nmm] = std::conj(array[npm]); 
          p1 = p;
          p  = (cosTheta*(2*n+1)*p1 - (n+m)*p2)/(n-m+1);
        }
        pn = -pn*fact*s2;
        eim = eim * eiphi;
      }
    };

    void evaluateMultipoleTheta(coeff_type multipole[], coeff_type multipoleTheta[], 
				real_type r, real_type cosTheta, real_type sinTheta, 
				coeff_type eiPhi) {
      const real_type s2 = sqrt((1-cosTheta)*(1+cosTheta));
      real_type       pn = 1.0;
      coeff_type dummy;
      coeff_type eim = coeff_type(1.0,0.0);

      for(int m = 0, fact = 1; m < numTerms; ++m, fact += 2) {
        real_type  p   = pn;
        const int  npn = m*m + 2*m;
        const int  nmn = m*m;
        coeff_type Ynm = prefactor[npn]*p*eim;
        real_type  p1  = p;

        multipole[npn] = Ynm;
        multipole[nmn] = std::conj(Ynm);
        p = cosTheta*(2*m+1)*p;

        coeff_type Yth = prefactor[npn]*(p-(m+1)*cosTheta*p1)/sinTheta*eim;

        multipoleTheta[npn] = Yth;
        multipoleTheta[nmn] = std::conj(Yth);
        for(int n = m+1; n < numTerms; ++n) {
          const int       npm = n*n+n+m;
          const int       nmm = n*n+n-m;
          const real_type p2  = p1;

          Ynm = prefactor[npm]*p*eim;
          multipole[npm] = Ynm;
          multipole[nmm] = std::conj(Ynm);
          p1 = p;
          p  = (cosTheta*(2*n+1)*p1 - (n+m)*p2)/(n-m+1);
          Yth = prefactor[npm]*((n-m+1)*p - (n+1)*cosTheta*p1)/sinTheta*eim;
          multipoleTheta[npm] = Yth;
          multipoleTheta[nmm] = std::conj(Yth);
        }
        pn = -pn*fact*s2;
        eim = eim * eiPhi;
      }
    };
  public:

  inline int powNeg1(int i) {
    return (i & 1) ? -1 : 1;
  }

    void M2Madd(coeff_type array[],  point_type newCenter,coeff_type coeff[],point_type center) {
      vect_type diff = newCenter - center;
      real_type r = diff.Length();
      real_type cosTheta = diff.z/r;
      real_type rxy = sqrt(diff.x*diff.x + diff.y*diff.y);
      coeff_type eiphi = (rxy==0) ? coeff_type(1,0) : coeff_type(diff.x/rxy, diff.y/rxy);

      coeff_type multipole[numCoefficients];
      evaluateMultipole(multipole, r, cosTheta, std::conj(eiphi));

      for(int j = 0; j < numTerms; ++j) {
        for(int k = 0; k <= j; ++k) {
          const int  jk  = j*j + j+k;
          const int  jks = j*(j+1)/2+k;
          coeff_type bx  = 0.0;

          for(int n = 0; n <= j; ++n) {
            for(int m = -n; m <= min(k-1,n); ++m) {
              if (j-n >= k-m) {
                int        jnkm  = (j-n)*(j-n) + j-n+k-m;
                int        jnkms = (j-n)*(j-n+1)/2 + k-m;
                int        nm    = n*n + n+m;
                coeff_type cnm = (powNeg1((m-abs(m))/2)*powNeg1(n)*
				  Anm[nm]*Anm[jnkm]*AnmI[jk])*multipole[nm];
                bx += complexMult(coeff[jnkms], cnm);
              }
            }
            for(int m = k; m <= n; ++m) {
              if (j-n >= m-k) {
                int        jnkm  = (j-n)*(j-n) + j-n+k-m;
		int        jnkms = (j-n)*(j-n+1)/2 - k+m;
		int        nm    = n*n + n+m;
		coeff_type cnm   = (powNeg1(k+n+m)*Anm[nm]*Anm[jnkm]*AnmI[jk])*
		                   multipole[nm];
                bx += complexMult(std::conj(coeff[jnkms]), cnm);
              }
            }
          }
          array[jks] += bx;
        }
      }
    };


    void M2Ladd(coeff_type array[], point_type newCenter, coeff_type coeff[],point_type center) {
      vect_type diff = newCenter - center;
      real_type r = diff.Length();
      real_type cosTheta = diff.z/r;
      real_type rxy = sqrt(diff.x*diff.x + diff.y*diff.y);
      coeff_type eiphi = (rxy==0) ? coeff_type(1,0) : coeff_type(diff.x/rxy, diff.y/rxy);

      // to save a little time with little loss
      int numSourceTerms = numTerms - 1 ; 
      coeff_type co[coefficients];
      coeff_type local[precompSize];
      real_type zz[2*numTerms-1];
      
      //coeff_type *co = new coeff_type[coefficients];
      //coeff_type *local = new coeff_type[precompSize];
      //real_type *zz = new real_type[2*numTerms-1];
      if (r < 0) cout << "this is a fix for broken cilkplus" << endl;

      // copy into full form [-m,m]
      for (int n = 0; n < numSourceTerms; ++n) {
	int nns = n*(n+1);
	for(int m = -n; m < 0; ++m) 
	  co[nns + m] = std::conj(coeff[nns/2 - m]);
	for(int m = 0; m <= n; ++m) 
	  co[nns + m] = coeff[nns/2 + m];
      }

      evaluateLocal(local, r, cosTheta, eiphi);
      for(int j = 0; j < numTerms; ++j) {
        for(int k = 0; k <= j; k++) {
          const int  jk  = j*j + j+k;
          const int  jks = j*(j+1)/2 + k;
          coeff_type ax  = 0.0;
	  for (int m = -(numSourceTerms-1); m < numSourceTerms; m++) {
	    // i^{|k-m|-|k|-|m|}
	    int ip = (1-2*((-abs(k-m) + abs(k) + abs(m))/2 & 1));  
	    // i^{|k-m|-|k|-|m|} * A^k_j * -1^{j}
	    // should it be? : i^{|k-m|-|k|-|m|} * A^k_j * -1^{j+k}
	    zz[m+numSourceTerms-1] = ip * pow(-1.0, j) * Anm[jk];
	  }

          for(int n = 0; n < numSourceTerms; ++n) {
	    int nns = n*(n+1);
	    int jn = j+n;
	    int jns = jn*(jn+1)-k;
	    coeff_type *plocal = local + jns - n;
	    real_type *pAnmI = AnmI + jns - n;
	    real_type *pAnm = Anm + nns -n;
            coeff_type *pco = co + nns - n;
	    real_type *pzz = zz + numSourceTerms -1 - n;
            for(int m = -n; m <= n; ++m) {
              //int nm   = nns + m;
              //int jnkm = jns + m;
	      //real_type srr = zz[m + numSourceTerms -1]*Anm[nm]*AnmI[jnkm];
	      //ax += co[nm]*srr*local[jnkm]; 
              // optimized to use pointer arithmetic...ideally compiler would do it
	      real_type srr = (*pzz++)*(*pAnm++)*(*pAnmI++); 
	      ax += complexMult((*pco++) , srr*(*plocal++));
            }
          }
          array[jks] += ax;
        }
      }
      //delete local; delete co; delete zz;
    };



    void L2Ladd(coeff_type array[],  point_type newCenter,  coeff_type coeff[],  point_type center) {
      vect_type diff = newCenter - center;
      real_type r = diff.Length();
      real_type cosTheta = diff.z/r;
      real_type rxy = sqrt(diff.x*diff.x + diff.y*diff.y);
      coeff_type eiphi = (rxy==0) ? coeff_type(1,0) : coeff_type(diff.x/rxy, diff.y/rxy);

      coeff_type multipole[numCoefficients];
      evaluateMultipole(multipole, r, cosTheta, eiphi);
      for(int j = 0; j < numTerms; ++j) {
        for(int k = 0; k <= j; ++k) {
          const int  jk  = j*j + j+k;
          const int  jks = j*(j+1)/2 + k;
          coeff_type ax  = 0.0;

          for(int n = j; n < numTerms; ++n) {
            for(int m = j+k-n; m < 0; ++m) {
              int        jnkm = (n-j)*(n-j) + n-j+m-k;
              int        nm   = n*n + n-m;
              int        nms  = n*(n+1)/2 - m;
              coeff_type cnm  = (powNeg1(k)*Anm[jnkm]*Anm[jk]*AnmI[nm])*
		                multipole[jnkm];
              ax += complexMult(std::conj(coeff[nms]), cnm);
            }
            for(int m = 0; m <= n; ++m) {
              if (n-j >= abs(m-k)) {
                int        jnkm = (n-j)*(n-j) + n-j+m-k;
                int        nm   = n*n + n+m;
                int        nms  = n*(n+1)/2 + m;
                coeff_type cnm  = (powNeg1((m-k - abs(m-k))/2)*Anm[jnkm]*
				   Anm[jk]*AnmI[nm])*multipole[jnkm];
                ax += complexMult(coeff[nms], cnm);
              }
            }
          }
          array[jks] += ax;
        }
      }
    };

    void clearM(coeff_type array[]) {
      for (int i=0; i < numTerms*(numTerms+1)/2; i++) array[i] = coeff_type(0.0,0.0);
    }

    void P2Madd(coeff_type array[], real_type gamma, point_type center, point_type x) {
      vect_type diff = x - center;
      real_type r = diff.Length();
      real_type cosTheta = (r==0) ? 1.0 : diff.z/r;
      real_type rxy = sqrt(diff.x*diff.x + diff.y*diff.y);
      coeff_type eiphi = (rxy==0) ? coeff_type(1,0) : coeff_type(diff.x/rxy, diff.y/rxy);

      coeff_type multipole[numCoefficients];
      evaluateMultipole(multipole, r, cosTheta, std::conj(eiphi));

      for(int n = 0, nms = 0; n < numTerms; ++n) {
        for(int m = 0; m <= n; ++m, ++nms) {
          int nm = n*n + n+m;
          array[nms] += gamma*multipole[nm];
        }
      }
    };

    void M2P(real_type& potential, vect_type& field, point_type x, coeff_type coeff[], 
	     point_type center) {
      vect_type diff  = x - center;
      real_type r = diff.Length();
      real_type rxy = sqrt(diff.x*diff.x + diff.y*diff.y);
      real_type cosTheta = (r==0) ? 1.0 : diff.z/r;
      real_type sinTheta = (r==0) ? 0.0 : rxy/r;
      coeff_type eiphi = (rxy==0) ? coeff_type(1,0) : coeff_type(diff.x/rxy, diff.y/rxy);

      real_type        gx = 0.0, gxr = 0.0, gxth = 0.0, gxph = 0.0;
      real_type powers[numTerms+1];
      powers[0] = 1.0;
      real_type ri = 1.0/r;
      for(int i=1; i<numTerms+1; i++) powers[i] = powers[i-1]*ri;
      coeff_type multipole[numCoefficients];
      coeff_type multipoleTheta[numCoefficients];

      evaluateMultipoleTheta(multipole, multipoleTheta, r, cosTheta, sinTheta, eiphi);
      for(int n = 0; n < numTerms; ++n) {
        const int nm  = n*n+n;
        const int nms = n*(n+1)/2;

	real_type xx = .5*powers[n+1]*(multipole[nm]*coeff[nms]).real();
	gx  += xx;
        gxr  += -(n+1)*ri*xx;
        gxth += .5*(powers[n+1]*multipoleTheta[nm]*coeff[nms]).real();
        for(int m = 1; m <= n; ++m) {
          const int nm  = n*n + n+m;
          const int nms = n*(n+1)/2 + m;
	  coeff_type xx = (powers[n+1]*complexMult(multipole[nm],coeff[nms]));
          gx  += xx.real();
          gxr  += -(n+1)*ri*xx.real();
          gxth += powers[n+1]*complexMult(multipoleTheta[nm],coeff[nms]).real();
          gxph += -m*xx.imag();
        }
      }
      real_type cosPhi = eiphi.real();
      real_type sinPhi = eiphi.imag();
      gx *= 2; gxr *= 2; gxth *= 2; gxph *= 2;
      const real_type gxx = sinTheta*cosPhi*gxr +cosTheta*cosPhi/r*gxth - sinPhi/r/sinTheta*gxph;
      const real_type gxy = sinTheta*sinPhi*gxr +cosTheta*sinPhi/r*gxth + cosPhi/r/sinTheta*gxph;
      const real_type gxz = cosTheta*gxr - sinTheta/r*gxth;
      potential = gx; //1.0/(4*M_PI) * gx;
      field.x  = gxx; //1.0/(4*M_PI) * gxx;
      field.y  = gxy; //1.0/(4*M_PI) * gxy;
      field.z  = gxz; //1.0/(4*M_PI) * gxz;
    };

    // gamma currently not used
    void L2P(real_type& potential, vector3d<coord>& field, point_type x, coeff_type coeff[], 
	     point_type center) {
      vect_type diff  = x - center;
      real_type r = diff.Length();
      real_type rxy = sqrt(diff.x*diff.x + diff.y*diff.y);
      real_type cosTheta = (r==0) ? 1.0 : diff.z/r;
      real_type sinTheta = (r==0) ? 0.0 : rxy/r;
      coeff_type eiphi = (rxy==0) ? coeff_type(1,0) : coeff_type(diff.x/rxy, diff.y/rxy);

      real_type gx = 0.0, gxr = 0.0, gxth = 0.0, gxph = 0.0;
      coeff_type multipole[numCoefficients];
      coeff_type multipoleTheta[numCoefficients];

      evaluateMultipoleTheta(multipole, multipoleTheta, r, cosTheta, sinTheta, eiphi);
      for(int n = 0; n < numTerms; ++n) {
        const int nm  = n*n+n;
        const int nms = n*(n+1)/2;

        gx  += (pow(r,n)*multipole[nm]*coeff[nms]).real();
        gxr  += (n*pow(r,n-1)*multipole[nm]*coeff[nms]).real();
        gxth += (pow(r,n)*multipoleTheta[nm]*coeff[nms]).real();
        for(int m = 1; m <= n; ++m) {
          int nm  = n*n + n+m;
          int nms = n*(n+1)/2 + m;
	  real_type MCR = complexMult(multipole[nm], coeff[nms]).real();
	  real_type MCI = complexMult(multipole[nm],coeff[nms]).imag();
	  real_type MTCR = complexMult(multipoleTheta[nm],coeff[nms]).real();

          gx  += 2.0*(pow(r,n)* MCR);
          gxr  += 2.0*(n*pow(r,n-1)*MCR);
          gxth += 2.0*(pow(r,n)*MTCR);
          //gxph += 2*(I*(m*pow(r,n)*multipole[nm]*coeff[nms])).real();
	  gxph += -2.0*(m*pow(r,n)*MCI);
        }
      }
      real_type cosPhi = eiphi.real();
      real_type sinPhi = eiphi.imag();
      const real_type gxx = sinTheta*cosPhi*gxr+ cosTheta*cosPhi/r*gxth - sinPhi/r/sinTheta*gxph;
      const real_type gxy = sinTheta*sinPhi*gxr+ cosTheta*sinPhi/r*gxth + cosPhi/r/sinTheta*gxph;
      const real_type gxz = cosTheta*gxr - sinTheta/r*gxth;
      potential += gx; //1.0/(4*M_PI) * gx;
      field.x  += gxx; //1.0/(4*M_PI) * gxx;
      field.y  += gxy; //1.0/(4*M_PI) * gxy;
      field.z  += gxz; //1.0/(4*M_PI) * gxz;
    };

  };
#endif // included_Transform_SphericalHarmonic_hh

// ===== bench/nbody.h (particle type) =====
using coord = double;
using point = point3d<coord>;
using vect = vector3d<coord>;

class particle {
public:
    point pt;
    vect force;
    double mass;
    particle(point p, double m) : pt(p), mass(m) {}
    particle() {}
};

// ===== parallelCK/nbody.C =====
using vect3d = vect;

#define CHECK 0

// Following for 1e-3 accuracy
//#define ALPHA 2.2
//#define terms 7
//#define BOXSIZE 150

// Following for 1e-6 accuracy (2.5x slower than above)
#define ALPHA 2.6
#define terms 12  
#define BOXSIZE 250

// Following for 1e-9 accuracy (2.2x slower than above)
// #define ALPHA 3.0
// #define terms 17
// #define BOXSIZE 550

// Following for 1e-12 accuracy (1.8x slower than above)
//#define ALPHA 3.2
//#define terms 22
//#define BOXSIZE 700

double check(sequence<particle*> const &p) {
  size_t n = p.size();
  size_t nCheck = min<size_t>(n, 200);
  sequence<double> Err(nCheck);
  
  parlay::parallel_for (0, nCheck, [&] (size_t i) {
    size_t idx = parlay::hash64(i)%n;
    vect3d force(0.,0.,0.);
    for (size_t j=0; j < n; j++) {
      if (idx != j) {
	vect3d v = (p[j]->pt) - (p[idx]->pt);
	double r2 = v.dot(v);
	force = force + (v * (p[j]->mass * p[idx]->mass / (r2*sqrt(r2))));
      }
    }
    Err[i] = (force - p[idx]->force).Length()/force.Length();
    });
  double total = 0.0;
  for(int i=0; i < nCheck; i++) 
    total += Err[i];
  return total/nCheck;
}

// *************************************************************
//    FORCE CALCULATIONS
// *************************************************************

// *************************************************************
//  Inner expansions (also called multipole expansion)
//  The spherical harmonic expansion of a set of nearby points around
//  a center for estimating forces at a distance.
// *************************************************************
struct innerExpansion {
  Transform<terms>* TR;
  complex<double> coefficients[terms*terms];
  point center;
  void addTo(point pt, double mass) {
    TR->P2Madd(coefficients, mass, center, pt);
  }
  void addTo(innerExpansion* y) {
    TR->M2Madd(coefficients, center, y->coefficients, y->center);
  }
  innerExpansion(Transform<terms>* _TR, point _center) : TR(_TR), center(_center) {
    for (size_t i=0; i < terms*terms; i++) coefficients[i] = 0.0;
  }
  vect3d force(point y, double mass) {
    vect3d result;
    double potential;
    TR->M2P(potential, result, y, coefficients, center);
    result = result*mass;
    return result;
  }
  innerExpansion() {}
};

parlay::type_allocator<innerExpansion> inner_pool;

// *************************************************************
//  Outer expansions (also called local)
//  The inverse spherical harmonic expansion of a set of distant
//  points around a center for estimating forces for nearby points.
// *************************************************************
struct outerExpansion {
  Transform<terms>* TR;
  complex<double> coefficients[terms*terms];
  point center;
  void addTo(innerExpansion* y) {
    TR->M2Ladd(coefficients, center, y->coefficients, y->center);}
  void addTo(outerExpansion* y) {
    TR->L2Ladd(coefficients, center, y->coefficients, y->center);
  }
  vect3d force(point y, double mass) {
    vect3d result;
    double potential;
    TR->L2P(potential, result, y, coefficients, center);
    result = result*mass;
    return result;
  }
  outerExpansion(Transform<terms>* _TR, point _center) : TR(_TR), center(_center) {
    for (size_t i=0; i < terms*terms; i++) coefficients[i] = 0.0;
  }
  outerExpansion() {}
};

parlay::type_allocator<outerExpansion> outer_pool;

// Set global constants for spherical harmonics
Transform<terms>* TRglobal = new Transform<terms>();

using box = pair<point,point>;
using vect3d = typename point::vector;

// *************************************************************
//  A node in the CK tree
//  Either a leaf (if children are null) or internal node.
//  If a leaf contains a set of points
//  If an internal node contains a left and right child as is
//  augmented with first inner than outer expansions.
//  The leftNeighbors and rightNeighbors contain edges in the CK
//  well separated decomposition.
// *************************************************************
struct node {
  using edge = pair<node*, size_t>;
  node* left;
  node* right;
  sequence<particle*> particles;
  sequence<particle> particles_d;
  size_t n;
  box b;
  innerExpansion* InExp;
  outerExpansion* OutExp;
  vector<node*> indirectNeighbors;
  vector<edge> leftNeighbors;
  vector<edge> rightNeighbors;
  sequence<sequence<vect3d>> hold;
  bool leaf() {return left == NULL;}
  node() {}
  point center() { return b.first + (b.second-b.first)/2.0;}
  double radius() { return (b.second - b.first).Length()/2.0;}
  double lmax() {
    vect3d d = b.second-b.first;
    return max(d.x,max(d.y,d.z));
  }
  void allocateExpansions() {
    InExp = inner_pool.allocate(TRglobal, center());
    OutExp = outer_pool.allocate(TRglobal, center());
  }
  node(node* L, node* R, size_t n, box b)
    : left(L), right(R), n(n), b(b) {
    allocateExpansions();
  }
  node(parlay::sequence<particle*> P, box b) 
    : left(NULL), right(NULL), particles(std::move(P)), b(b) {
    n = particles.size();
    particles_d = parlay::map(particles, [] (auto p) {return *p;});
    allocateExpansions();
  }
};

size_t numLeaves(node* tr) {
  if (tr->leaf()) return 1;
  else return(numLeaves(tr->left)+numLeaves(tr->right));
}

parlay::type_allocator<node> node_pool;

using edge = pair<node*, size_t>;

// *************************************************************
//  Build the CK tree
//  Similar to a kd-tree but always split along widest dimension
//  of the points instead of the next round-robin dimension.
// *************************************************************
template <typename Particles>
node* buildTree(Particles& particles, size_t effective_size) {
  
  size_t n = particles.size();
  size_t en = std::max(effective_size, n);

  auto minmax = [] (box a, box b) {
    return box((a.first).minCoords(b.first),
	       (a.second).maxCoords(b.second));};
  auto pairs = parlay::delayed_map(particles, [&] (particle* p) {
      return box(p->pt, p->pt);});
  box b = parlay::reduce(pairs, parlay::make_monoid(minmax,pairs[0]));
										      
  if (en < BOXSIZE || n < 10) 
    return node_pool.allocate(parlay::to_sequence(particles), b);

  size_t d = 0;
  double Delta = 0.0;
  for (int i=0; i < 3; i++) {
    if (b.second[i] - b.first[i] > Delta) {
      d = i;
      Delta = b.second[i] - b.first[i];
    }
  }
  
  double splitpoint = (b.first[d] + b.second[d])/2.0;

  auto isLeft = parlay::delayed_map(particles, [&] (particle* p) {
      return std::pair(p->pt[d] < splitpoint, p);});
  auto foo = parlay::group_by_index(isLeft, 2);
  particles.clear();

  auto r = parlay::map(foo, [&] (auto& x) {
      return buildTree(x, .4 * en);}, 1);
  return node_pool.allocate(r[0], r[1], n, b);
}


// *************************************************************
//  Determine if a point is far enough to use approximation.
// *************************************************************
bool far(node* a, node* b) {
  double rmax = max(a->radius(), b->radius());
  double r = (a->center() - b->center()).Length();
  return r >= (ALPHA * rmax);
}

// *************************************************************
// Used to count the number of interactions, just for performance
// statistics not needed for correctness.
// *************************************************************
struct interactions_count {
  long direct;
  long indirect;
  interactions_count() {}
  interactions_count(long a, long b) : direct(a), indirect(b) {}
  interactions_count operator+ (interactions_count b) {
    return interactions_count(direct + b.direct, indirect + b.indirect);}
};

// *************************************************************
// The following two functions are the core of the CK method.
// They calculate the "well separated decomposition" of the points.
// *************************************************************
interactions_count interactions(node* Left, node* Right) {
  if (far(Left,Right)) {
    Left->indirectNeighbors.push_back(Right); 
    Right->indirectNeighbors.push_back(Left); 
    return interactions_count(0,2);
  } else {
    if (!Left->leaf() && (Left->lmax() >= Right->lmax() || Right->leaf())) {
      interactions_count x = interactions(Left->left, Right);
      interactions_count y = interactions(Left->right, Right);
      return x + y;
    } else if (!Right->leaf()) {
      interactions_count x = interactions(Left, Right->left);
      interactions_count y = interactions(Left, Right->right);
      return x + y;
    } else { // both are leaves
      if (Right->n > Left->n) swap(Right,Left);
      size_t rn = Right->leftNeighbors.size();
      size_t ln = Left->rightNeighbors.size();
      Right->leftNeighbors.push_back(edge(Left,ln)); 
      Left->rightNeighbors.push_back(edge(Right,rn));
      return interactions_count(Right->n*Left->n,0);
    }
  }
}

// Could be parallelized but would require avoiding push_back.
// Currently not a bottleneck so left serial.
interactions_count interactions(node* tr) {
  if (!tr->leaf()) {
    interactions_count x, y, z; 
    x = interactions(tr->left);
    y = interactions(tr->right);
    z = interactions(tr->left,tr->right);
    return x + y + z;
  } else return interactions_count(0,0);
}

// *************************************************************
// Translate from inner (multipole) expansion to outer (local)
// expansion along all far-field interactions.
// *************************************************************
void doIndirect(node* tr) {
  for (size_t i = 0; i < tr->indirectNeighbors.size(); i++) 
    tr->OutExp->addTo(tr->indirectNeighbors[i]->InExp);
  if (!tr->leaf()) {
    parlay::par_do([&] () {doIndirect(tr->left);},
		   [&] () {doIndirect(tr->right);});
  }
}

// *************************************************************
// Translate and accumulate inner (multipole) expansions up the tree,
// including translating particles to expansions at the leaves.
// *************************************************************
void upSweep(node* tr) {
  if (tr->leaf()) {
    for (size_t i=0; i < tr->n; i++) {
      particle* P = tr->particles[i];
      tr->InExp->addTo(P->pt, P->mass);
    }
  } else {
    parlay::par_do([&] () {upSweep(tr->left);},
		   [&] () {upSweep(tr->right);});
    tr->InExp->addTo(tr->left->InExp);
    tr->InExp->addTo(tr->right->InExp);
  }
}

// *************************************************************
// Translate and accumulate outer (local) expansions down the tree,
// including applying them to all particles at the leaves.
// *************************************************************
void downSweep(node* tr) {
  if (tr->leaf()) {
    for (size_t i=0; i < tr->n; i++) {
      particle* P = tr->particles[i];
      P->force = P->force + tr->OutExp->force(P->pt, P->mass);
    }
  } else {
    parlay::par_do([&] () {tr->left->OutExp->addTo(tr->OutExp);
	                   downSweep(tr->left);},
		   [&] () {tr->right->OutExp->addTo(tr->OutExp);
		           downSweep(tr->right);});
  }
}

// puts the leaves of tree tr into the array Leaves in left to right order
size_t getLeaves(node* tr, node** Leaves) {
  if (tr->leaf()) {
    Leaves[0] = tr;
    return 1;
  } else {
    size_t l = getLeaves(tr->left, Leaves);
    size_t r = getLeaves(tr->right, Leaves + l);
    return l + r;
  }
}

// *************************************************************
// Calculates the direct forces between all pairs of particles in two nodes.
// Directly updates forces in Left, and places forces for ngh in hold
// This avoid a race condition on modifying ngh while someone else is
// updating it.
// *************************************************************
auto direct(node* Left, node* ngh) {
  auto LP = (Left->particles).data();
  auto R = (ngh->particles_d).data();
  size_t nl = Left->n;
  size_t nr = ngh->n;
  parlay::sequence<vect3d> hold(nr, vect3d(0.,0.,0.));
  for (size_t i=0; i < nl; i++) {
    vect3d frc(0.,0.,0.);
    particle pa = *LP[i];
    for (size_t j=0; j < nr; j++) {
      particle& pb = R[j];
      vect3d v = pb.pt - pa.pt;
      double r2 = v.dot(v);
      vect3d force = (v * (pa.mass * pb.mass / (r2*sqrt(r2))));;
      hold[j] = hold[j] - force;
      frc = frc + force;
    }
    LP[i]->force = LP[i]->force + frc;
  }
  return hold;
}

// *************************************************************
// Calculates local forces within a leaf
// *************************************************************
void self(node* Tr) {
  auto PP = (Tr->particles).data();
  for (size_t i=0; i < Tr->n; i++) {
    particle* pa = PP[i];
    for (size_t j=i+1; j < Tr->n; j++) {
	particle* pb = PP[j];
	vect3d v = (pb->pt) - (pa->pt);
	double r2 = v.dot(v);
	vect3d force = (v * (pa->mass * pb->mass / (r2*sqrt(r2))));
	pb->force = pb->force - force;
	pa->force = pa->force + force;
      }
  }
}

// *************************************************************
// Calculates the direct interactions between and within leaves.
// Since the forces are symmetric, this calculates the force on one
// side (rightNeighbors) while storing them away (in hold).
// It then goes over the other side (leftNeighbors) picking up
// the precalculated results (from hold).
// It does not update both sides immediately since that would 
// generate a race condition.
// *************************************************************
void doDirect(node* a) {
  size_t nleaves = numLeaves(a);
  sequence <node*> Leaves(nleaves);
  getLeaves(a, Leaves.data());

  // calculates interactions and put neighbor's results in hold
  parlay::parallel_for (0, nleaves, [&] (size_t i) {
      size_t rn = Leaves[i]->rightNeighbors.size();
      Leaves[i]->hold = parlay::tabulate(rn, [&] (size_t j) {
	  return direct(Leaves[i], Leaves[i]->rightNeighbors[j].first);}, rn);}, 1);

  // picks up results from neighbors that were left in hold
  parlay::parallel_for (0, nleaves, [&] (size_t i) {
    for (size_t j = 0; j < Leaves[i]->leftNeighbors.size(); j++) {
      node* L = Leaves[i];
      auto [u, v] = L->leftNeighbors[j];
      for (size_t k=0; k < Leaves[i]->n; k++) 
	L->particles[k]->force = L->particles[k]->force + u->hold[v][k];
    }}, 1);

  // calculate forces within a node
  parlay::parallel_for (0, nleaves, [&] (size_t i) {self(Leaves[i]);});
}

// *************************************************************
// STEP
// takes one step and places forces in particles[i]->force
// *************************************************************
void stepBH(sequence<particle*> &particles, double alpha) {
  timer t("CK nbody");
  size_t n = particles.size();
  TRglobal->precompute();

  parlay::parallel_for (0, n, [&] (size_t i) {
      particles[i]->force = vect3d(0.,0.,0.);});

  sequence<particle*> part_copy = particles;

  // build the CK tree
  node* a = buildTree(part_copy, 0);
  t.next("build tree");

  // Sweep up the tree calculating multipole expansions for each node
  upSweep(a);
  t.next("up sweep");

  // Determine all far-field interactions using the CK method
  interactions_count z = interactions(a);
  t.next("interactions");

  // Translate multipole to local expansions along the far-field
  // interactions
  doIndirect(a);
  t.next("do Indirect");

  // Translate the local expansions down the tree to the leaves
  downSweep(a);
  t.next("down sweep");

  // Add in all the direct (near-field) interactions
  doDirect(a);
  t.next("do Direct");

  if(false) cout << "Direct = " << (long) z.direct << " Indirect = " << z.indirect
       << " Boxes = " << numLeaves(a) << endl;
  if (CHECK) {
    if(false) cout << "  Sampled RMS Error = "<< check(particles) << endl;
    t.next("check");
  }
}

} // namespace pbbs_ck_nbody
