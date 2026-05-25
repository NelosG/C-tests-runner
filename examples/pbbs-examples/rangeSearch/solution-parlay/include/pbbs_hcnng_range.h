// Vendored from pbbsbench/benchmarks/ANN/
//   utils/{types.h, NSGDist.h, indexTools.h, clusterEdge.h, beamSearch.h}
//   HCNNG/{hcnng_index.h, neighbors.h}
// (MIT licensed, (c) Guy Blelloch and the PBBS team.)
// Concatenated into a single TU + wrapped in a namespace so parlay's
// scheduler thread_local ODR is happy.
#pragma once

#include <algorithm>
#include <iostream>
#include <math.h>
#include <queue>
#include <random>
#include <set>
#include <vector>
#include <immintrin.h>     // SSE/AVX intrinsics used by NSGDist's distance fns
#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <parlay/random.h>

namespace pbbs_hcnng_range {

using namespace std;
using parlay::sequence;

// Each vendored file declares `extern bool report_stats;` expecting a
// single definition in the program. Provide it once at namespace scope.
inline bool report_stats = false;

// ===== utils/types.h =====
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

#ifndef TYPES
#define TYPES



//for a file in .fvecs or .bvecs format, but extendible to other types
template<typename T>
struct Tvec_point {
  int id;
  size_t visited;
  size_t dist_calls;  
  int rounds;
  parlay::slice<T*, T*> coordinates;
  parlay::slice<int*, int*> out_nbh; 
  parlay::slice<int*, int*> new_nbh; 
  Tvec_point() :
    coordinates(parlay::make_slice<T*, T*>(nullptr, nullptr)),
    out_nbh(parlay::make_slice<int*, int*>(nullptr, nullptr)),
    new_nbh(parlay::make_slice<int*, int*>(nullptr, nullptr)) {}
  parlay::sequence<int> ngh = parlay::sequence<int>();
};




//for an ivec file, which contains the ground truth
//only info needed is the coordinates of the nearest neighbors of each point
struct ivec_point {
  int id;
  parlay::slice<int*, int*> coordinates;
  parlay::slice<float*, float*> distances;
  ivec_point() :
    coordinates(parlay::make_slice<int*, int*>(nullptr, nullptr)), distances(parlay::make_slice<float*, float*>(nullptr, nullptr)) {}
};

#endif
// ===== utils/NSGDist.h =====
//
// Created by 付聪 on 2017/6/21.
//

#ifndef EFANNA2E_DISTANCE_H
#define EFANNA2E_DISTANCE_H


extern bool report_stats;

namespace efanna2e{

  // atomic_sum_counter<size_t> distance_calls;
  
  enum Metric{
    L2 = 0,
    INNER_PRODUCT = 1,
    FAST_L2 = 2,
    PQ = 3
  };
    class Distance {
    public:
        virtual float compare(const float* a, const float* b, unsigned length) const = 0;
        virtual ~Distance() {}
    };

    class DistanceL2 : public Distance{
    public:
        float compare(const float* a, const float* b, unsigned size) const {
            float result = 0;

#ifdef __GNUC__
#ifdef __AVX__

  #define AVX_L2SQR(addr1, addr2, dest, tmp1, tmp2) \
      tmp1 = _mm256_loadu_ps(addr1);\
      tmp2 = _mm256_loadu_ps(addr2);\
      tmp1 = _mm256_sub_ps(tmp1, tmp2); \
      tmp1 = _mm256_mul_ps(tmp1, tmp1); \
      dest = _mm256_add_ps(dest, tmp1);

      __m256 sum;
      __m256 l0, l1;
      __m256 r0, r1;
      unsigned D = (size + 7) & ~7U;
      unsigned DR = D % 16;
      unsigned DD = D - DR;
      const float *l = a;
      const float *r = b;
      const float *e_l = l + DD;
      const float *e_r = r + DD;
      float unpack[8] __attribute__ ((aligned (32))) = {0, 0, 0, 0, 0, 0, 0, 0};

      sum = _mm256_loadu_ps(unpack);
      if(DR){AVX_L2SQR(e_l, e_r, sum, l0, r0);}

      for (unsigned i = 0; i < DD; i += 16, l += 16, r += 16) {
      	AVX_L2SQR(l, r, sum, l0, r0);
      	AVX_L2SQR(l + 8, r + 8, sum, l1, r1);
      }
      _mm256_storeu_ps(unpack, sum);
      result = unpack[0] + unpack[1] + unpack[2] + unpack[3] + unpack[4] + unpack[5] + unpack[6] + unpack[7];

#else
#ifdef __SSE2__
  #define SSE_L2SQR(addr1, addr2, dest, tmp1, tmp2) \
          tmp1 = _mm_load_ps(addr1);\
          tmp2 = _mm_load_ps(addr2);\
          tmp1 = _mm_sub_ps(tmp1, tmp2); \
          tmp1 = _mm_mul_ps(tmp1, tmp1); \
          dest = _mm_add_ps(dest, tmp1);

  __m128 sum;
  __m128 l0, l1, l2, l3;
  __m128 r0, r1, r2, r3;
  unsigned D = (size + 3) & ~3U;
  unsigned DR = D % 16;
  unsigned DD = D - DR;
  const float *l = a;
  const float *r = b;
  const float *e_l = l + DD;
  const float *e_r = r + DD;
  float unpack[4] __attribute__ ((aligned (16))) = {0, 0, 0, 0};

  sum = _mm_load_ps(unpack);
  switch (DR) {
      case 12:
      SSE_L2SQR(e_l+8, e_r+8, sum, l2, r2);
      case 8:
      SSE_L2SQR(e_l+4, e_r+4, sum, l1, r1);
      case 4:
      SSE_L2SQR(e_l, e_r, sum, l0, r0);
    default:
      break;
  }
  for (unsigned i = 0; i < DD; i += 16, l += 16, r += 16) {
      SSE_L2SQR(l, r, sum, l0, r0);
      SSE_L2SQR(l + 4, r + 4, sum, l1, r1);
      SSE_L2SQR(l + 8, r + 8, sum, l2, r2);
      SSE_L2SQR(l + 12, r + 12, sum, l3, r3);
  }
  _mm_storeu_ps(unpack, sum);
  result += unpack[0] + unpack[1] + unpack[2] + unpack[3];

//normal distance
#else

      float diff0, diff1, diff2, diff3;
      const float* last = a + size;
      const float* unroll_group = last - 3;

      /* Process 4 items with each loop for efficiency. */
      while (a < unroll_group) {
          diff0 = a[0] - b[0];
          diff1 = a[1] - b[1];
          diff2 = a[2] - b[2];
          diff3 = a[3] - b[3];
          result += diff0 * diff0 + diff1 * diff1 + diff2 * diff2 + diff3 * diff3;
          a += 4;
          b += 4;
      }
      /* Process last 0-3 pixels.  Not needed for standard vector lengths. */
      while (a < last) {
          diff0 = *a++ - *b++;
          result += diff0 * diff0;
      }
#endif
#endif
#endif

            return result;
        }
    };

  class DistanceInnerProduct : public Distance{
  public:
    float compare(const float* a, const float* b, unsigned size) const {
      float result = 0;
#ifdef __GNUC__
#ifdef __AVX__
      #define AVX_DOT(addr1, addr2, dest, tmp1, tmp2) \
          tmp1 = _mm256_loadu_ps(addr1);\
          tmp2 = _mm256_loadu_ps(addr2);\
          tmp1 = _mm256_mul_ps(tmp1, tmp2); \
          dest = _mm256_add_ps(dest, tmp1);

	  __m256 sum;
   	  __m256 l0, l1;
   	  __m256 r0, r1;
      unsigned D = (size + 7) & ~7U;
      unsigned DR = D % 16;
      unsigned DD = D - DR;
   	  const float *l = a;
   	  const float *r = b;
      const float *e_l = l + DD;
   	  const float *e_r = r + DD;
      float unpack[8] __attribute__ ((aligned (32))) = {0, 0, 0, 0, 0, 0, 0, 0};

      sum = _mm256_loadu_ps(unpack);
      if(DR){AVX_DOT(e_l, e_r, sum, l0, r0);}

      for (unsigned i = 0; i < DD; i += 16, l += 16, r += 16) {
	    AVX_DOT(l, r, sum, l0, r0);
	    AVX_DOT(l + 8, r + 8, sum, l1, r1);
      }
      _mm256_storeu_ps(unpack, sum);
      result = unpack[0] + unpack[1] + unpack[2] + unpack[3] + unpack[4] + unpack[5] + unpack[6] + unpack[7];

#else
#ifdef __SSE2__
      #define SSE_DOT(addr1, addr2, dest, tmp1, tmp2) \
          tmp1 = _mm128_loadu_ps(addr1);\
          tmp2 = _mm128_loadu_ps(addr2);\
          tmp1 = _mm128_mul_ps(tmp1, tmp2); \
          dest = _mm128_add_ps(dest, tmp1);
      __m128 sum;
      __m128 l0, l1, l2, l3;
      __m128 r0, r1, r2, r3;
      unsigned D = (size + 3) & ~3U;
      unsigned DR = D % 16;
      unsigned DD = D - DR;
      const float *l = a;
      const float *r = b;
      const float *e_l = l + DD;
      const float *e_r = r + DD;
      float unpack[4] __attribute__ ((aligned (16))) = {0, 0, 0, 0};

      sum = _mm_load_ps(unpack);
      switch (DR) {
          case 12:
          SSE_DOT(e_l+8, e_r+8, sum, l2, r2);
          case 8:
          SSE_DOT(e_l+4, e_r+4, sum, l1, r1);
          case 4:
          SSE_DOT(e_l, e_r, sum, l0, r0);
        default:
          break;
      }
      for (unsigned i = 0; i < DD; i += 16, l += 16, r += 16) {
          SSE_DOT(l, r, sum, l0, r0);
          SSE_DOT(l + 4, r + 4, sum, l1, r1);
          SSE_DOT(l + 8, r + 8, sum, l2, r2);
          SSE_DOT(l + 12, r + 12, sum, l3, r3);
      }
      _mm_storeu_ps(unpack, sum);
      result += unpack[0] + unpack[1] + unpack[2] + unpack[3];
#else

      float dot0, dot1, dot2, dot3;
      const float* last = a + size;
      const float* unroll_group = last - 3;

      /* Process 4 items with each loop for efficiency. */
      while (a < unroll_group) {
          dot0 = a[0] * b[0];
          dot1 = a[1] * b[1];
          dot2 = a[2] * b[2];
          dot3 = a[3] * b[3];
          result += dot0 + dot1 + dot2 + dot3;
          a += 4;
          b += 4;
      }
      /* Process last 0-3 pixels.  Not needed for standard vector lengths. */
      while (a < last) {
          result += *a++ * *b++;
      }
#endif
#endif
#endif
      return result;
    }

  };
  class DistanceFastL2 : public DistanceInnerProduct{
   public:
    float norm(const float* a, unsigned size) const{
      float result = 0;
#ifdef __GNUC__
#ifdef __AVX__
#define AVX_L2NORM(addr, dest, tmp) \
    tmp = _mm256_loadu_ps(addr); \
    tmp = _mm256_mul_ps(tmp, tmp); \
    dest = _mm256_add_ps(dest, tmp);

    __m256 sum;
   	__m256 l0, l1;
    unsigned D = (size + 7) & ~7U;
    unsigned DR = D % 16;
    unsigned DD = D - DR;
    const float *l = a;
    const float *e_l = l + DD;
    float unpack[8] __attribute__ ((aligned (32))) = {0, 0, 0, 0, 0, 0, 0, 0};

    sum = _mm256_loadu_ps(unpack);
    if(DR){AVX_L2NORM(e_l, sum, l0);}
	for (unsigned i = 0; i < DD; i += 16, l += 16) {
      AVX_L2NORM(l, sum, l0);
      AVX_L2NORM(l + 8, sum, l1);
    }
    _mm256_storeu_ps(unpack, sum);
    result = unpack[0] + unpack[1] + unpack[2] + unpack[3] + unpack[4] + unpack[5] + unpack[6] + unpack[7];
#else
#ifdef __SSE2__
#define SSE_L2NORM(addr, dest, tmp) \
    tmp = _mm128_loadu_ps(addr); \
    tmp = _mm128_mul_ps(tmp, tmp); \
    dest = _mm128_add_ps(dest, tmp);

    __m128 sum;
    __m128 l0, l1, l2, l3;
    unsigned D = (size + 3) & ~3U;
    unsigned DR = D % 16;
    unsigned DD = D - DR;
    const float *l = a;
    const float *e_l = l + DD;
    float unpack[4] __attribute__ ((aligned (16))) = {0, 0, 0, 0};

    sum = _mm_load_ps(unpack);
    switch (DR) {
        case 12:
        SSE_L2NORM(e_l+8, sum, l2);
        case 8:
        SSE_L2NORM(e_l+4, sum, l1);
        case 4:
        SSE_L2NORM(e_l, sum, l0);
      default:
        break;
    }
    for (unsigned i = 0; i < DD; i += 16, l += 16) {
        SSE_L2NORM(l, sum, l0);
        SSE_L2NORM(l + 4, sum, l1);
        SSE_L2NORM(l + 8, sum, l2);
        SSE_L2NORM(l + 12, sum, l3);
    }
    _mm_storeu_ps(unpack, sum);
    result += unpack[0] + unpack[1] + unpack[2] + unpack[3];
#else
    float dot0, dot1, dot2, dot3;
    const float* last = a + size;
    const float* unroll_group = last - 3;

    /* Process 4 items with each loop for efficiency. */
    while (a < unroll_group) {
        dot0 = a[0] * a[0];
        dot1 = a[1] * a[1];
        dot2 = a[2] * a[2];
        dot3 = a[3] * a[3];
        result += dot0 + dot1 + dot2 + dot3;
        a += 4;
    }
    /* Process last 0-3 pixels.  Not needed for standard vector lengths. */
    while (a < last) {
        result += (*a) * (*a);
        a++;
    }
#endif
#endif
#endif
      return result;
    }
    using DistanceInnerProduct::compare;
    float compare(const float* a, const float* b, float norm, unsigned size) const {//not implement
      float result = -2 * DistanceInnerProduct::compare(a, b, size);
      result += norm;
      return result;
    }
  };
}

float mips_distance(uint8_t *p, uint8_t *q, unsigned d){
  int result = 0;
  for(int i=0; i<d; i++){
    result += ((int32_t) q[i]) *
                  ((int32_t) p[i]);
  }
  return -((float) result);
}

float mips_distance(int8_t *p, int8_t *q, unsigned d){
  int result = 0;
  for(int i=0; i<d; i++){
    result += ((int32_t) q[i]) *
                  ((int32_t) p[i]);
  }
  return -((float) result);
}

float mips_distance(float *q, uint8_t *p, unsigned d){
  float result = 0;
  for(int i=0; i<d; i++){
    result += (q[i]) *
                  ((float) p[i]);
  }
  return -result;
}

float mips_distance(float *q, int8_t *p, unsigned d){
  float result = 0;
  for(int i=0; i<d; i++){
    result += (q[i]) *
                  ((float) p[i]);
  }
  return -result;
}

float mips_distance(float *p, float *q, unsigned d){
    efanna2e::DistanceL2 distfunc;
    float result = 0;
    for(int i=0; i<d; i++){
      result += (q[i]) * (p[i]);
    }
    return -result;
}

float distance(uint8_t *p, uint8_t *q, unsigned d){
  int result = 0;
  for(int i=0; i<d; i++){
    result += ((int32_t)((int16_t) q[i] - (int16_t) p[i])) *
                  ((int32_t)((int16_t) q[i] - (int16_t) p[i]));
  }
  return (float) result;
}

float distance(int8_t *p, int8_t *q, unsigned d){
  int result = 0;
  for(int i=0; i<d; i++){
    result += ((int32_t)((int16_t) q[i] - (int16_t) p[i])) *
                  ((int32_t)((int16_t) q[i] - (int16_t) p[i]));
  }
  return (float) result;
}

float distance(float *q, uint8_t *p, unsigned d){
  float result = 0;
  for(int i=0; i<d; i++){
    result += (q[i] - (float) p[i]) *
                  (q[i] - (float) p[i]);
  }
  return result;
}

float distance(float *q, int8_t *p, unsigned d){
  float result = 0;
  for(int i=0; i<d; i++){
    result += (q[i] - (float) p[i]) *
                  (q[i] - (float) p[i]);
  }
  return result;
}

float distance(float *p, float *q, unsigned d){
    efanna2e::DistanceL2 distfunc;
    return distfunc.compare(p, q, d);
}

#endif //EFANNA2E_DISTANCE_H

// ===== utils/indexTools.h =====
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

#ifndef INDEXTOOLS
#define INDEXTOOLS


//special size function
template<typename T>
int size_of(parlay::slice<T*, T*> nbh){
	int size = 0;
	int i=0;
	while(i<nbh.size() && nbh[i] != -1) {size++; i++;}
	return size;
}

//adding more neighbors
template<typename T>
void add_nbh(int nbh, Tvec_point<T> *p){
	if(size_of(p->out_nbh) >= p->out_nbh.size()){
		std::cout << "error: tried to exceed degree bound " << p->out_nbh.size() << std::endl;
		abort();
	}
	p->out_nbh[size_of(p->out_nbh)] = nbh;
}

template<typename T>
void add_out_nbh(parlay::sequence<int> nbh, Tvec_point<T> *p){
  if (nbh.size() > p->out_nbh.size()) {
    std::cout << "oversize" << std::endl;
    abort();
  }
	for(int i=0; i<p->out_nbh.size(); i++){
		p->out_nbh[i] = -1;
	}
	for(int i=0; i<nbh.size(); i++){
		p->out_nbh[i] = nbh[i];
	}
}

template<typename T>
void add_new_nbh(parlay::sequence<int> nbh, Tvec_point<T> *p){
  if (nbh.size() > p->new_nbh.size()) {
    std::cout << "oversize" << std::endl;
    abort();
  }
	for(int i=0; i<p->new_nbh.size(); i++){
		p->new_nbh[i] = -1;
	}
	for(int i=0; i<nbh.size(); i++){
		p->new_nbh[i] = nbh[i];
	}
}

template<typename T>
void synchronize(Tvec_point<T> *p){
	std::vector<int> container = std::vector<int>();
	for(int j=0; j<p->new_nbh.size(); j++) {
		container.push_back(p->new_nbh[j]); 
	}
	for(int j=0; j<p->new_nbh.size(); j++){
		p->out_nbh[j] = container[j];
	}
	p->new_nbh = parlay::make_slice<int*, int*>(nullptr, nullptr);
}

//synchronization function
template<typename T>
void synchronize(parlay::sequence<Tvec_point<T>*> &v){
	size_t n = v.size();
	parlay::parallel_for(0, n, [&] (size_t i){
		synchronize(v[i]);
	});
}

template<typename T>
void clear(Tvec_point<T>* p){
	for(int j=0; j<p->out_nbh.size(); j++) p->out_nbh[j] = -1;
} 

template<typename T>
void clear(parlay::sequence<Tvec_point<T>*> &v){
	size_t n = v.size();
	parlay::parallel_for(0, n, [&] (size_t i){
		for(int j=0; j<v[i]->out_nbh.size(); j++) v[i]->out_nbh[j] = -1;
	});
} 

#endif  


// ===== utils/clusterEdge.h =====
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


std::pair<size_t, size_t> select_two_random(parlay::sequence<size_t>& active_indices,
	parlay::random& rnd) {
	size_t first_index = rnd.ith_rand(0) % active_indices.size(); 
	size_t second_index_unshifted = rnd.ith_rand(1) % (active_indices.size()-1);
	size_t second_index = (second_index_unshifted < first_index) ?
	second_index_unshifted : (second_index_unshifted + 1);

	return {active_indices[first_index], active_indices[second_index]};
}

struct DisjointSet{
	parlay::sequence<int> parent;
	parlay::sequence<int> rank;
	size_t N; 

	DisjointSet(size_t size){
		N = size;
		parent = parlay::sequence<int>(N);
		rank = parlay::sequence<int>(N);
		parlay::parallel_for(0, N, [&] (size_t i) {
			parent[i]=i;
			rank[i] = 0;
		});		
	}

	void _union(int x, int y){
		int xroot = parent[x];
		int yroot = parent[y];
		int xrank = rank[x];
		int yrank = rank[y];
		if(xroot == yroot)
			return;
		else if(xrank < yrank)
			parent[xroot] = yroot;
		else{
			parent[yroot] = xroot;
			if(xrank == yrank)
				rank[xroot] = rank[xroot] + 1;
		}
	}

	int find(int x){
		if(parent[x] != x)
			parent[x] = find(parent[x]);
		return parent[x];
	}

	void flatten(){
		for(int i=0; i<N; i++) find(i);
	}

	bool is_full(){
		flatten();
		parlay::sequence<bool> truthvals(N);
		parlay::parallel_for(0, N, [&] (size_t i){
			truthvals[i] = (parent[i]==parent[0]);
		});
		auto ff = [&] (bool a) {return not a;};
		auto filtered = parlay::filter(truthvals, ff);
		if(filtered.size()==0) return true;
		return false;
	}

};

template<typename T>
struct cluster{
	unsigned d; 
	bool mips;
	using tvec_point = Tvec_point<T>;
	using edge = std::pair<int, int>;
	using labelled_edge = std::pair<edge, float>;

	cluster(unsigned dim, bool m): d(dim), mips(m) {}

	float Distance(T* p, T* q, unsigned d){
		if(mips) return mips_distance(p, q, d);
		else return distance(p, q, d);
	}

	//inserts each edge after checking for duplicates
	void process_edges(parlay::sequence<tvec_point*> &v, parlay::sequence<edge> edges){
		int maxDeg = v[1]->out_nbh.begin() - v[0]->out_nbh.begin();
		auto grouped = parlay::group_by_key(edges);
		for(auto pair : grouped){
			auto [index, candidates] = pair;
			for(auto c : candidates){
				if(size_of(v[index]->out_nbh) < maxDeg){
					add_nbh(c, v[index]);
				}else{
					remove_edge_duplicates(v[index]);
					add_nbh(c, v[index]);
				}
			}
		}
	}

	void remove_edge_duplicates(tvec_point* p){
		parlay::sequence<int> points;
		for(int i=0; i<size_of(p->out_nbh); i++){
			points.push_back(p->out_nbh[i]);
		}
		auto np = parlay::remove_duplicates(points);
		add_out_nbh(np, p);
	}

	int generate_index(int N, int i){
		return (N*(N-1) - (N-i)*(N-i-1))/2;
	}
	
	//parameters dim and K are just to interface with the cluster tree code
	void MSTk(parlay::sequence<tvec_point*> &v, parlay::sequence<size_t> &active_indices, 
		unsigned dim, int K){
		//preprocessing for Kruskal's
		int N = active_indices.size();
		DisjointSet *disjset = new DisjointSet(N);
		size_t m = 10;
		auto less = [&] (labelled_edge a, labelled_edge b) {return a.second < b.second;};
		parlay::sequence<parlay::sequence<labelled_edge>> pre_labelled(N);
		parlay::parallel_for(0, N, [&] (size_t i){
			std::priority_queue<labelled_edge, std::vector<labelled_edge>, decltype(less)> Q(less);
			for(int j=0; j<N; j++){
				if(j!=i){
					float dist_ij = Distance(v[active_indices[i]]->coordinates.begin(), v[active_indices[j]]->coordinates.begin(), dim);
					if(Q.size() >= m){
						float topdist = Q.top().second;
						if(dist_ij < topdist){
							labelled_edge e;
							if(i<j) e = std::make_pair(std::make_pair(i,j), dist_ij);
							else e = std::make_pair(std::make_pair(j, i), dist_ij);
							Q.pop();
							Q.push(e);
						}
					}else{
						labelled_edge e;
						if(i<j) e = std::make_pair(std::make_pair(i,j), dist_ij);
						else e = std::make_pair(std::make_pair(j, i), dist_ij);
						Q.push(e);
					}
				}
			}
			parlay::sequence<labelled_edge> edges(m);
			for(int j=0; j<m; j++){edges[j] = Q.top(); Q.pop();}
			pre_labelled[i] = edges;
		});
		auto flat_edges = parlay::flatten(pre_labelled);
		// std::cout << flat_edges.size() << std::endl;
		auto less_dup = [&] (labelled_edge a, labelled_edge b){
			auto dist_a = a.second;
			auto dist_b = b.second;
			if(dist_a == dist_b){
				int i_a = a.first.first;
				int j_a = a.first.second;
				int i_b = b.first.first;
				int j_b = b.first.second;
				if((i_a==i_b) && (j_a==j_b)){
					return true;
				} else{
					if(i_a != i_b) return i_a < i_b;
					else return j_a < j_b;
				}
			}else return (dist_a < dist_b);
		};
		auto labelled_edges = parlay::remove_duplicates_ordered(flat_edges, less_dup);
		// parlay::sort_inplace(labelled_edges, less);
		auto degrees = parlay::tabulate(active_indices.size(), [&] (size_t i) {return 0;});
		parlay::sequence<edge> MST_edges = parlay::sequence<edge>();
		//modified Kruskal's algorithm
		for(int i=0; i<labelled_edges.size(); i++){
			labelled_edge e_l = labelled_edges[i];
			edge e = e_l.first;
			if((disjset->find(e.first) != disjset->find(e.second)) && (degrees[e.first]<K) && (degrees[e.second]<K)){
				MST_edges.push_back(std::make_pair(active_indices[e.first], active_indices[e.second]));
				MST_edges.push_back(std::make_pair(active_indices[e.second], active_indices[e.first]));
				degrees[e.first] += 1;
				degrees[e.second] += 1;
				disjset->_union(e.first, e.second);
			}
			if(i%N==0){
				if(disjset->is_full()){
					break;
				}
			}
		}
		delete disjset;
		process_edges(v, MST_edges);
	}

	bool tvec_equal(tvec_point* a, tvec_point* b, unsigned d){
		for(int i=0; i<d; i++){
			if(a->coordinates[i] != b->coordinates[i]){
				return false;
			}
		}
		return true;
	}

	void recurse(parlay::sequence<tvec_point*> &v, parlay::sequence<size_t> &active_indices,
		parlay::random& rnd, size_t cluster_size, 
		unsigned dim, int K, tvec_point* first, tvec_point* second){
		// Split points based on which of the two points are closer.
		auto closer_first = parlay::filter(parlay::make_slice(active_indices), [&] (size_t ind) {
			tvec_point* p = v[ind];
			float dist_first = distance(p->coordinates.begin(), first->coordinates.begin(), d);
			float dist_second = distance(p->coordinates.begin(), second->coordinates.begin(), d);
			return dist_first <= dist_second;

		});

		auto closer_second = parlay::filter(parlay::make_slice(active_indices), [&] (size_t ind) {
			tvec_point* p = v[ind];
			float dist_first = distance(p->coordinates.begin(), first->coordinates.begin(), d);
			float dist_second = distance(p->coordinates.begin(), second->coordinates.begin(), d);
			return dist_second < dist_first;
		});

		auto left_rnd = rnd.fork(0);
		auto right_rnd = rnd.fork(1);

		if(closer_first.size() == 1) {
			random_clustering(v, active_indices, right_rnd, cluster_size, dim, K);
		}
		else if(closer_second.size() == 1){
			random_clustering(v, active_indices, left_rnd, cluster_size, dim, K);
		}
		else{
			parlay::par_do(
				[&] () {random_clustering(v, closer_first, left_rnd, cluster_size, dim, K);}, 
				[&] () {random_clustering(v, closer_second, right_rnd, cluster_size, dim, K);}
			);
		}
	}

	void random_clustering(parlay::sequence<tvec_point*> &v, parlay::sequence<size_t> &active_indices,
		parlay::random& rnd, size_t cluster_size, unsigned dim, int K){
		if(active_indices.size() < cluster_size) MSTk(v, active_indices, dim, K);
		else{
			auto [f, s] = select_two_random(active_indices, rnd);
    		tvec_point* first = v[f];
    		tvec_point* second = v[s];

			if(tvec_equal(first, second, dim)){
				// std::cout << "Equal points selected, splitting evenly" << std::endl;
				parlay::sequence<size_t> closer_first;
				parlay::sequence<size_t> closer_second;
				for(int i=0; i<active_indices.size(); i++){
					if(i<active_indices.size()/2) closer_first.push_back(active_indices[i]);
					else closer_second.push_back(active_indices[i]);
				}
				auto left_rnd = rnd.fork(0);
				auto right_rnd = rnd.fork(1);
				parlay::par_do(
					[&] () {random_clustering(v, closer_first, left_rnd, cluster_size, dim, K);}, 
					[&] () {random_clustering(v, closer_second, right_rnd, cluster_size, dim, K);}
				);
			} else{
				recurse(v, active_indices, rnd, cluster_size, dim, K, first, second);
			}
		}
	}

	void random_clustering_wrapper(parlay::sequence<tvec_point*> &v, size_t cluster_size, 
		unsigned dim, int K){
		std::random_device rd;    
  		std::mt19937 rng(rd());   
  		std::uniform_int_distribution<int> uni(0,v.size()); 
    	parlay::random rnd(uni(rng));
    	auto active_indices = parlay::tabulate(v.size(), [&] (size_t i) { return i; });
    	random_clustering(v, active_indices, rnd, cluster_size, dim, K);
	}

	void multiple_clustertrees(parlay::sequence<tvec_point*> &v, size_t cluster_size, int num_clusters,
		unsigned dim, int K, int bound = 0){
		for(int i=0; i<num_clusters; i++){
			random_clustering_wrapper(v, cluster_size, dim, K);
		}
	}
};
// ===== utils/beamSearch.h =====
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

#ifndef BEAMSEARCH
#define BEAMSEARCH


extern bool report_stats;

using pid = std::pair<int, float>;

// returns true if F \setminus V = emptyset
bool intersect_nonempty(parlay::sequence<pid>& V, parlay::sequence<pid>& F) {
  for (int i = 0; i < F.size(); i++) {
    auto pred = [&](pid a) { return F[i].first == a.first; };
    if (parlay::find_if(V, pred) == V.end()) return true;
  }
  return false;
}

// will only be used when there is an element in F that is not in V
// hence the ``return 0" line will never be called
pid id_next(parlay::sequence<pid>& V, parlay::sequence<pid>& F) {
  for (int i = 0; i < F.size(); i++) {
    auto pred = [&](pid a) { return F[i].first == a.first; };
    if (parlay::find_if(V, pred) == V.end()) return F[i];
  }
  return std::make_pair(0, 0);
}

// for debugging
void print_seq(parlay::sequence<int> seq) {
  int fsize = seq.size();
  std::cout << "[";
  for (int i = 0; i < fsize; i++) {
    std::cout << seq[i] << ", ";
  }
  std::cout << "]" << std::endl;
}

// updated version by Guy
template <typename T>
std::pair<std::pair<parlay::sequence<pid>, parlay::sequence<pid>>, int> beam_search(
    Tvec_point<T>* p, parlay::sequence<Tvec_point<T>*>& v,
    Tvec_point<T>* starting_point, int beamSize, unsigned d, bool mips, int k=0, float cut=1.14, int limit=-1) {
  
  parlay::sequence<Tvec_point<T>*> start_points;
  start_points.push_back(starting_point);
  return beam_search(p, v, start_points, beamSize, d, mips, k, cut, limit);

}

// updated version by Guy
template <typename T>
std::pair<std::pair<parlay::sequence<pid>, parlay::sequence<pid>>, size_t> beam_search(
    Tvec_point<T>* p, parlay::sequence<Tvec_point<T>*>& v,
    parlay::sequence<Tvec_point<T>*> starting_points, int beamSize, unsigned d, bool mips, int k=0, float cut=1.14, int limit=-1) {
  // initialize data structures
  if(limit==-1) limit=v.size();
  size_t dist_cmps = 0;
  auto vvc = v[0]->coordinates.begin();
  long stride = v[1]->coordinates.begin() - v[0]->coordinates.begin();
  std::vector<pid> visited;
  auto less = [&](pid a, pid b) {
      return a.second < b.second || (a.second == b.second && a.first < b.first); };
  auto make_pid = [&] (int q) {
      if(mips) return std::pair{q, mips_distance(vvc + q*stride, p->coordinates.begin(), d)};
      else return std::pair{q, distance(vvc + q*stride, p->coordinates.begin(), d)};
  };
  int bits = std::ceil(std::log2(beamSize*beamSize))-2;
  parlay::sequence<int> hash_table(1 << bits, -1);

  auto pre_frontier = parlay::tabulate(starting_points.size(), [&] (size_t i) {
    return make_pid(starting_points[i]->id);
  });

  dist_cmps += starting_points.size();

  auto frontier = parlay::sort(pre_frontier, less);

  std::vector<pid> unvisited_frontier(beamSize);
  parlay::sequence<pid> new_frontier(beamSize + v[0]->out_nbh.size());
  unvisited_frontier[0] = frontier[0];
  int remain = 1;
  int num_visited = 0;

  // terminate beam search when the entire frontier has been visited
  while (remain > 0 && num_visited<limit) {
    // the next node to visit is the unvisited frontier node that is closest to p
    pid currentPid = unvisited_frontier[0];
    Tvec_point<T>* current = v[currentPid.first];
    auto nbh = current->out_nbh.cut(0, size_of(current->out_nbh));
    auto candidates = parlay::filter(nbh,  [&] (int a) {
	     int loc = parlay::hash64_2(a) & ((1 << bits) - 1);
	     if (a == p->id || hash_table[loc] == a) return false;
	     hash_table[loc] = a;
	     return true;});
    auto pairCandidates = parlay::map(candidates, [&] (long c) {return make_pid(c);}, 1000);
    dist_cmps += candidates.size();
    auto sortedCandidates = parlay::sort(pairCandidates, less);
    auto f_iter = std::set_union(frontier.begin(), frontier.end(),
				 sortedCandidates.begin(), sortedCandidates.end(),
				 new_frontier.begin(), less);
    size_t f_size = std::min<size_t>(beamSize, f_iter - new_frontier.begin());
    if (k > 0 && f_size > k) {
      if(mips){
        f_size = (std::upper_bound(new_frontier.begin(), new_frontier.begin() + f_size,
				std::pair{0, -cut * new_frontier[k].second}, less)
		- new_frontier.begin());
      }
      else{f_size = (std::upper_bound(new_frontier.begin(), new_frontier.begin() + f_size,
				std::pair{0, cut * new_frontier[k].second}, less)
		- new_frontier.begin());}
    }
    frontier = parlay::tabulate(f_size, [&] (long i) {return new_frontier[i];});
    visited.insert(std::upper_bound(visited.begin(), visited.end(), currentPid, less), currentPid);
    auto uf_iter = std::set_difference(frontier.begin(), frontier.end(),
				 visited.begin(), visited.end(),
				 unvisited_frontier.begin(), less);
    remain = uf_iter - unvisited_frontier.begin();
    num_visited++;
  }
  return std::make_pair(std::make_pair(frontier, parlay::to_sequence(visited)), dist_cmps);
}


// searches every element in q starting from a randomly selected point
template <typename T>
void beamSearchRandom(parlay::sequence<Tvec_point<T>*>& q,
                      parlay::sequence<Tvec_point<T>*>& v, int beamSizeQ, int k,
                      unsigned d, bool mips, double cut = 1.14, int limit=-1) {
  // std::cout << "Mips: " << mips << std::endl;
  if ((k + 1) > beamSizeQ) {
    std::cout << "Error: beam search parameter Q = " << beamSizeQ
              << " same size or smaller than k = " << k << std::endl;
    abort();
  }
  // use a random shuffle to generate random starting points for each query
  size_t n = v.size();
  // auto indices = parlay::random_permutation<int>(static_cast<int>(n), time(NULL));

  parlay::random_generator gen;
  std::uniform_int_distribution<long> dis(0, n-1);
  auto indices = parlay::tabulate(q.size(), [&](size_t i) {
    auto r = gen[i];
    return dis(r);
  });

  parlay::parallel_for(0, q.size(), [&](size_t i) {
    parlay::sequence<int> neighbors = parlay::sequence<int>(k);
    size_t index = indices[i];
    Tvec_point<T>* start = v[index];
    parlay::sequence<pid> beamElts;
    parlay::sequence<pid> visitedElts;
    auto [pairElts, dist_cmps] = beam_search(q[i], v, start, beamSizeQ, d, mips, k, cut, limit);
    beamElts = pairElts.first;
    visitedElts = pairElts.second;
    for (int j = 0; j < k; j++) {
      neighbors[j] = beamElts[j].first;
    }
    q[i]->ngh = neighbors;
    if (report_stats) {q[i]->visited = visitedElts.size(); q[i]->dist_calls = dist_cmps; }
  });
}

template <typename T>
void searchAll(parlay::sequence<Tvec_point<T>*>& q,
                      parlay::sequence<Tvec_point<T>*>& v, int beamSizeQ, int k,
                      unsigned d, Tvec_point<T>* starting_point, bool mips, float cut, int limit) {
    // std::cout << "Mips: " << mips <<  std::endl;
    parlay::sequence<Tvec_point<T>*> start_points;
    start_points.push_back(starting_point);
    searchAll(q, v, beamSizeQ, k, d, start_points, mips, cut, limit);
}

template <typename T>
void searchAll(parlay::sequence<Tvec_point<T>*>& q,
                      parlay::sequence<Tvec_point<T>*>& v, int beamSizeQ, int k,
                      unsigned d, parlay::sequence<Tvec_point<T>*> starting_points, bool mips, float cut, int limit) {
  // std::cout << "Mips: " << mips << std::endl;
  if ((k + 1) > beamSizeQ) {
    std::cout << "Error: beam search parameter Q = " << beamSizeQ
              << " same size or smaller than k = " << k << std::endl;
    abort();
  }
  parlay::parallel_for(0, q.size(), [&](size_t i) {
    parlay::sequence<int> neighbors = parlay::sequence<int>(k);
    auto [pairElts, dist_cmps] = beam_search(q[i], v, starting_points, beamSizeQ, d, mips, k, cut, limit);
    auto [beamElts, visitedElts] = pairElts;
      for (int j = 0; j < k; j++) {
        neighbors[j] = beamElts[j].first;
      }
    q[i]->ngh = neighbors;
    q[i]->visited = visitedElts.size();
    q[i]->dist_calls = dist_cmps; 

  });
}

template<typename T>
void rangeSearchAll(parlay::sequence<Tvec_point<T>*> q, parlay::sequence<Tvec_point<T>*>& v, 
  int beamSize, unsigned d, Tvec_point<T>* start_point, double r, int k, double cut, double slack){
    parlay::parallel_for(0, q.size(), [&] (size_t i){
      auto in_range = range_search(q[i], v, beamSize, d, start_point, r, k, cut, slack);
      parlay::sequence<int> nbh;
      for(auto j : in_range) nbh.push_back(j);
      q[i]->ngh = nbh;
    });
}

template<typename T>
void rangeSearchRandom(parlay::sequence<Tvec_point<T>*> q, parlay::sequence<Tvec_point<T>*>& v, 
  int beamSize, unsigned d, double r, int k, double cut = 1.14, double slack = 1.0){
    size_t n = v.size();
    auto indices = parlay::random_permutation<int>(static_cast<int>(n), time(NULL));
    parlay::parallel_for(0, q.size(), [&] (size_t i){
      auto in_range = range_search(q[i], v, beamSize, d, v[indices[i]], r, k, cut, slack);
      parlay::sequence<int> nbh;
      for(auto j : in_range) nbh.push_back(j);
      q[i]->ngh = nbh;
    });
    
}

template<typename T>   
std::set<int> range_search(Tvec_point<T>* q, parlay::sequence<Tvec_point<T>*>& v, 
  int beamSize, unsigned d, Tvec_point<T>* start_point, double r, int k, float cut, double slack){
  
  double max_rad = 0;

  std::set<int> nbh;
  bool mips=false;

  auto [pairElts, dist_cmps] = beam_search(q, v, start_point, beamSize, d, mips, k, cut);
  auto [neighbors, visited] = pairElts;

  q->visited = visited.size();
  q->dist_calls = dist_cmps;
  
  for(auto p : visited){
    if((p.second <= r)) nbh.insert(p.first);
  }

  return nbh;

}
                      

#endif


// ===== HCNNG/hcnng_index.h =====
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


extern bool report_stats;

template<typename T>
struct hcnng_index{
	int maxDeg;
	unsigned d;
	bool mips;
	using tvec_point = Tvec_point<T>;
	using slice_tvec = decltype(make_slice(parlay::sequence<tvec_point*>()));
	using edge = std::pair<int, int>;
	using labelled_edge = std::pair<edge, float>;
	using pid = std::pair<int, float>;

	hcnng_index(int md, unsigned dim, bool m) : maxDeg(md), d(dim), mips(m) {}

	float Distance(T* p, T* q, unsigned d){
		if(mips) return mips_distance(p, q, d);
		else return distance(p, q, d);
	}

	void remove_edge_duplicates(tvec_point* p){
		parlay::sequence<int> points;
		for(int i=0; i<size_of(p->out_nbh); i++){
			points.push_back(p->out_nbh[i]);
		}
		auto np = parlay::remove_duplicates(points);
		add_out_nbh(np, p);
	}

	void remove_all_duplicates(parlay::sequence<tvec_point*> &v){
		parlay::parallel_for(0, v.size(), [&] (size_t i){
			remove_edge_duplicates(v[i]);
		});
	}


	void build_index(parlay::sequence<tvec_point*> &v, int cluster_rounds, size_t cluster_size){ 
		std::cout << "Mips: " << mips << std::endl;
		clear(v); 
		cluster<T> C(d, mips);
		C.multiple_clustertrees(v, cluster_size, cluster_rounds, d, maxDeg);
		remove_all_duplicates(v);
	}
	
};
} // namespace pbbs_hcnng_range
