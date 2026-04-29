#include "cpu_isolator.h"
#include "numa_utils.h"
#include "log_utils.h"

#include <algorithm>
#include <iostream>
#include <sstream>

#ifndef _WIN32
#include <sched.h>
#endif

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::string CpuIsolator::format_cores(const std::vector<int>& cores) {
    if(cores.empty()) return "";
    std::vector<int> sorted = cores;
    std::sort(sorted.begin(), sorted.end());

    std::ostringstream ss;
    size_t i = 0;
    bool first = true;
    while(i < sorted.size()) {
        size_t start = i;
        while(i + 1 < sorted.size() && sorted[i + 1] == sorted[i] + 1) ++i;
        if(!first) ss << ",";
        first = false;
        if(i == start) {
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
    if(str.empty()) return result;
    std::istringstream ss(str);
    std::string token;
    while(std::getline(ss, token, ',')) {
        auto dash = token.find('-');
        if(dash != std::string::npos) {
            int lo = std::stoi(token.substr(0, dash));
            int hi = std::stoi(token.substr(dash + 1));
            for(int i = lo; i <= hi; ++i) result.push_back(i);
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
    // Discover which CPUs the kernel actually permits this process to use.
    // sched_getaffinity reflects all combined constraints (cgroup v1/v2
    // cpuset, isolcpus=, taskset, namespace restrictions). Same single
    // source of truth on bare-metal and inside containers - Docker Desktop
    // / WSL2 typically returns a sparse subset like {0,1,3,5,7,...}.
    // numa::discover() uses this to pick SMT representatives that the
    // kernel will actually accept for sched_setaffinity().
    std::set<int> effective_cores;
    #ifndef _WIN32
    {
        cpu_set_t mask;
        CPU_ZERO(&mask);
        if(sched_getaffinity(0, sizeof(mask), &mask) == 0) {
            for(int i = 0; i < CPU_SETSIZE; ++i) {
                if(CPU_ISSET(i, &mask)) effective_cores.insert(i);
            }
        }
    }
    #endif

    auto topo = numa::discover(effective_cores);
    nodes_ = topo.cores_per_node;
    l3_ = topo.cores_per_l3;
    for(auto& node_cores : nodes_) {
        system_cores_ += static_cast<int>(node_cores.size());
        for(int core : node_cores) {
            available_.insert(core);
        }
    }

    if(system_cores_ == 0) {
        system_cores_ = 1;
        nodes_.push_back({0});
        available_.insert(0);
    }

    if(!effective_cores.empty()) {
        LOG("CpuIsolator") << "Kernel-allowed cpuset: "
            << format_cores(
                std::vector<int>(
                    effective_cores.begin(),
                    effective_cores.end()
                )
            )
            << " (" << effective_cores.size() << " cores)\n";
    }

    // Reserve the lowest-indexed `infra_reserve` cores for engine infrastructure
    // (server, adapters, git/cmake subprocesses). Keeps hot tests off the
    // CPUs that the engine itself competes for, regardless of how many
    // service threads we end up running.
    std::vector<int> infra_cpus;
    for(int taken = 0; taken < config_.infra_reserve && !available_.empty(); ++taken) {
        int low = *available_.begin();
        available_.erase(low);
        infra_cpus.push_back(low);
        for(auto& node_cores : nodes_) {
            node_cores.erase(
                std::remove(node_cores.begin(), node_cores.end(), low),
                node_cores.end()
            );
        }
    }
    if(!infra_cpus.empty()) {
        LOG("CpuIsolator") << "Reserved " << infra_cpus.size()
            << " cpu(s) for infrastructure: " << format_cores(infra_cpus) << "\n";
    }

    // Drop infra-reserved CPUs from L3 groups as well, then drop empty groups
    // so the L3-aware path in allocate() doesn't waste time on them.
    if(!l3_.empty()) {
        std::set<int> infra_set(infra_cpus.begin(), infra_cpus.end());
        for(auto& g : l3_) {
            g.erase(
                std::remove_if(
                    g.begin(),
                    g.end(),
                    [&](int c) { return infra_set.count(c) > 0; }
                ),
                g.end()
            );
        }
        l3_.erase(
            std::remove_if(
                l3_.begin(),
                l3_.end(),
                [](const std::vector<int>& g) { return g.empty(); }
            ),
            l3_.end()
        );
    }

    LOG("CpuIsolator") << "Test pool: " << available_.size()
        << " cores across " << nodes_.size() << " NUMA node(s)";
    if(!l3_.empty()) {
        std::cout << ", " << l3_.size() << " L3 group(s)";
    }
    std::cout << "\n";
}

// ---------------------------------------------------------------------------
// Allocate
// ---------------------------------------------------------------------------

std::string CpuIsolator::allocate(int num_cores) {
    if(num_cores <= 0) return "";

    std::lock_guard<std::mutex> lock(mutex_);

    if(static_cast<int>(available_.size()) < num_cores) {
        LOG_ERR("CpuIsolator") << "Requested " << num_cores
            << " cores but only " << available_.size() << " available\n";
        if(available_.empty()) return "";
        num_cores = static_cast<int>(available_.size());
    }

    // Pick a preferred NUMA node:
    //   - Explicit `numa_node` from config - respect it (throws below if it
    //     can't fit, rather than silently spilling).
    //   - Auto: choose the node with the most currently-free cores - gives
    //     us the best chance of single-node allocation (no cross-NUMA
    //     memory penalty).
    int preferred_node = 0;
    if(config_.numa_node >= 0) {
        preferred_node = config_.numa_node;
        if(preferred_node >= static_cast<int>(nodes_.size())) {
            throw std::runtime_error(
                "[cpu_isolator] NUMA node " + std::to_string(preferred_node)
                + " does not exist (system has " + std::to_string(nodes_.size()) + " nodes)"
            );
        }
    } else {
        int best_count = -1;
        for(int n = 0; n < static_cast<int>(nodes_.size()); ++n) {
            int count = 0;
            for(int core : nodes_[n]) {
                if(available_.count(core)) ++count;
            }
            if(count > best_count) {
                best_count = count;
                preferred_node = n;
            }
        }
    }

    std::vector<int> allocated;

    // Pass 0 (preferred - only when topology source exposes L3 groups, e.g.
    // hwloc): try to pick all `num_cores` from a single L3 cache.
    // This wins us cache locality - threads sharing an L3 see each other's
    // writes through the same last-level cache instead of bouncing through
    // memory. Constraint: the L3 group must belong to the preferred NUMA
    // node (otherwise we'd silently violate an explicit numa_node setting).
    if(!l3_.empty()) {
        const auto& numa_cores = nodes_[preferred_node];
        std::set<int> numa_set(numa_cores.begin(), numa_cores.end());
        for(const auto& l3_group : l3_) {
            // Is this L3 group inside our preferred NUMA node?
            bool inside_numa = !l3_group.empty();
            for(int c : l3_group) {
                if(!numa_set.count(c)) {
                    inside_numa = false;
                    break;
                }
            }
            if(!inside_numa) continue;
            std::vector<int> l3_free;
            for(int c : l3_group) {
                if(available_.count(c)) l3_free.push_back(c);
            }
            if(static_cast<int>(l3_free.size()) >= num_cores) {
                allocated.assign(l3_free.begin(), l3_free.begin() + num_cores);
                break;
            }
        }
    }

    // Pass 1: take all from the preferred NUMA node.
    std::vector<int> node_available;
    for(int core : nodes_[preferred_node]) {
        if(available_.count(core)) node_available.push_back(core);
    }

    if(!allocated.empty()) {
        // already filled by L3 pass - done
    } else if(static_cast<int>(node_available.size()) >= num_cores) {
        allocated.assign(node_available.begin(), node_available.begin() + num_cores);
    } else {
        // Preferred node insufficient.
        if(config_.numa_node >= 0) {
            // User asked for a specific node - respect it, fail rather than spill.
            throw std::runtime_error(
                "[cpu_isolator] Requested " + std::to_string(num_cores)
                + " cores on NUMA node " + std::to_string(preferred_node)
                + " but only " + std::to_string(node_available.size()) + " available"
            );
        }

        // Pass 2: any other single node that fits the whole request?
        int best_node = -1;
        for(int n = 0; n < static_cast<int>(nodes_.size()); ++n) {
            if(n == preferred_node) continue;
            int count = 0;
            for(int core : nodes_[n]) {
                if(available_.count(core)) ++count;
            }
            if(count >= num_cores) {
                best_node = n;
                break;
            }
        }

        if(best_node >= 0) {
            // Single non-preferred node fits - no cross-NUMA penalty.
            for(int core : nodes_[best_node]) {
                if(static_cast<int>(allocated.size()) >= num_cores) break;
                if(available_.count(core)) allocated.push_back(core);
            }
        } else {
            // Pass 3: span multiple nodes. Cross-NUMA memory penalty applies -
            // warn so perf-measurement results are interpreted accordingly.
            LOG_ERR("CpuIsolator") << "Warning: " << num_cores
                << " cores don't fit on any single NUMA node - spanning multiple "
                << "(cross-NUMA memory penalty applies)\n";

            allocated = node_available;
            int remaining = num_cores - static_cast<int>(allocated.size());
            for(int n = 0; n < static_cast<int>(nodes_.size()) && remaining > 0; ++n) {
                if(n == preferred_node) continue;
                for(int core : nodes_[n]) {
                    if(remaining <= 0) break;
                    if(available_.count(core)) {
                        allocated.push_back(core);
                        --remaining;
                    }
                }
            }
        }
    }

    for(int core : allocated) {
        available_.erase(core);
    }

    std::sort(allocated.begin(), allocated.end());
    return format_cores(allocated);
}

// ---------------------------------------------------------------------------
// Release
// ---------------------------------------------------------------------------

void CpuIsolator::release(const std::string& cores_str) {
    if(cores_str.empty()) return;
    auto cores = parse_cores(cores_str);

    std::lock_guard<std::mutex> lock(mutex_);
    for(int core : cores) {
        available_.insert(core);
    }
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

int CpuIsolator::system_cores() const {
    return system_cores_;
}
