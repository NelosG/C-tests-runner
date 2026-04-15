#include "cpu_isolator.h"
#include "numa_utils.h"

#include <algorithm>
#include <iostream>
#include <numeric>
#include <sstream>

#ifndef _WIN32
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#endif

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::string CpuIsolator::format_cores(const std::vector<int>& cores) {
    if (cores.empty()) return "";
    std::vector<int> sorted = cores;
    std::sort(sorted.begin(), sorted.end());

    std::ostringstream ss;
    size_t i = 0;
    bool first = true;
    while (i < sorted.size()) {
        size_t start = i;
        while (i + 1 < sorted.size() && sorted[i + 1] == sorted[i] + 1) ++i;
        if (!first) ss << ",";
        first = false;
        if (i == start) {
            ss << sorted[start];
        } else {
            ss << sorted[start] << "-" << sorted[i];
        }
        ++i;
    }
    return ss.str();
}

std::vector<int> CpuIsolator::parse_cores(const std::string& str) {
    std::vector<int> result;
    if (str.empty()) return result;
    std::istringstream ss(str);
    std::string token;
    while (std::getline(ss, token, ',')) {
        auto dash = token.find('-');
        if (dash != std::string::npos) {
            int lo = std::stoi(token.substr(0, dash));
            int hi = std::stoi(token.substr(dash + 1));
            for (int i = lo; i <= hi; ++i) result.push_back(i);
        } else {
            result.push_back(std::stoi(token));
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

CpuIsolator::CpuIsolator(Config config) : config_(config) {
    auto topo = numa::discover();
    nodes_ = topo.cores_per_node;
    for (auto& node_cores : nodes_) {
        system_cores_ += static_cast<int>(node_cores.size());
    }

    if (system_cores_ == 0) {
        system_cores_ = 1;
        nodes_.push_back({0});
    }

    // Pool strategy:
    //
    // partition=true:
    //   Reserve core 0 (node 0) for OS. All other cores available for tests.
    //   Allocate() prefers non-OS NUMA nodes, spills to node 0 when needed.
    //
    // partition=false:
    //   All cores available (no exclusivity, OS shares freely).

    // Add all cores from all nodes
    for (auto& node_cores : nodes_) {
        for (int core : node_cores) {
            available_.insert(core);
        }
    }

    if (config_.enable_partition) {
        os_node_ = 0;
        // Reserve core 0 for OS - always on node 0
        available_.erase(0);

        std::cerr << "[cpu_isolator] Partition: reserved core 0 (node 0) for OS, "
                  << available_.size() << " cores available for tests\n";
    } else {
        std::cerr << "[cpu_isolator] No partition: all " << available_.size()
                  << " cores available (shared with OS)\n";
    }

    std::cerr << "[cpu_isolator] System: " << system_cores_ << " cores, "
              << nodes_.size() << " NUMA node(s)\n";

#ifndef _WIN32
    if (config_.enable_partition) {
        setup_cgroup();
    }
#endif
}

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------

CpuIsolator::~CpuIsolator() {
#ifndef _WIN32
    teardown_cgroup();
#endif
}

// ---------------------------------------------------------------------------
// Allocate
// ---------------------------------------------------------------------------

std::string CpuIsolator::allocate(int num_cores) {
    if (num_cores <= 0) return "";

    std::lock_guard<std::mutex> lock(mutex_);

    // Check total available
    if (static_cast<int>(available_.size()) < num_cores) {
        std::cerr << "[cpu_isolator] Warning: requested " << num_cores
                  << " cores but only " << available_.size() << " available\n";
        if (available_.empty()) return "";
        num_cores = static_cast<int>(available_.size());
    }

    // Determine preferred NUMA node:
    // - Explicit numaNode from config takes priority
    // - Multi-node: prefer first non-OS node
    // - Single node: use node 0
    int preferred_node;
    if (config_.numa_node >= 0) {
        preferred_node = config_.numa_node;
    } else if (nodes_.size() > 1) {
        // Pick first non-OS node
        preferred_node = (os_node_ == 0) ? 1 : 0;
    } else {
        preferred_node = 0;
    }
    if (preferred_node >= static_cast<int>(nodes_.size())) {
        throw std::runtime_error(
            "[cpu_isolator] NUMA node " + std::to_string(preferred_node)
            + " does not exist (system has " + std::to_string(nodes_.size()) + " nodes)");
    }

    // Try to allocate all from preferred NUMA node
    std::vector<int> allocated;
    std::vector<int> node_available;
    for (int core : nodes_[preferred_node]) {
        if (available_.count(core)) node_available.push_back(core);
    }

    if (static_cast<int>(node_available.size()) >= num_cores) {
        // All fit on preferred node
        allocated.assign(node_available.begin(), node_available.begin() + num_cores);
    } else {
        // Not enough on preferred node
        if (config_.numa_node >= 0) {
            // Explicit node requested - error
            throw std::runtime_error(
                "[cpu_isolator] Requested " + std::to_string(num_cores)
                + " cores on NUMA node " + std::to_string(preferred_node)
                + " but only " + std::to_string(node_available.size()) + " available");
        }

        // Try to find another single node that can fit all cores.
        // Priority: non-OS nodes first, then OS node.
        int best_node = -1;

        for (int n = 0; n < static_cast<int>(nodes_.size()); ++n) {
            if (n == preferred_node) continue;
            if (config_.enable_partition && n == os_node_) continue;
            int count = 0;
            for (int core : nodes_[n]) {
                if (available_.count(core)) ++count;
            }
            if (count >= num_cores) { best_node = n; break; }
        }

        if (best_node < 0 && config_.enable_partition && os_node_ != preferred_node) {
            int count = 0;
            for (int core : nodes_[os_node_]) {
                if (available_.count(core)) ++count;
            }
            if (count >= num_cores) best_node = os_node_;
        }

        if (best_node >= 0) {
            // Found a single node that fits all - no cross-NUMA penalty
            for (int core : nodes_[best_node]) {
                if (static_cast<int>(allocated.size()) >= num_cores) break;
                if (available_.count(core)) allocated.push_back(core);
            }
        } else {
            // No single node fits - split across nodes (preferred first, then others)
            std::cerr << "[cpu_isolator] Warning: " << num_cores
                      << " cores don't fit on any single NUMA node, spanning multiple nodes\n";

            allocated = node_available;
            int remaining = num_cores - static_cast<int>(allocated.size());

            // Fill from non-OS nodes first (skip preferred - already taken)
            for (int n = 0; n < static_cast<int>(nodes_.size()) && remaining > 0; ++n) {
                if (n == preferred_node) continue;
                if (config_.enable_partition && n == os_node_) continue;
                for (int core : nodes_[n]) {
                    if (remaining <= 0) break;
                    if (available_.count(core)) {
                        allocated.push_back(core);
                        --remaining;
                    }
                }
            }

            // Finally spill to OS node if still needed
            if (remaining > 0 && config_.enable_partition && os_node_ != preferred_node) {
                for (int core : nodes_[os_node_]) {
                    if (remaining <= 0) break;
                    if (available_.count(core)) {
                        allocated.push_back(core);
                        --remaining;
                    }
                }
            }
        }
    }

    // Remove allocated from pool
    for (int core : allocated) {
        available_.erase(core);
    }

    std::sort(allocated.begin(), allocated.end());
    std::string result = format_cores(allocated);

    return result;
}

// ---------------------------------------------------------------------------
// Release
// ---------------------------------------------------------------------------

void CpuIsolator::release(const std::string& cores_str) {
    if (cores_str.empty()) return;
    auto cores = parse_cores(cores_str);

    std::lock_guard<std::mutex> lock(mutex_);
    for (int core : cores) {
        available_.insert(core);
    }
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

int CpuIsolator::system_cores() const {
    return system_cores_;
}

// ---------------------------------------------------------------------------
// Linux cgroup management
// ---------------------------------------------------------------------------

#ifndef _WIN32

static void write_file(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    if (f.is_open()) {
        f << content;
    }
}

void CpuIsolator::setup_cgroup() {
    cgroup_path_ = "/sys/fs/cgroup/test-runner";

    mkdir(cgroup_path_.c_str(), 0755);

    // Write all test-available cores to the cgroup
    std::vector<int> cores(available_.begin(), available_.end());
    write_file(cgroup_path_ + "/cpuset.cpus", format_cores(cores));

    // Determine which NUMA nodes contain our test cores
    std::set<int> core_set(cores.begin(), cores.end());
    std::vector<int> mems;
    for (int n = 0; n < static_cast<int>(nodes_.size()); ++n) {
        for (int c : nodes_[n]) {
            if (core_set.count(c)) {
                mems.push_back(n);
                break;
            }
        }
    }
    write_file(cgroup_path_ + "/cpuset.mems", mems.empty() ? "0" : format_cores(mems));

    // Try partition isolation (may fail on older kernels)
    std::ofstream part(cgroup_path_ + "/cpuset.cpus.partition");
    if (part.is_open()) {
        part << "isolated";
        if (part.fail()) {
            std::cerr << "[cpu_isolator] Warning: failed to set cpuset.cpus.partition=isolated"
                      << " (may require newer kernel or root privileges)\n";
        }
        part.close();
    }
}

void CpuIsolator::teardown_cgroup() {
    if (cgroup_path_.empty()) return;

    std::ofstream part(cgroup_path_ + "/cpuset.cpus.partition");
    if (part.is_open()) {
        part << "member";
        part.close();
    }

    if (rmdir(cgroup_path_.c_str()) != 0) {
        // Silently ignore - may still have processes
    }
    cgroup_path_.clear();
}

#endif
