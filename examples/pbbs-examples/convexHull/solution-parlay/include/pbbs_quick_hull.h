// Vendored from pbbsbench/benchmarks/convexHull/quickHull/hull.C
// (MIT licensed, (c) Guy Blelloch and the PBBS team).
#pragma once

#include <algorithm>
#include <geometry.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>

namespace pbbs_quick_hull {

using indexT = unsigned int;
using coord = double;
using point = point2d<coord>;

inline parlay::sequence<indexT> quickHull(parlay::sequence<point> const & Points,
                                   parlay::sequence<indexT> Idxs,
                                   indexT l, indexT mid, indexT r) {
  size_t n = Idxs.size();
  if (n <= 1) return Idxs;
  else {
    using cipair = std::pair<coord,indexT>;
    using cipairs = std::pair<cipair,cipair>;
    auto pairMax = [&] (cipairs a, cipairs b) {
      return cipairs((a.first.first > b.first.first) ? a.first : b.first,
                     (a.second.first > b.second.first) ? a.second : b.second);};
    auto ci_monoid = parlay::make_monoid(pairMax,cipairs());

    auto leftFlag = parlay::sequence<bool>::uninitialized(n) ;
    auto rightFlag = parlay::sequence<bool>::uninitialized(n) ;

    point lP = Points[l], midP = Points[mid], rP = Points[r];
    auto P = parlay::delayed_tabulate(n, [&] (size_t i) {
        indexT j = Idxs[i];
        coord lefta = triArea(lP, midP, Points[j]);
        coord righta = triArea(midP, rP, Points[j]);
        leftFlag[i] = lefta > 0.0;
        rightFlag[i] = righta > 0.0;
        return cipairs(cipair(lefta,j),cipair(righta,j));
      });
    cipairs prs = parlay::reduce(P, ci_monoid);
    indexT maxleft = prs.first.second;
    indexT maxright = prs.second.second;

    parlay::sequence<indexT> left = parlay::pack(Idxs, leftFlag);
    parlay::sequence<indexT> right = parlay::pack(Idxs, rightFlag);
    Idxs.clear();

    parlay::sequence<indexT> leftR, rightR;
    parlay::par_do_if(n > 400,
              [&] () {leftR = quickHull(Points, std::move(left), l, maxleft, mid);},
              [&] () {rightR = quickHull(Points, std::move(right), mid, maxright, r);});

    parlay::sequence<indexT> result(leftR.size() + rightR.size() + 1);
    auto xxx = result.head(leftR.size());
    parlay::copy(leftR, xxx);
    result[leftR.size()] = mid;
    auto yyy = result.cut(leftR.size() + 1, result.size());
    parlay::copy(rightR, yyy);
    return result;
  }
}

inline parlay::sequence<indexT> hull(parlay::sequence<point> const &Points) {
  size_t n = Points.size();
  auto pntless = [&] (point a, point b) {
    return (a.x < b.x) || ((a.x == b.x) && (a.y < b.y));};

  auto minmax = parlay::minmax_element(Points, pntless);
  auto min_x_idx = minmax.first - std::begin(Points);
  auto max_x_idx = minmax.second - std::begin(Points);

  using cipair = std::pair<coord,indexT>;
  using cipairs = std::pair<cipair,cipair>;
  auto pairMinMax = [&] (cipairs a, cipairs b) {
      return cipairs((a.first.first < b.first.first) ? a.first : b.first,
                     (a.second.first > b.second.first) ? a.second : b.second);};
  auto ci_monoid = parlay::make_monoid(pairMinMax,cipairs());

  auto upperFlag = parlay::sequence<bool>::uninitialized(n) ;
  auto lowerFlag = parlay::sequence<bool>::uninitialized(n) ;
  auto P = parlay::delayed_tabulate(n, [&] (size_t i) {
    coord a = triArea(Points[min_x_idx], Points[max_x_idx], Points[i]);
    upperFlag[i] = a > 0;
    lowerFlag[i] = a < 0;
    return cipairs(cipair(a,i),cipair(a,i));
    });

  auto max_lower_upper = parlay::reduce(P, ci_monoid);
  size_t max_lower_idx = max_lower_upper.first.second;
  size_t max_upper_idx = max_lower_upper.second.second;

  parlay::sequence<indexT> upper = parlay::internal::pack_index<indexT>(upperFlag);
  parlay::sequence<indexT> lower = parlay::internal::pack_index<indexT>(lowerFlag);

  parlay::sequence<indexT> upperR, lowerR;
  parlay::par_do(
         [&] () {upperR = quickHull(Points, std::move(upper),
                                    min_x_idx, max_upper_idx, max_x_idx);},
         [&] () {lowerR = quickHull(Points, std::move(lower),
                                    max_x_idx, max_lower_idx, min_x_idx);}
         );

  parlay::sequence<indexT> result(upperR.size() + lowerR.size() + 2);
  result[0] = min_x_idx;
  auto xxx = result.cut(1, 1 + upperR.size());
  parlay::copy(upperR, xxx);
  result[1 + upperR.size()] = max_x_idx;
  auto yyy = result.cut(upperR.size() + 2, result.size());
  parlay::copy(lowerR, yyy);
  return result;
}

} // namespace pbbs_quick_hull
