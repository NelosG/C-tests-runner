// Both MIS variants in a single TU (parlay ODR / thread_local).
//
// Parlay-native: the graph is built by the runner outside the timed region
// and passed by const reference. The student function is just the pbbs
// algorithm body - no graph construction or narrowing inside RUNNER_EXECUTE.

#include <mis.h>
#include <pbbs_incremental_mis.h>
#include <pbbs_nd_mis.h>

namespace student {

    parlay::sequence<char> nd_mis(const graph<unsigned int, unsigned int>& G) {
        return pbbs_nd_mis::maximalIndependentSet(G);
    }

    parlay::sequence<char> incremental_mis(const graph<unsigned int, unsigned int>& G) {
        return pbbs_incremental_mis::maximalIndependentSet(G);
    }

} // namespace student
