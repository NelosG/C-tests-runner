// Vendored from pbbsbench/benchmarks/classify/decisionTree/classify.C
// (MIT licensed, (c) Guy Blelloch and the PBBS team).
#pragma once

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <parlay/delayed.h>
#include <parlay/io.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <utility>

namespace pbbs_decision_tree {

constexpr int max_value = 255;
using value = unsigned char;
using row = parlay::sequence<value>;
using rows = parlay::sequence<row>;

struct feature {
  bool discrete;
  int num;
  row vals;
  feature(bool discrete, int num) : discrete(discrete), num(num) {}
  feature(bool d, int n, row v) : discrete(d), num(n), vals(v) {}
};

using features = parlay::sequence<feature>;

using namespace parlay;

inline double infinity_val() {
  return std::numeric_limits<double>::infinity();
}

constexpr size_t min_size = 1;
constexpr double encode_node_factor = 0.0;

struct tree {
  bool is_leaf;
  int feature_index;
  int feature_cut;
  int best;
  size_t size;
  sequence<tree*> children;
  tree(int i, int c, int best, sequence<tree*> children)
    : is_leaf(false), feature_index(i), feature_cut(c), best(best), children(children) {
    size = reduce(delayed_map(children, [] (tree* t) {return t->size;}));}
  tree(int best) : is_leaf(true), best(best), size(1) {}
};

inline tree* Leaf(int best) {
  if (best > max_value) std::abort();
  return new tree(best);
}

inline tree* Internal(int i, int cut, int majority, sequence<tree*> children) {
  return new tree(i, cut, majority, children);
}

template <typename S1, typename S2>
auto delayed_zip(S1 const &a, S2 const &b) {
  return delayed_tabulate(a.size(), [&] (size_t i) {return std::pair(a[i],b[i]);});
}

template <typename S1>
auto all_equal(S1 const &a) {
  return (a.size() == 0) || (count(a, a[0]) == a.size());
}

template <typename S1>
auto majority_(S1 const &a, size_t m) {
  auto x = histogram_by_index(a,m);
  return max_element(x) - x.begin();
}

template <typename Seq>
double entropy(Seq a, int total) {
  double ecost = encode_node_factor * std::log2(float(1 + total));
  return ecost + reduce(delayed_map(a, [=] (int l) {
      return (l > 0) ? -(l * std::log2(float(l)/total)) : 0.0;}));
}

inline auto cond_info_continuous(feature const &a, feature const &b) {
  int num_buckets = a.num * b.num;
  size_t n = a.vals.size();
  auto sums = histogram_by_index(delayed_tabulate(n, [&] (size_t i) {
                           return a.vals[i] + b.vals[i]*a.num;}), num_buckets);
  sequence<int> low_counts(a.num, 0);
  sequence<int> high_counts(a.num, 0);
  for (int i=0; i < b.num; i++)
    for (int j=0; j < a.num; j++) high_counts[j] += sums[a.num*i + j];
  double cur_e = infinity_val();
  int cur_i = 0;
  int m = 0;
  for (int i=0; i < b.num-1; i++) {
    for (int j=0; j < a.num; j++) {
      low_counts[j] += sums[a.num*i + j];
      high_counts[j] -= sums[a.num*i + j];
      m += sums[a.num*i + j];
    }
    double e = entropy(low_counts, m) + entropy(high_counts, n - m);
    if (e < cur_e) {
      cur_e = e;
      cur_i = i+1;
    }
  }
  return std::pair(cur_e, cur_i);
}

inline double info(row s, int num_vals) {
  size_t n = s.size();
  if (n == 0) return 0.0;
  auto x = histogram_by_index(s, num_vals);
  return entropy(x, n);
}

inline double cond_info_discrete(feature const &a, feature const &b) {
  int num_buckets = a.num * b.num;
  size_t n = a.vals.size();
  auto sums = histogram_by_index(delayed_tabulate(n, [&] (size_t i) {
                           return a.vals[i] + b.vals[i]*a.num;}), num_buckets);
  return reduce(tabulate(b.num, [&] (size_t i) {
      auto x = sums.cut(i*a.num,(i+1)*a.num);
      return entropy(x, reduce(x));}));
}

inline tree* build_tree(features &A, bool verbose) {
  int num_features = A.size();
  int num_entries = A[0].vals.size();
  int majority_value = (num_entries == 0) ? -1 : majority_(A[0].vals, A[0].num);
  if (num_entries < 2 || all_equal(A[0].vals))
    return Leaf(majority_value);
  double label_info = info(A[0].vals,A[0].num);
  auto costs = tabulate(num_features - 1, [&] (int i) {
      if (A[i+1].discrete) {
        return std::tuple<double, int, int>(cond_info_discrete(A[0], A[i+1]), i+1, -1);
      } else {
        auto info_cut = cond_info_continuous(A[0], A[i+1]);
        return std::tuple<double, int, int>(info_cut.first, i+1, info_cut.second);
      }},1);

  auto min1 = [&] (auto a, auto b) {return (std::get<0>(a) < std::get<0>(b)) ? a : b;};
  auto min_m = make_monoid(min1, std::tuple<double, int, int>(infinity_val(), 0, 0));
  auto [best_info, best_i, cutx] = reduce(costs, min_m);
  auto cut = cutx;
  double threshold = std::log2(float(num_features));

  if (label_info - best_info < threshold)
    return Leaf(majority_value);
  else {
    int m;
    row split_on;
    if (A[best_i].discrete) {
      m = A[best_i].num;
      split_on = A[best_i].vals;
    } else {
      m = 2;
      split_on = map(A[best_i].vals, [&] (value x) -> value {return x >= cut;});
    }

    features F = map(A, [&] (feature a) {return feature(a.discrete, a.num);});
    sequence<features> B(m, F);
    parallel_for (0, num_features, [&] (size_t j) {
      auto x = group_by_index(delayed_zip(split_on, A[j].vals), m);
      for (int i=0; i < m; i++) B[i][j].vals = std::move(x[i]);
    }, 1);

    auto children = map(B, [&] (features &a) {return build_tree(a, verbose);}, 1);
    return Internal(best_i - 1, cut, majority_value, children);
  }
}

inline int classify_row(tree* T, row const&r) {
  if (T->is_leaf) {
    return T->best;
  } else if (T->feature_cut == -1) {
    if (!(r[T->feature_index] < (int)T->children.size())) return T->best;
    int val = classify_row(T->children[r[T->feature_index]], r);
    return (val == -1) ? T->best : val;
  } else {
    int idx = (r[T->feature_index] < T->feature_cut) ? 0 : 1;
    int val = classify_row(T->children[idx], r);
    return (val == -1) ? T->best : val;
  }
}

inline row classify(features const &Train, rows const &Test) {
  features A = Train;
  tree* T = build_tree(A, false);
  return map(Test, [&] (row const& r) -> value {return classify_row(T, r);});
}

} // namespace pbbs_decision_tree
