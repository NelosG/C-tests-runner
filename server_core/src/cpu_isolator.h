#pragma once

/**
 * @file cpu_isolator.h
 * @brief Dynamic CPU core allocation for sandboxed test execution.
 *
 * Allocates cores on demand from the system's available cores:
 * - partition=true: reserves core 0 (node 0) for OS, all other cores in test pool
 * - partition=false: all cores available (no reservation)
 * - Prefers non-OS NUMA nodes first, spills to OS node when needed
 * - Warns if requested count exceeds a single NUMA node
 * - Errors if explicit numaNode is set and cores don't fit
 * - Linux: optionally creates cpuset cgroup with partition=isolated
 * - Windows: stores core list; affinity set per-process by SandboxLauncher
 */

#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

class CpuIsolator {
public:
    struct Config {
        int numa_node = -1;               // prefer this NUMA node (-1 = auto, pick node 0)
        bool enable_partition = true;     // Linux: cpuset.cpus.partition=isolated
    };

    /// Allocation result
    struct Allocation {
        std::vector<int> cores;           // allocated core IDs
        int numa_node = -1;               // which NUMA node they belong to (-1 = mixed)
    };

    explicit CpuIsolator(Config config);
    ~CpuIsolator();

    /// Allocate N cores. Returns cores as "0-3" string for taskset.
    /// Throws std::runtime_error if numa_node is explicit and not enough cores.
    /// Returns empty string if no cores available (with warning).
    std::string allocate(int num_cores);

    /// Release previously allocated cores back to pool.
    void release(const std::string& cores_str);

    /// Total system cores (all nodes combined).
    int system_cores() const;

private:
    Config config_;
    std::mutex mutex_;
    std::set<int> available_;             // pool of free cores
    std::vector<std::vector<int>> nodes_; // cores per NUMA node
    int system_cores_ = 0;
    int os_node_ = 0;                    // NUMA node reserved for OS (multi-node only)

    // Format core list as compact range string
    static std::string format_cores(const std::vector<int>& cores);

    // Parse "0-3,5" back to vector
    static std::vector<int> parse_cores(const std::string& str);

#ifndef _WIN32
    void setup_cgroup();
    void teardown_cgroup();
    std::string cgroup_path_;
#endif
};
