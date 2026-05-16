#include <algorithm>
#include <fstream>
#include <iostream>
#include <numa_utils.h>
#include <omp.h>
#include <sstream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <sched.h>
#include <unistd.h>
#include <cstring>
#ifdef NUMA_USE_LIBNUMA
#include <numa.h>
#else
#include <dirent.h>
#include <sys/syscall.h>
#include <linux/mempolicy.h>
#endif
#endif

namespace numa {

    #ifdef __linux__

    // === Common helpers (shared by both libnuma and syscall implementations) ===

    // Read the first line from a sysfs file.
    static std::string read_sys_file(const std::string& path) {
        std::ifstream f(path);
        std::string line;
        if(f.is_open()) std::getline(f, line);
        return line;
    }

    // Parse "0-1,4,7-9" sibling list to a sorted vector of cpu IDs.
    static std::vector<int> parse_sibling_list(const std::string& s) {
        std::vector<int> out;
        if(s.empty()) return out;
        std::string tok;
        for(size_t i = 0; i <= s.size(); ++i) {
            if(i == s.size() || s[i] == ',') {
                if(!tok.empty()) {
                    auto dash = tok.find('-');
                    try {
                        if(dash == std::string::npos) {
                            out.push_back(std::stoi(tok));
                        } else {
                            int lo = std::stoi(tok.substr(0, dash));
                            int hi = std::stoi(tok.substr(dash + 1));
                            for(int v = lo; v <= hi; ++v) out.push_back(v);
                        }
                    } catch(const std::exception&) {}
                }
                tok.clear();
            } else {
                tok += s[i];
            }
        }
        return out;
    }

    // Filter to physical cores only - one representative per SMT sibling group.
    // libnuma doesn't expose SMT siblings, so this remains sysfs-based in both
    // implementations. When `allowed` is non-empty the representative is the
    // lowest sibling that is also in `allowed`; otherwise the absolute lowest
    // sibling (legacy behaviour). The `allowed`-aware mode matters inside
    // restricted containers (Docker Desktop / WSL2) where the kernel exposes
    // a sparse cpuset like {0,1,3,5,7,...} - picking the absolute first
    // sibling (say cpu 2 of pair {2,3}) would silently drop the whole pair.
    static std::vector<int> filter_physical_cores(
        const std::vector<int>& cpus,
        const std::set<int>& allowed = {}
    ) {
        std::vector<int> physical;
        for(int cpu : cpus) {
            std::string siblings_str = read_sys_file(
                "/sys/devices/system/cpu/cpu" + std::to_string(cpu) + "/topology/thread_siblings_list"
            );
            if(siblings_str.empty()) {
                physical.push_back(cpu);
                continue;
            }
            auto siblings = parse_sibling_list(siblings_str);
            if(siblings.empty()) {
                physical.push_back(cpu);
                continue;
            }
            int rep = siblings.front();
            if(!allowed.empty()) {
                rep = -1;
                for(int s : siblings) {
                    if(allowed.count(s)) {
                        rep = s;
                        break;
                    }
                }
                // No sibling of this group is allowed - entire SMT class
                // unavailable, contributes nothing.
                if(rep < 0) continue;
            }
            if(rep == cpu) physical.push_back(cpu);
        }
        return physical;
    }

    #ifdef NUMA_USE_LIBNUMA

    // === libnuma-based implementation ===

    TopologyInfo discover(const std::set<int>& allowed_cpus) {
        TopologyInfo info;
        if(numa_available() < 0) {
            // libnuma not usable on this kernel - fallback: single node
            info.node_count = 1;
            int n = static_cast<int>(std::thread::hardware_concurrency());
            std::vector<int> all;
            for(int i = 0; i < n; ++i) all.push_back(i);
            info.cores_per_node.push_back(filter_physical_cores(all, allowed_cpus));
            return info;
        }

        info.node_count = numa_num_configured_nodes();
        info.cores_per_node.resize(info.node_count);

        struct bitmask* cpumask = numa_allocate_cpumask();
        int total_cpus = numa_num_configured_cpus();

        for(int nd = 0; nd < info.node_count; ++nd) {
            numa_node_to_cpus(nd, cpumask);
            std::vector<int> all_cpus;
            for(int i = 0; i < total_cpus; ++i) {
                if(numa_bitmask_isbitset(cpumask, static_cast<unsigned int>(i))) {
                    all_cpus.push_back(i);
                }
            }
            info.cores_per_node[nd] = filter_physical_cores(all_cpus, allowed_cpus);
        }
        numa_free_cpumask(cpumask);
        return info;
    }

    #else // !NUMA_USE_LIBNUMA

    // === Syscall-based implementation (fallback) ===

    // Parse a CPU list string like "0-3,8-11" into individual CPU IDs.
    static std::vector<int> parse_cpu_list(const std::string& str) {
        std::vector<int> cpus;
        std::istringstream stream(str);
        std::string token;
        while(std::getline(stream, token, ',')) {
            try {
                auto dash = token.find('-');
                if(dash != std::string::npos) {
                    int lo = std::stoi(token.substr(0, dash));
                    int hi = std::stoi(token.substr(dash + 1));
                    for(int i = lo; i <= hi; ++i) cpus.push_back(i);
                } else {
                    cpus.push_back(std::stoi(token));
                }
            } catch(const std::exception& e) {
                std::cerr << "[NUMA] Warning: failed to parse CPU list token '" << token << "': " << e.what() << "\n";
            }
        }
        return cpus;
    }

    TopologyInfo discover(const std::set<int>& allowed_cpus) {
        TopologyInfo info;
        // Count NUMA nodes from /sys/devices/system/node/
        DIR* dir = opendir("/sys/devices/system/node");
        if(!dir) {
            // Fallback: single node with all cores
            info.node_count = 1;
            int n = static_cast<int>(std::thread::hardware_concurrency());
            std::vector<int> all;
            for(int i = 0; i < n; ++i) all.push_back(i);
            info.cores_per_node.push_back(filter_physical_cores(all, allowed_cpus));
            return info;
        }

        std::vector<int> node_ids;
        struct dirent* entry;
        while((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            if(name.substr(0, 4) == "node" && name.size() > 4) {
                try {
                    node_ids.push_back(std::stoi(name.substr(4)));
                } catch(...) {}
            }
        }
        closedir(dir);
        std::sort(node_ids.begin(), node_ids.end());

        info.node_count = static_cast<int>(node_ids.size());
        info.cores_per_node.resize(info.node_count);

        for(int i = 0; i < info.node_count; ++i) {
            std::string cpulist = read_sys_file(
                "/sys/devices/system/node/node" + std::to_string(node_ids[i]) + "/cpulist"
            );
            auto all_cpus = parse_cpu_list(cpulist);
            info.cores_per_node[i] = filter_physical_cores(all_cpus, allowed_cpus);
        }
        return info;
    }

    #endif // NUMA_USE_LIBNUMA

    #elif defined(_WIN32)

    TopologyInfo discover(const std::set<int>& /*allowed_cpus*/) {
        // Windows: no SMT-sibling filtering (sysfs/libnuma path is Linux-only),
        // allowed_cpus parameter ignored.
        TopologyInfo info;

        DWORD len = 0;
        GetLogicalProcessorInformationEx(RelationNumaNode, nullptr, &len);

        std::vector<char> buffer(len);
        auto* ptr = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data());

        if(!GetLogicalProcessorInformationEx(RelationNumaNode, ptr, &len)) {
            // Fallback: single node
            info.node_count = 1;
            int n = static_cast<int>(std::thread::hardware_concurrency());
            std::vector<int> all;
            for(int i = 0; i < n; ++i) all.push_back(i);
            info.cores_per_node.push_back(all);
            return info;
        }

        // Collect unique node IDs and their masks
        struct NodeData {
            int node_id;
            KAFFINITY mask;
        };
        std::vector<NodeData> nodes;

        DWORD offset = 0;
        while(offset < len) {
            auto* item = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data() + offset);
            if(item->Relationship == RelationNumaNode) {
                NodeData nd;
                nd.node_id = item->NumaNode.NodeNumber;
                nd.mask = item->NumaNode.GroupMask.Mask;
                nodes.push_back(nd);
            }
            offset += item->Size;
        }

        std::sort(
            nodes.begin(),
            nodes.end(),
            [](const NodeData& a, const NodeData& b) {
                return a.node_id < b.node_id;
            }
        );

        info.node_count = static_cast<int>(nodes.size());
        info.cores_per_node.resize(info.node_count);

        for(int i = 0; i < info.node_count; ++i) {
            KAFFINITY mask = nodes[i].mask;
            for(int bit = 0; bit < 64; ++bit) {
                if(mask & (1ULL << bit)) {
                    info.cores_per_node[i].push_back(bit);
                }
            }
        }

        return info;
    }

    #else

    // Unsupported platform stubs
    TopologyInfo discover() {
        TopologyInfo info;
        info.node_count = 1;
        int n = static_cast<int>(std::thread::hardware_concurrency());
        std::vector<int> all;
        for(int i = 0; i < n; ++i) all.push_back(i);
        info.cores_per_node.push_back(all);
        return info;
    }


    #endif

} // namespace numa
