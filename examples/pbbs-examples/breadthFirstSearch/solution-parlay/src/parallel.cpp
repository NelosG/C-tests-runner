// Parlay-native BFS. The graph is built by the runner outside the timed
// region and passed by const reference; the student function is just the
// pbbs BFS body (plus a serial fallback for tiny graphs).

#include <cstddef>
#include <vector>

#include <bfs.h>
#include <pbbs_back_forward_bfs.h>
#include <pbbs_deterministic_bfs.h>
#include <pbbs_simple_bfs.h>

namespace {

    // Serial BFS fallback. pbbs's parallel BFS variants hit a
    // parlay-scheduler corner case on tiny graphs (N <= a few hundred)
    // under T>=2 - the worker pool's task stealing races against the
    // per-vertex atomic-parent CAS and segfaults. We run serially for
    // tiny inputs; the parent tree we return is a valid BFS.
    parlay::sequence<int> serial_bfs(
        int start, const graph<int, unsigned int>& G)
    {
        std::size_t n = G.numVertices();
        parlay::sequence<int> parent(n, -1);
        if(n == 0) return parent;
        parent[start] = start;
        std::vector<int> frontier{start};
        while(!frontier.empty()) {
            std::vector<int> next;
            for(int u : frontier) {
                auto vtx = G[static_cast<std::size_t>(u)];
                for(unsigned int j = 0; j < vtx.degree; ++j) {
                    int v = vtx.Neighbors[j];
                    if(parent[v] == -1) {
                        parent[v] = u;
                        next.push_back(v);
                    }
                }
            }
            frontier = std::move(next);
        }
        return parent;
    }

    constexpr std::size_t parallel_bfs_min_n = 1000;

}

namespace student {

    parlay::sequence<int> simple_bfs(int start, const graph<int, unsigned int>& G) {
        if(G.numVertices() < parallel_bfs_min_n) return serial_bfs(start, G);
        return pbbs_simple_bfs::BFS(start, G);
    }

    parlay::sequence<int> deterministic_bfs(int start, const graph<int, unsigned int>& G) {
        if(G.numVertices() < parallel_bfs_min_n) return serial_bfs(start, G);
        return pbbs_deterministic_bfs::BFS(start, G);
    }

    parlay::sequence<int> back_forward_bfs(int start, const graph<int, unsigned int>& G) {
        if(G.numVertices() < parallel_bfs_min_n) return serial_bfs(start, G);
        return pbbs_back_forward_bfs::BFS(start, G);
    }

} // namespace student
