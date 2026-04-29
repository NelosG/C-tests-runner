#pragma once

/**
 * @file cpu_isolator.h
 * @brief Dynamic CPU core allocation for sandboxed test execution.
 *
 * On startup, intersects NUMA topology (numa::discover()) with the kernel's
 * sched_getaffinity() set - only cores the kernel actually permits this
 * process to use (cgroup cpuset, taskset, isolcpus, namespace restrictions).
 * The lowest-indexed `infra_reserve` CPUs are then excluded from the test
 * pool so the engine itself (server thread, adapter event loops, git/cmake
 * subprocesses) doesn't fight hot tests for cycles.
 *
 * allocate(N) tries successively wider scopes to minimise cache penalty:
 *   1. single L3 cache (best cache locality)
 *   2. single NUMA node (no cross-NUMA memory penalty)
 *   3. span multiple NUMA nodes - logged as a warning
 * L3 grouping is only available when the topology source provides it
 * (hwloc). With libnuma / sysfs / Win32 fallbacks step 1 is skipped.
 *
 * Identical behaviour on bare-metal Linux, inside Docker (cgroup-limited
 * cpuset), and Windows (sched_getaffinity step is no-op there).
 */

#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

class CpuIsolator {
    public:
        struct Config {
            // CPUs reserved for engine infrastructure (server thread, adapter
            // event loops, git fetch, cmake build, plugin load). Excluded from
            // the test pool - taken from the lowest-indexed kernel-allowed CPUs.
            int infra_reserve = 1;

            // Pin tests to this NUMA node (-1 = auto: pick the node with the
            // most currently-free test cores). Useful on multi-socket HPC
            // servers; on single-socket systems it's effectively a no-op.
            // allocate() throws if an explicit node can't fit the request.
            int numa_node = -1;
        };

        /// Allocation result (currently unused - allocate() returns string).
        struct Allocation {
            std::vector<int> cores;
            int numa_node = -1;
        };

        explicit CpuIsolator(Config config);

        /// Allocate N cores. Returns cores as "0-3,5" string for taskset.
        /// Returns empty string if no cores available at all.
        std::string allocate(int num_cores);

        /// Release previously allocated cores back to the test pool.
        void release(const std::string& cores_str);

        /// Total system cores (sum across NUMA nodes - before sched_getaffinity filter).
        int system_cores() const;

    private:
        Config config_;
        std::mutex mutex_;
        std::set<int> available_;             // free cores in the test pool
        std::vector<std::vector<int>> nodes_; // cores per NUMA node, after kernel-allow filter + infra reserve
        std::vector<std::vector<int>> l3_;    // cores per L3 cache (empty if topology source has no cache info)
        int system_cores_ = 0;

        static std::string format_cores(const std::vector<int>& cores);
        static std::vector<int> parse_cores(const std::string& str);
};
