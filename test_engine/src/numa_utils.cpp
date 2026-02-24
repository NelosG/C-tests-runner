#include <algorithm>
#include <fstream>
#include <iostream>
#include <numa_utils.h>
#include <sstream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <sched.h>
#include <unistd.h>
#include <dirent.h>
#include <cstring>
#include <sys/syscall.h>
#include <linux/mempolicy.h>
#endif

namespace numa {

    #ifdef __linux__

    // Parse a CPU list string like "0-3,8-11" into individual CPU IDs.
    static std::vector<int> parseCpuList(const std::string& str) {
        std::vector<int> cpus;
        std::istringstream stream(str);
        std::string token;
        while(std::getline(stream, token, ',')) {
            auto dash = token.find('-');
            if(dash != std::string::npos) {
                int lo = std::stoi(token.substr(0, dash));
                int hi = std::stoi(token.substr(dash + 1));
                for(int i = lo; i <= hi; ++i) cpus.push_back(i);
            } else {
                cpus.push_back(std::stoi(token));
            }
        }
        return cpus;
    }

    // Read the first line from a sysfs file.
    static std::string readSysFile(const std::string& path) {
        std::ifstream f(path);
        std::string line;
        if(f.is_open()) std::getline(f, line);
        return line;
    }

    // Filter to physical cores only (first thread per sibling group).
    static std::vector<int> filterPhysicalCores(const std::vector<int>& cpus) {
        std::vector<int> physical;
        for(int cpu : cpus) {
            std::string siblings = readSysFile(
                "/sys/devices/system/cpu/cpu" + std::to_string(cpu) + "/topology/thread_siblings_list"
            );
            if(siblings.empty()) {
                physical.push_back(cpu);
                continue;
            }
            // First CPU in the siblings list is the physical core representative.
            auto first_end = siblings.find_first_of(",-");
            int first = std::stoi(siblings.substr(0, first_end));
            if(first == cpu) {
                physical.push_back(cpu);
            }
        }
        return physical;
    }

    TopologyInfo discover() {
        TopologyInfo info;
        // Count NUMA nodes from /sys/devices/system/node/
        DIR* dir = opendir("/sys/devices/system/node");
        if(!dir) {
            // Fallback: single node with all cores
            info.node_count = 1;
            int n = static_cast<int>(std::thread::hardware_concurrency());
            std::vector<int> all;
            for(int i = 0; i < n; ++i) all.push_back(i);
            info.cores_per_node.push_back(filterPhysicalCores(all));
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
            std::string cpulist = readSysFile(
                "/sys/devices/system/node/node" + std::to_string(node_ids[i]) + "/cpulist"
            );
            auto all_cpus = parseCpuList(cpulist);
            info.cores_per_node[i] = filterPhysicalCores(all_cpus);
        }
        return info;
    }

    bool pinToNode(int node) {
        auto topo = discover();
        if(node < 0 || node >= topo.node_count) {
            std::cerr << "[NUMA] Invalid node " << node << ", have " << topo.node_count << " nodes\n";
            return false;
        }

        const auto& cores = topo.cores_per_node[node];
        cpu_set_t mask;
        CPU_ZERO(&mask);
        for(int c : cores) {
            CPU_SET(c, &mask);
        }

        if(sched_setaffinity(0, sizeof(mask), &mask) != 0) {
            std::cerr << "[NUMA] sched_setaffinity failed: " << strerror(errno) << "\n";
            return false;
        }

        std::cout << "[NUMA] Pinned to node " << node << " cores:";
        for(int c : cores) std::cout << " " << c;
        std::cout << "\n";
        return true;
    }

    bool setMemoryPolicy(int node) {
        unsigned long nodemask = 1UL << node;
        // set_mempolicy(MPOL_BIND, nodemask, maxnode)
        long ret = syscall(SYS_set_mempolicy, MPOL_BIND, &nodemask, sizeof(nodemask) * 8);
        if(ret != 0) {
            std::cerr << "[NUMA] set_mempolicy failed: " << strerror(errno) << "\n";
            return false;
        }
        std::cout << "[NUMA] Memory policy set to BIND node " << node << "\n";
        return true;
    }

    void reset() {
        // Reset CPU affinity to all CPUs
        int n = static_cast<int>(std::thread::hardware_concurrency());
        cpu_set_t mask;
        CPU_ZERO(&mask);
        for(int i = 0; i < n; ++i) CPU_SET(i, &mask);
        if(sched_setaffinity(0, sizeof(mask), &mask) != 0) {
            std::cerr << "[NUMA] Reset affinity failed: " << strerror(errno) << "\n";
        }

        // Reset memory policy to default
        if(syscall(SYS_set_mempolicy, MPOL_DEFAULT, nullptr, 0) != 0) {
            std::cerr << "[NUMA] Reset memory policy failed: " << strerror(errno) << "\n";
        }
        std::cout << "[NUMA] Affinity and memory policy reset\n";
    }

    #elif defined(_WIN32)

    TopologyInfo discover() {
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

    bool pinToNode(const int node) {
        auto [node_count, cores_per_node] = discover();
        if(node < 0 || node >= node_count) {
            std::cerr << "[NUMA] Invalid node " << node << ", have " << node_count << " nodes\n";
            return false;
        }

        const auto& cores = cores_per_node[node];
        DWORD_PTR mask = 0;
        for(const int c : cores) {
            mask |= (1ULL << c);
        }

        if(!SetThreadAffinityMask(GetCurrentThread(), mask)) {
            std::cerr << "[NUMA] SetThreadAffinityMask failed: " << GetLastError() << "\n";
            return false;
        }

        std::cout << "[NUMA] Pinned to node " << node << " cores:";
        for(int c : cores) std::cout << " " << c;
        std::cout << "\n";
        return true;
    }

    bool setMemoryPolicy(int /*node*/) {
        // Best-effort on Windows — VirtualAllocExNuma could be used per-allocation.
        return true;
    }

    void reset() {
        // Reset to all processors
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        DWORD_PTR mask = (si.dwNumberOfProcessors >= 64)
            ? ~static_cast<DWORD_PTR>(0)
            : (static_cast<DWORD_PTR>(1) << si.dwNumberOfProcessors) - 1;
        if(!SetThreadAffinityMask(GetCurrentThread(), mask)) {
            std::cerr << "[NUMA] Reset affinity failed: " << GetLastError() << "\n";
        }
        std::cout << "[NUMA] Affinity reset\n";
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

    bool pinToNode(int) { return false; }
    bool setMemoryPolicy(int) { return false; }
    void reset() {}

    #endif

} // namespace numa