#pragma once

/**
 * @file numa_utils.h
 * @brief NUMA topology discovery and CPU/memory affinity management.
 *
 * Provides cross-platform utilities for discovering NUMA topology,
 * pinning threads to a specific NUMA node's physical cores, and
 * setting memory allocation policy to prefer local memory.
 *
 * - Linux: libnuma (default, CMake USE_LIBNUMA=ON) or fallback to
 *          sched_setaffinity() / set_mempolicy() with sysfs parsing.
 * - Windows: GetLogicalProcessorInformationEx(), SetProcessAffinityMask().
 *            NUMA memory binding is best-effort on Windows.
 */

#include <set>
#include <vector>


namespace numa {

    /**
 * @brief Describes the system's NUMA topology.
 */
    struct TopologyInfo {
        int node_count = 0;                              ///< Number of NUMA nodes.
        std::vector<std::vector<int>> cores_per_node;    ///< Physical core IDs per NUMA node.
        /// Physical core IDs per L3 cache (empty when topology source
        /// doesn't expose cache levels - sysfs/libnuma fallback). When
        /// populated, CpuIsolator prefers picking cores from a single L3
        /// to keep cache traffic local; falls back to single-NUMA, then
        /// spanning. Each entry is a subset of some cores_per_node[k].
        std::vector<std::vector<int>> cores_per_l3;
    };

    /**
 * @brief Discover the system's NUMA topology.
 * @param allowed_cpus Optional kernel-allowed cpuset (from sched_getaffinity).
 *                     When provided, SMT representatives are chosen as the
 *                     lowest sibling that is also in this set - so an SMT
 *                     pair (2,3) where the kernel only allows cpu 3 yields 3
 *                     instead of being dropped. Empty / nullptr = legacy
 *                     behaviour (pick the absolute lowest sibling).
 * @return TopologyInfo with node count and cores-per-node mapping.
 *
 * On single-socket systems, returns one node containing all cores.
 */
    TopologyInfo discover(const std::set<int>& allowed_cpus = {});

    /**
 * @brief Pin the current process/thread to cores of the given NUMA node.
 * @param node NUMA node index (0-based).
 * @return True if affinity was set successfully.
 */
    bool pin_to_node(int node);

    /**
 * @brief Set memory allocation policy to prefer the given NUMA node.
 * @param node NUMA node index.
 * @return True if policy was set (Linux), always true on Windows (best-effort).
 */
    bool set_memory_policy(int node);

    /**
 * @brief Reset CPU affinity and memory policy to system defaults.
 */
    void reset();

    /**
 * @brief Pin all OMP threads (master + workers) to the given NUMA node.
 *
 * Uses #pragma omp parallel to reach every thread in the pool and sets
 * per-thread affinity to the node's physical cores.
 * Caller must set omp_set_num_threads() before calling.
 *
 * @param node NUMA node index (0-based).
 * @return True if affinity was set on all threads successfully.
 */
    bool pin_omp_threads(int node);

    /**
 * @brief Reset affinity for all OMP threads to system defaults.
 *
 * Uses #pragma omp parallel to reach every thread and sets each one's
 * affinity mask to all available CPUs.
 * Caller must ensure omp_set_num_threads() covers all pool threads.
 */
    void reset_omp_threads();

} // namespace numa