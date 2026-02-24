#pragma once

/**
 * @file numa_utils.h
 * @brief NUMA topology discovery and CPU/memory affinity management.
 *
 * Provides cross-platform utilities for discovering NUMA topology,
 * pinning threads to a specific NUMA node's physical cores, and
 * setting memory allocation policy to prefer local memory.
 *
 * - Linux: sched_setaffinity(), /sys/devices/system/node/ topology,
 *          set_mempolicy() for memory binding.
 * - Windows: GetLogicalProcessorInformationEx(), SetProcessAffinityMask().
 *            NUMA memory binding is best-effort on Windows.
 */

#include <vector>


namespace numa {

    /**
 * @brief Describes the system's NUMA topology.
 */
    struct TopologyInfo {
        int node_count = 0;                             ///< Number of NUMA nodes.
        std::vector<std::vector<int>> cores_per_node;   ///< Physical core IDs per NUMA node.
    };

    /**
 * @brief Discover the system's NUMA topology.
 * @return TopologyInfo with node count and cores-per-node mapping.
 *
 * On single-socket systems, returns one node containing all cores.
 */
    TopologyInfo discover();

    /**
 * @brief Pin the current process/thread to cores of the given NUMA node.
 * @param node NUMA node index (0-based).
 * @return True if affinity was set successfully.
 */
    bool pinToNode(int node);

    /**
 * @brief Set memory allocation policy to prefer the given NUMA node.
 * @param node NUMA node index.
 * @return True if policy was set (Linux), always true on Windows (best-effort).
 */
    bool setMemoryPolicy(int node);

    /**
 * @brief Reset CPU affinity and memory policy to system defaults.
 */
    void reset();

} // namespace numa