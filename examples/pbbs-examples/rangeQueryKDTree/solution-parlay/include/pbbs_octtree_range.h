// Vendored from pbbsbench/benchmarks/nearestNeighbors/octTree/
//   {qknn.hpp, oct_tree.h, k_nearest_neighbors.h}
// plus the RNG / range_search_rec helper from
//   pbbsbench/benchmarks/concurrentKNN/octTree/k_nearest_neighbors.h
// (MIT licensed, (c) Guy Blelloch and the PBBS team.)
//
// The static-tree range query does not need flock / verlib - those
// guard concurrent insert/delete, and we treat the tree as read-only.
//
// Concatenated into a single TU so parlay scheduler thread_local ODR
// is happy.
#pragma once

#include <algorithm>
#include <iostream>
#include <math.h>
#include <queue>
#include <vector>
#include <parlay/alloc.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>

#include <geometry.h>

namespace pbbs_octtree_range {

using namespace std;
using parlay::sequence;
using timer = parlay::internal::timer;
inline bool report_stats = false;
inline int algorithm_version = 2;


/*! \file qknn.hpp
\brief Implements priority queue functions for dpoints and point pairs */

//! Priority Queue element comparator
/*! This class orders priority queue elements based on the distance
  given as the first item in the pair 
*/

template <class vtx>
class q_intelementCompare {   
public:

  //! Less than operator
  /*! 
    Compares two priority queue elements based on thier distance
    \param p1 First element to be compared
    \param p2 Second element to be compared
    \return Returns true if p1 distance is less than p2 distance
  */
  bool operator()( const std::pair<vtx*, double> p1,  
                   const std::pair<vtx*, double> p2 ){
    return p1.second < p2.second;
  }
};

//! Distance Priority Queue
/*! 
  Implements a priority queue for pairs of floating point 
  distances and array indexes.  The priority queue is ordered 
  based on the squared distance stored in the first element
  of the pair.
*/

template <class vtx>
class qknn 
{
private:
  long unsigned int K;
  typedef std::pair<vtx*, double> q_intelement;
  //typedef std::priority_queue<q_intelement, parlay::sequence<q_intelement>, q_intelementCompare>
  typedef std::priority_queue<q_intelement, std::vector<q_intelement>, q_intelementCompare<vtx>>  
  PQ;
  PQ pq;
  
public:
  
  //! Constructor
  /*! 
    Creates an empty priority  queue.
   */
  qknn(){};
  
  //! Largest distance
  /*! 
    Returns the largest distance value stored in the priority queue
    \return Largest distance value
  */

  void pop(){
    pq.pop();
  }

  q_intelement top(){
    return pq.top();
  }

  double topdist(void)
  {
    return pq.top().second;
  }
  
  //! Set Size
  /*! 
    Sets the size of the priority queue.  This should be set before the
    queue is used
    \param k The maximum number of elements to be stored in the queue.
  */
  void set_size(long unsigned int k)
  {
    K = k;
  }
  
  /*bool is_element(double dist, long int p)
  {
  }*/
  //! Point with largest distance
  /*!
    Returns the index associated with the largest element in the queue.
    \return Index of largest (most distant) element
  */



  //! Update queue
  /*! 
    Updates the queue with the given distance and point
    \param dist Distance of point to be added
    \param p index of point to be added
    \return True if a point was added to the queue
  */
  bool update(vtx* v, double d)
  {
    if(size() < K)
      {
	q_intelement tq(v, d);
	pq.push(tq);
	return true;
      }
    else if(topdist() > d)
      {
	pq.pop();
	q_intelement tq(v, d);
	pq.push(tq);
	return true;
      }
    return false;
  }


  //! Size function
  /*! 
    Returns the current size of the queue 
    \return Size
  */  
   long unsigned int size(){ return pq.size(); }
};

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

  using point = typename vtx::pointT;
  using uint = unsigned int;
  using box = std::pair<point,point>;
  using indexed_point = std::pair<size_t,vtx*>;
  using slice_t = decltype(make_slice(parlay::sequence<indexed_point>()));
  using slice_v = decltype(make_slice(parlay::sequence<vtx*>()));

  constexpr static int node_cutoff = 32;


  


  // takes a point, rounds each coordinate to an integer, and interleaves
  // the bits into "key_bits" total bits.
  // min_point is the minimmum x,y,z coordinate for all points
  // delta is the largest range of any of the three dimensions
  static size_t interleave_bits(point p, point min_point, double delta) {
    int dim = p.dimension();
    int bits = key_bits/dim;
    uint maxval = (((size_t) 1) << bits) - 1; //maybe should just be size_t instead of uint
    uint ip[dim];
    for (int i = 0; i < dim; i++) 
      ip[i] = floor(maxval * (p[i] - min_point[i])/delta); //could be something other than floor? nearest to?
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
    if(V.size()==0) abort();
    size_t n = V.size();
    auto minmax = [&] (box x, box y) {
      return box(x.first.minCoords(y.first),
     x.second.maxCoords(y.second));};
    // uses a delayed sequence to avoid making a copy
    auto pts = parlay::delayed_seq<box>(n, [&] (size_t i) {
      return box(V[i]->pt, V[i]->pt);});
    box identity = pts[0];  
    box final = parlay::reduce(pts, parlay::make_monoid(minmax, identity));
    return (final);
  }

  struct node { 

  public:
    bool flag = false; 
    int bit;
    parlay::sequence<indexed_point> indexed_pts;
    using leaf_seq = parlay::sequence<vtx*>;
    point center() {return centerv;}
    box Box() {return b;}
    size_t size() {return n;}
    bool is_leaf() {return (L == nullptr) && (R == nullptr);}
    node* Left() {return L;}
    node* Right() {return R;}
    node* Parent() {return parent;}
    leaf_seq& Vertices() {return P;}

    //the flag is for the batch dynamic updates
    //it keeps track of whether a node has been updated or not
    //so that we can go back and fix boxes etc where necessary
    void set_flag(bool new_flag){
      flag = new_flag; 
    }

    void set_bit(int currentBit){
      bit = currentBit;
    }

    void set_size(int size){
      n = size;
    }

    void set_vertices(parlay::sequence<vtx*> Vertices0){
      size_t n = Vertices0.size();
      P.clear();
      P.resize(n);
      for(size_t i=0; i<n; i++){
        P[i] = Vertices0[i];
      }
    }

    void set_idpts(parlay::sequence<indexed_point> idpts){
      size_t n = idpts.size();
      indexed_pts.clear();
      indexed_pts.resize(n);
      for(size_t i=0; i<n; i++){
        indexed_pts[i] = idpts[i];
      }
    }

    void reset_center(){
      set_center();
    }

    void set_box(box B){
      b = B; 
    }

    void set_parent(node* Parent){
      parent = Parent; 
    }

    void set_child(node* child, bool left){
      if(left) L = child;
      else R = child;
    }

    void batch_update(slice_t new_points){
      int new_size = new_points.size();
      for(size_t i=0; i<new_size; i++){
        indexed_pts.push_back(new_points[i]);
        P.push_back(new_points[i].second);
      }
      n += new_size;
      b = get_box(P);
      set_center();
    }

    static void print_point(point p){
      int d = p.dimension();
      std::cout << "Point: ";
      for(int j=0; j<d; j++){
        std::cout << p[j] << ", ";
      }
      std::cout << "\n";
    }

    void print_seq(parlay::sequence<vtx*> v){
      for(size_t i=0; i<v.size(); i++) print_point(v[i]->pt);
    }

    void print_slice(slice_t v){
      for(size_t i=0; i<v.size(); i++) print_point(v[i].second->pt);
    }

    bool are_equal(point p, point q, int d){
      for(int i=0; i<d; i++){
        if(p[i] != q[i]) return false;
      }
      return true;
    }

    void batch_remove(slice_t points_to_delete){
      int deleted_size = points_to_delete.size();
      parlay::sequence<bool> indices_to_retain(P.size(), true);
      for(size_t i=0; i<deleted_size; i++){
        for(size_t j=0; j<P.size(); j++){
          if(are_equal(points_to_delete[i].second->pt, P[j]->pt, P[j]->pt.dimension())){
            indices_to_retain[j]=false;
            break;
          }
        }
      }
      auto new_P = parlay::pack(P, indices_to_retain);
      P = new_P;
      auto new_idpts = parlay::pack(indexed_pts, indices_to_retain);
      indexed_pts = new_idpts;
      n -= deleted_size;
      b = get_box(P);
      set_center();
    }


    // construct a leaf node with a sequence of points directly in it
    node(slice_t Pts, int currentBit) { 
      n = Pts.size();
      parent = nullptr;

      // strips off the integer tag, no longer needed
      P = leaf_seq(n);
      indexed_pts = parlay::sequence<indexed_point>(n);
      for (int i = 0; i < n; i++) {
        P[i] = Pts[i].second;
        indexed_pts[i] = Pts[i];  
      }
      L = R = nullptr;
      b = get_box(P);
      set_center();
      set_bit(currentBit);
    }

    // construct an internal binary node
    node(node* L, node* R, int currentBit) : L(L), R(R) { 
      parent = nullptr;
      b = box(L->b.first.minCoords(R->b.first),
	      L->b.second.maxCoords(R->b.second));
      n = L->size() + R->size();
      set_center();
      set_bit(currentBit);
    }
    
    static node* new_leaf(slice_t Pts, int currentBit) {
      node* r = alloc_node();
      new (r) node(Pts, currentBit);
      return r;
    }

    static node* new_node(node* L, node* R, int currentBit) {
      node* nd = alloc_node();
      new (nd) node(L, R, currentBit);
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
    // in a pointer to a vertex, and a pointer to the leaf node it is in.
    // f should return void

    //pass in a function to compute nearest neighbors
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
  }; // this ends the node structure


    //takes in a sequence of points and a leaf node and splits based on the leaf node
    //TODO fix edge case where T has no parent 
    static void batch_split(slice_t new_points, node* T, int bit){
      //get the new sequence of indexed points and sort it
      int size = T->size();
      int new_size = new_points.size();
      parlay::sequence<indexed_point> indexed_points;
      indexed_points = parlay::sequence<indexed_point>(size+new_size);
      for(size_t i=0; i<size; i++){
        indexed_points[i] = (T->indexed_pts)[i];
      }
      for(size_t i=0; i<new_size; i++){
        indexed_points[size+i] = new_points[i];
      }
      auto less = [] (indexed_point a, indexed_point b){
        return a.first < b.first; 
      };
      auto x = parlay::sort(indexed_points, less);
      //get a new tree based on the sorted sequence
      node* parent = build_recursive(parlay::make_slice(x), bit);
      // std::cout << "built tree" << std::endl;
      //set everyone's parent pointers and delete the old node
      node* grandparent = T->Parent();
      if(grandparent->Left() == T) grandparent->set_child(parent, true);
      else grandparent->set_child(parent, false);
      parent->set_parent(grandparent);
      T->set_parent(nullptr);
      node::delete_tree(T);
    }

    //occasionally, inserting a point will require not splitting an existing node but creating a new one
    //this function creates the new node and a new intermediate node
    static void create_new(node* T, slice_t indexed_points, int bit, bool points_left){
      if (indexed_points.size() == 0) return; 
      node* new_int = build_recursive(indexed_points, bit-1);
      node* T_parent = T->Parent();
      node* intermediate;
      if(points_left) intermediate = node::new_node(new_int, T, bit);
      else intermediate = node::new_node(T, new_int, bit);
      intermediate->set_parent(T_parent);
      bool T_left = (T == T_parent->Left());
      T_parent->set_child(intermediate, T_left);
      T_parent->set_size(T_parent->Left()->size() + T_parent->Right()->size());
    }

    //delete a node and all its children
    //replace the node's parent with its other child
    //must then also set the grandparent's pointer correctly
    static void prune(node* T){
      if(T->Parent() == nullptr || (T->Parent())->Parent() == nullptr){
        std::cout << "ERROR: deleting the root or one of its two children is not supported" << std::endl;
        abort(); 
      } 
      if(T == (T->Parent())->Left()) (T->Parent())->set_child(nullptr, true);
      else (T->Parent())->set_child(nullptr, false);
      node::delete_tree(T);
    }

    //gets rid of any nodes which only have one child due to pruning
    //can assume due to constraints on prune() that the deleted node has a parent
    static void compress(node* T){
      if(T->is_leaf()) return;
      if(T->flag == false) return; 
      if((T->Left() != nullptr) && (T->Right() != nullptr)){
        parlay::par_do_if(T->size()>1000,
          [&] () {compress(T->Right());},
          [&] () {compress(T->Left());}
        );
      }else{
        node* grandparent = T->Parent();
        node* T_replacement;
        if(T->Left() == nullptr){
          T_replacement = T->Right();
          T -> set_child(nullptr, false);
        } else if(T->Right() == nullptr){ //the right child of T is the nullptr
          T_replacement = T->Left();
          T -> set_child(nullptr, true);
        }
        T_replacement -> set_parent(grandparent);
        if(T == grandparent -> Left()) grandparent -> set_child(T_replacement, true);
        else grandparent -> set_child(T_replacement, false);
        node::delete_tree(T);
        compress(T_replacement);
      }  
    }

    static void verify_compress(node* T){
      if((T->Left() == nullptr) && (T->Right() == nullptr)) return;
      else if((T->Left() != nullptr) && (T->Right() != nullptr)){
        verify_compress(T->Left());
        verify_compress(T->Right());
      } else{
        if((T->Left() == nullptr) && (T->Right() != nullptr)){
           verify_compress(T->Right());
        } else if((T->Left() != nullptr) && (T->Right() == nullptr)){
           verify_compress(T->Left());
        }
        std::cout << "ERROR: node with only one nullptr child exists in tree" << std::endl; 
        abort();
      }

    }

    static void verify_parents0(node* T){
      if(T->Parent() == nullptr){
        std::cout << "ERROR: parent of a non-root node is null" << std::endl; 
        abort();
      }
      if(T->is_leaf()) return;
      verify_parents0(T->Left());
      verify_parents0(T->Right());
    }

    static void verify_parents(node* T){
      verify_parents0(T->Left());
      verify_parents0(T->Right());
    }

    static box update_boxes(node* T){
      if(T->flag == false){ //if the node was not traversed during the update, stop recursing
        // std::cout << "here" << std::endl; 
        return T->Box();
      }
      if (T->is_leaf()){
        return T->Box();
        T->set_flag(false);
      } else{
        size_t n = T->size();
        box b_L, b_R;
        parlay::par_do_if(n > 1000,
          [&] () {b_L = update_boxes(T->Left());},
          [&] () {b_R = update_boxes(T->Right());} 
        );
        box b_T = box(b_L.first.minCoords(b_R.first), b_L.second.maxCoords(b_R.second));
        T->set_box(b_T);
        T->reset_center();
        T->set_flag(false);
        return b_T;
      }
    }



  
  // A unique pointer to a tree node to ensure the tree is
  // destructed when the pointer is, and that  no copies are made.
  struct delete_tree {void operator() (node *T) const {node::delete_tree(T);}};
  using tree_ptr = std::unique_ptr<node,delete_tree>;

  // build a tree given a sequence of pointers to points
  template <typename Seq>
  static tree_ptr build(Seq &P) {
    timer t("oct_tree", false);
    int dims = (P[0]->pt).dimension();
    auto pts = tag_points(P);
    t.next("tag");
    node* r = build_recursive(make_slice(pts), dims*(key_bits/dims));
    t.next("build");
    return tree_ptr(r);
  }

    // build a tree given a sequence of pointers to points
  template <typename Seq>
  static tree_ptr build(Seq &P, box b) {
    timer t("oct_tree", false);
    int dims = (P[0]->pt).dimension();
    auto pts = tag_points(P, b);
    t.next("tag");
    node* r = build_recursive(make_slice(pts), dims*(key_bits/dims));
    t.next("build");
    return tree_ptr(r);
  }

  static parlay::sequence<indexed_point> tag_points_external(parlay::sequence<vtx*> &V) {
    return tag_points(V);
  }

private:
  constexpr static int key_bits = 64;
 

  // tags each point (actually a pointer to it), with an interger
  // consisting of the interleaved bits for the x,y,z coordinates.
  // Also sorts based the integer.
  static parlay::sequence<indexed_point> tag_points(parlay::sequence<vtx*> &V) {
    timer t("tag", false); //tag is an arbitrary string, turn to true for printing out
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
      }); //make this not a delayed sequence, tabulate instead, so that we can use t.next()
    
    auto less = [] (indexed_point a, indexed_point b) {
      return a.first < b.first;};
    
    auto x = parlay::sort(points, less);
    t.next("tabulate and sort");
    return x;
  }

    static parlay::sequence<indexed_point> tag_points(parlay::sequence<vtx*> &V, box b) {
    timer t("tag", false); //tag is an arbitrary string, turn to true for printing out
    size_t n = V.size();
    int dims = (V[0]->pt).dimension();

    // find box around points, and size along largest axis
    double Delta = 0;
    for (int i = 0; i < dims; i++) 
      Delta = std::max(Delta, b.second[i] - b.first[i]); 
    t.next("get box");
    
    auto points = parlay::delayed_seq<indexed_point>(n, [&] (size_t i) -> indexed_point {
  return std::pair(interleave_bits(V[i]->pt, b.first, Delta), V[i]);
      }); //make this not a delayed sequence, tabulate instead, so that we can use t.next()
    
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

    // if run out of bit, or small then generate a leaf
    if (bit == 0 || n < node_cutoff) {
      // std::cout << "creating leaf" << std::endl;
      // std::cout << Pts.size() << std::endl;
      node* N = node::new_leaf(Pts, bit);
      // std::cout << "made leaf" << std::endl;
      return N; 
    } else {

      // this was extracted to lookup_bit but left as is here since the less function requires mask and val
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
	return node::new_node(L,R, bit); 
      }
    }
  }

}; //end octTree structure 

  // uses the parlay memory manager, could be replaced will alloc/free

template <typename vtx>
parlay::type_allocator<typename oct_tree<vtx>::node> node_allocator;

template <typename vtx>
typename oct_tree<vtx>::node* oct_tree<vtx>::node::alloc_node() { return node_allocator<vtx>.alloc();}

template <typename vtx>
void oct_tree<vtx>::node::free_node(node* T) { node_allocator<vtx>.free(T);}
  
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

int queue_cutoff = 50;   





// A k-nearest neighbor structure
// requires vertexT to have pointT and vectT typedefs
template <class vtx, int max_k>
struct k_nearest_neighbors {
  using vtx_dist = std::pair<vtx*, double>;
  using point = typename vtx::pointT;
  using fvect = typename point::vector;
  using o_tree = oct_tree<vtx>;
  using node = typename o_tree::node;
  using tree_ptr = typename o_tree::tree_ptr;
  using box = typename o_tree::box;
  using slice_t = typename o_tree::slice_t;

  tree_ptr tree;

  box tree_box; 

  bool box_eq(box b, box c, int d){
    bool first = true;
    bool second = true;
    for(int i=0; i<d; i++){
      first = first && (b.first[i] == c.first[i]);
      second = second && (b.second[i] == c.second[i]);
    }
    return (first && second);
  }

  void are_equal(node* T, int d){
    node* V = tree.get();
    return are_equal_rec(V, T, d);
  }

  void are_equal_rec(node* V, node* T, int d){
    if(T->bit != V->bit){
      std::cout << "UNEQUAL: bit" << std::endl;
      std::cout << "Inserted tree has bit " << V->bit << " while regular tree has bit " << T->bit << std::endl;
    }
    if(!box_eq(T->Box(), V->Box(), d)){
      std::cout << "UNEQUAL: box" << std::endl;
    }
    if(!(T->is_leaf()) && !(V->is_leaf())){
      are_equal_rec(T->Left(), V->Left(), d);
      are_equal_rec(T->Right(), V->Right(), d);
      return;
    }
    else if(T->is_leaf() && V->is_leaf()){
      if(T->size() != V->size()){
        std::cout << "UNEQUAL: leaf size" << std::endl;
        std::cout << "Inserted tree has leaf size " << V->size() << " while regular tree has leaf size " << T->size() << std::endl;
        std::cout << "Leaves have bit " << V->bit << std::endl;
      } //not a true eq check
      return;
    } else{
      std::cout << "UNEQUAL: internal node vs leaf node" << std::endl;
      abort();
    }
  }

  void set_box(box b){
    tree_box = b; 
  }

    // generates the search structure
  k_nearest_neighbors(parlay::sequence<vtx*> &V) {
    tree = o_tree::build(V); 
    node* root = tree.get();
    set_box(root->Box());
  }

  k_nearest_neighbors(parlay::sequence<vtx*> &V, box b) {
    //TODO add safety check
    box points_box = o_tree::get_box(V);
    int dims = V[0]->pt.dimension();
    bool ll_bad = false;
    bool ur_bad = false;
    for(int i=0; i<dims; i++){
      if(points_box.first[i] < b.first[i]){
        ll_bad = true;
      }
      if(points_box.second[i] > b.second[i]){
        ur_bad = true;
      }
    }
    if(ll_bad || ur_bad){
      std::cout << "ERROR: user-specified box does not contain dataset" << std::endl;
      abort();
    }
    set_box(b);
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
    double max_distance; // needed since we may need to update our biggest boi without a vector
    int k;
    int dimensions;
    size_t leaf_cnt;
    size_t internal_cnt;
    qknn<vtx> nearest_nbh;      


    kNN() {}


    // returns the ith smallest element (0 is smallest) up to k-1
    // no need to make a queue equivalent
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
      if (k < queue_cutoff){
        for (int i=0; i<k; i++) {
        	neighbors[i] = (vtx*) NULL; 
        	distances[i] = numeric_limits<double>::max();
        }
      } else{
        nearest_nbh = qknn<vtx>();
        nearest_nbh.set_size(k);
      }
      max_distance = numeric_limits<double>::max();
    }
    

    // if p is closer than neighbors[0] then swap it in
    void update_nearest(vtx *other) {  
      auto dist = (vertex->pt - other->pt).sqLength();
      if (dist < max_distance) { 
      	neighbors[0] = other;
      	distances[0] = dist;
      	for (int i = 1;
      	     i < k && distances[i-1] < distances[i];
      	     i++) {
      	  swap(distances[i-1], distances[i]);
      	  swap(neighbors[i-1], neighbors[i]); 
        }
        max_distance = distances[0];
      }
    }

    //put into queue if vtx is closer than the furthest neighbor
    void update_nearest_queue(vtx* other){
      auto dist = (vertex->pt - other->pt).sqLength();
      bool updated = nearest_nbh.update(other, dist);
      if (updated){
        max_distance = nearest_nbh.topdist();
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
      return (T->center() - vertex->pt).sqLength();
    }

   

    // sorted backwards
    void merge(kNN &L, kNN &R) {
      int i = k-1;
      int j = k-1;
      int r = k-1;
      while (r >= 0) {
	if (L.distances[i] < R.distances[j]) {
	  distances[r] = L.distances[i];
	  neighbors[r] = L.neighbors[i];
	  i--; 
	} else {
	  distances[r] = R.distances[j];
	  neighbors[r] = R.neighbors[j];
	  // same neighbor could appear in both lists
	  if (L.neighbors[i] == R.neighbors[j]) i--;
	  j--;
	}
	r--;
      }
    }
    
    // looks for nearest neighbors for pt in Tree node T
    void k_nearest_rec(node* T) {
      if (within_epsilon_box(T, sqrt(max_distance))) { 
        if (report_stats) internal_cnt++;
	       if (T->is_leaf()) {
	         if (report_stats) leaf_cnt+=T->size();
	         auto &Vtx = T->Vertices();
	         for (int i = 0; i < T->size(); i++)
	           if (Vtx[i] != vertex){ 
                if (k < queue_cutoff){
                  update_nearest(Vtx[i]);
                } else{
                  update_nearest_queue(Vtx[i]);
                }
              } 
	} else if (T->size() > 10000 && algorithm_version != 0 && k < queue_cutoff) { 
	  auto L = *this; // make copies of the distances
	  auto R = *this; // so safe to call in parallel
	  parlay::par_do([&] () {L.k_nearest_rec(T->Left());},
			 [&] () {R.k_nearest_rec(T->Right());});
	  merge(L,R); // merge the results
	} else if (distance(T->Left()) < distance(T->Right())) {
	  k_nearest_rec(T->Left());
	  k_nearest_rec(T->Right());
	} else {
	  k_nearest_rec(T->Right());
	  k_nearest_rec(T->Left());
	}
      }
    }

  void k_nearest_fromLeaf(node* T) {
    
    node* current = T; //this will be the node that node*T points to
    if (current -> is_leaf()){
        if (report_stats) leaf_cnt+=T->size();
        auto &Vtx = T->Vertices();
        for (int i = 0; i < T->size(); i++)
          if (Vtx[i] != vertex){
            if (k < queue_cutoff){
              update_nearest(Vtx[i]);
            } else{
              update_nearest_queue(Vtx[i]);
            }
          }
      } 
    while((not within_epsilon_box(current, -sqrt(max_distance))) and (current -> Parent() != nullptr)){ 
      node* parent = (current -> Parent());
      if (current == parent -> Right()){
        k_nearest_rec(parent -> Left());
      } else{
        k_nearest_rec(parent -> Right());
      }
      current = parent;  
    }
  }

  }; // this ends the knn structure


  using box_delta = std::pair<box, double>;

  box_delta get_box_delta(int dims){
    box b = tree_box; 
    double Delta = 0;
    for (int i = 0; i < dims; i++) 
      Delta = std::max(Delta, b.second[i] - b.first[i]);
    box_delta bd = make_pair(b, Delta);
    return bd;
  }


   // takes in an integer and a position in said integer and returns whether the bit at that position is 0 or 1
  int lookup_bit(size_t interleave_integer, int pos){ //pos must be less than key_bits, can I throw error if not?
    size_t val = ((size_t) 1) << (pos - 1);
    size_t mask = (pos == 64) ? ~((size_t) 0) : ~(~((size_t) 0) << pos);
    if ((interleave_integer & mask) < val){
      return 0;
    } else{
      return 1;
    }
  }

//This finds the leaf in the search structure that p is located in
node* find_leaf(point p, node* T, box b, double Delta){ //takes in a point since interleave_bits() takes in a point
  //first, we use code copied over from oct_tree to go from a point to an interleave integer
  node* current = T;
  size_t searchInt = o_tree::interleave_bits(p, b.first, Delta); //calling interleave_bits from oct_tree
  //then, we use this interleave integer to find the correct leaf
  while (not (current->is_leaf())){
    if(lookup_bit(searchInt, current -> bit) == 0){ 
      current = current->Right(); 
    } else{
      current = current->Left();
    }
  };
  return current;
}

//this instantiates the knn search structure and then calls the function k_nearest_fromLeaf
void k_nearest_leaf(vtx* p, node* T, int k) { 
  kNN nn(p, k); 
  nn.k_nearest_fromLeaf(T);
  if (report_stats) p->counter = nn.internal_cnt;
  for (int i=0; i < k; i++)
    p->ngh[i] = nn[i];
}

  void k_nearest(vtx* p, int k) {
    kNN nn(p,k);
    nn.k_nearest_rec(tree.get()); //this is passing in a pointer to the o_tree root
    if (report_stats){ p->counter = nn.internal_cnt; p->counter2 = nn.leaf_cnt;}
    for (int i=0; i < k; i++)
      p->ngh[i] = nn[i];
  }

 
  parlay::sequence<vtx*> z_sort(parlay::sequence<vtx*> v, box b, double Delta){ 
    using indexed_point = typename o_tree::indexed_point; 
    size_t n = v.size();
    parlay::sequence<indexed_point> points;
    points = parlay::sequence<indexed_point>(n);
    parlay::parallel_for(0, n, [&] (size_t i){
      size_t p1 = o_tree::interleave_bits(v[i]->pt, b.first, Delta);
      indexed_point i1 = std::make_pair(p1, v[i]);
      points[i] = i1; 
    });
    auto less = [&] (indexed_point a, indexed_point b){
      return a.first < b.first;
    };
    auto x = parlay::sort(points, less);
    parlay::sequence<vtx*> v3; 
    v3 = parlay::sequence<vtx*>(n);
    parlay::parallel_for(0, n, [&] (size_t i){
      v3[i] = x[i].second; 
    });
    return v3; 
  }

    using indexed_point = typename o_tree::indexed_point; 

  indexed_point get_point(node* T){
    if (T->is_leaf()){
      return (T->indexed_pts)[0];
    } else{
      return get_point(T->Left());
    }
  }

  void batch_insert0(slice_t idpts, node* T, int bit){
    if (idpts.size()==0) return;
    T->set_flag(true);
    if(T->is_leaf()){
      if(T->size() + idpts.size() < o_tree::node_cutoff || T->bit == 0){
        // std::cout << "batch update" << std::endl;
        T->batch_update(idpts); 
        // std::cout << "batch update done" << std::endl;
      } else{    
        // std::cout << "batch split" << std::endl;
        o_tree::batch_split(idpts, T, bit);
        // std::cout << "batch split done" << std::endl;
      }
    } else{
        // std::cout << "internal" << std::endl;
        //reset size of parent, and cut points based on bit of T
        // int new_bit = T->bit; 
        size_t val = ((size_t) 1) << (bit - 1);
        size_t mask = (bit == 64) ? ~((size_t) 0) : ~(~((size_t) 0) << bit);
        auto less = [&] (indexed_point x) {
          return (x.first & mask) < val;
        };
        int cut_point = parlay::internal::binary_search(idpts, less);
        // std::cout << "internal sorted" << std::endl;
        if(bit > T->bit){ // T cannot be root due to enforced condition in batch_insert()
          std::cout << "create new" << std::endl;
          indexed_point sample = get_point(T);
          int sample_pos = lookup_bit(sample.first, bit);
          if(sample_pos == 0){ // T and descendants are in left half
            o_tree::create_new(T, idpts.cut(cut_point, idpts.size()), bit, false);
            batch_insert0(idpts.cut(0, cut_point), T, bit-1);
          }else{ // T and descendants are in right half
            o_tree::create_new(T, idpts.cut(0, cut_point), bit, true);
            batch_insert0(idpts.cut(cut_point, idpts.size()), T, bit-1);
          }
        } else{
          // std::cout << "standard recursion" << std::endl;
          T->set_size(T->size()+idpts.size());
          parlay::par_do_if(idpts.size() > 100,
            [&] () {batch_insert0(idpts.cut(0, cut_point), T->Left(), bit-1);},
            [&] () {batch_insert0(idpts.cut(cut_point, idpts.size()), T->Right(), bit-1);}
          );
        }
    }
  }

  void batch_insert(parlay::sequence<vtx*> v, node* R, box b, double Delta){
    size_t vsize = v.size();
    //make sure all the points are within the bounding box of the initial tree
    box points_box = o_tree::get_box(v);
    int dims = v[0]->pt.dimension();
    bool ll_bad = false;
    bool ur_bad = false;
    for(int i=0; i<dims; i++){
      if(points_box.first[i] < b.first[i]){
        ll_bad = true;
      }
      if(points_box.second[i] > b.second[i]){
        ur_bad = true;
      }
    }
    if(ll_bad || ur_bad){
      std::cout << "ERROR: points not contained in bounding box of data structure" << std::endl;
      abort();
    }
    parlay::sequence<indexed_point> idpts; 
    idpts = parlay::sequence<indexed_point>(vsize);
    auto points = parlay::tabulate(vsize, [&] (size_t i) -> indexed_point {
      return std::make_pair(o_tree::interleave_bits(v[i]->pt, b.first, Delta), v[i]);
    });
    auto less = [] (indexed_point a, indexed_point b){
      return a.first < b.first; 
    };
    auto x = parlay::sort(points, less);
    std::cout << "sorted" << std::endl;
    batch_insert0(parlay::make_slice(x), R, R->bit);
    std::cout << "inserted" << std::endl;
    box root_box = o_tree::update_boxes(R);
    std::cout << "updated boxes" << std::endl;
  }

  void batch_delete0(slice_t idpts, node* R){
    if(idpts.size()==0) return;
    R->set_flag(true);
    if(R->is_leaf()){
      size_t n = idpts.size();
      if(n == R->size()){
        o_tree::prune(R); //TODO this might leave R's parent with wrong bit
      }else{
        R->batch_remove(idpts);
      }
    } else{
      size_t n = idpts.size();
      if(n == R->size()){
        o_tree::prune(R);
      } else{
        R->set_size(R->size()-n);
        int new_bit = R->bit; 
        size_t val = ((size_t) 1) << (new_bit - 1);
        size_t mask = (new_bit == 64) ? ~((size_t) 0) : ~(~((size_t) 0) << new_bit);
        auto less = [&] (indexed_point x) {
          return (x.first & mask) < val;
        };
        int cut_point = parlay::internal::binary_search(idpts, less);
        parlay::par_do_if(n > 100,
          [&] () {batch_delete0(idpts.cut(0, cut_point), R->Left());},
          [&] () {batch_delete0(idpts.cut(cut_point, n), R->Right());}
        );
      }
    }
  }

  void batch_delete(parlay::sequence<vtx*> v, node* R, box b, double Delta){
    size_t vsize = v.size();
    parlay::sequence<indexed_point> idpts; 
    idpts = parlay::sequence<indexed_point>(vsize);
    auto points = parlay::delayed_seq<indexed_point>(vsize, [&] (size_t i) -> indexed_point {
      return std::make_pair(o_tree::interleave_bits(v[i]->pt, b.first, Delta), v[i]);
    });
    auto less = [] (indexed_point a, indexed_point b){
      return a.first < b.first; 
    };
    auto x = parlay::sort(points, less); 
    // std::cout << "sorted" << std::endl;
    batch_delete0(parlay::make_slice(x), R);
    // std::cout << "deleted" << std::endl;
    o_tree::compress(R);  
    // std::cout << "pruned" << std::endl;
    box root_box = o_tree::update_boxes(R);
    // std::cout << "updated boxes" << std::endl;
  }

}; //this ends the k_nearest_neighbors structure


// ===== range-search helper (adapted from concurrentKNN) =====
template <typename vtx>
struct RNG {
    using point = typename vtx::pointT;
    using o_tree = oct_tree<vtx>;
    using node = typename o_tree::node;

    vtx* vertex;
    parlay::sequence<int> range_candidates;
    double radius;
    double r_sq;
    int dimensions;
    size_t leaf_cnt;
    size_t internal_cnt;

    RNG(vtx* p, double rad) : vertex(p), radius(rad), r_sq(rad*rad),
                              dimensions(p->pt.dimension()),
                              leaf_cnt(0), internal_cnt(0) {}

    parlay::sequence<int> return_answer() { return range_candidates; }

    bool within_radius(node* T) {
        auto box = T->Box();
        bool ok = true;
        for(int i = 0; i < dimensions; i++) {
            ok = ok &&
                (box.first[i] - radius < vertex->pt[i]) &&
                (box.second[i] + radius > vertex->pt[i]);
        }
        return ok;
    }

    bool point_within_radius(vtx* other) {
        return (vertex->pt - other->pt).sqLength() <= r_sq;
    }

    void range_search_rec(node* T) {
        if(!within_radius(T)) return;
        if(report_stats) internal_cnt++;
        if(T->is_leaf()) {
            if(report_stats) leaf_cnt++;
            auto& Vtx = T->indexed_pts;
            for(int i = 0; i < (int)T->size(); i++) {
                if(Vtx[i].second != vertex && point_within_radius(Vtx[i].second))
                    range_candidates.push_back(Vtx[i].second->identifier);
            }
        } else {
            range_search_rec(T->Left());
            range_search_rec(T->Right());
        }
    }
};

template <typename vtx>
parlay::sequence<int> range_search_for(
    typename oct_tree<vtx>::node* root, vtx* p, double rad) {
    RNG<vtx> rn(p, rad);
    rn.range_search_rec(root);
    return rn.return_answer();
}

} // namespace pbbs_octtree_range
