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

    // Filter to physical cores only (first thread per sibling group).
    // libnuma doesn't provide physical vs logical core distinction, so this
    // remains sysfs-based in both implementations.
    static std::vector<int> filter_physical_cores(const std::vector<int>& cpus) {
        std::vector<int> physical;
        for(int cpu : cpus) {
            std::string siblings = read_sys_file(
                "/sys/devices/system/cpu/cpu" + std::to_string(cpu) + "/topology/thread_siblings_list"
            );
            if(siblings.empty()) {
                physical.push_back(cpu);
                continue;
            }
            // First CPU in the siblings list is the physical core representative.
            auto first_end = siblings.find_first_of(",-");
            try {
                int first = std::stoi(siblings.substr(0, first_end));
                if(first == cpu) {
                    physical.push_back(cpu);
                }
            } catch(const std::exception&) {
                physical.push_back(cpu); // on parse failure, include the core
            }
        }
        return physical;
    }

    #ifdef NUMA_USE_LIBNUMA

    // === libnuma-based implementation ===

    TopologyInfo discover() {
        TopologyInfo info;
        if(numa_available() < 0) {
            // libnuma not usable on this kernel - fallback: single node
            info.node_count = 1;
            int n = static_cast<int>(std::thread::hardware_concurrency());
            std::vector<int> all;
            for(int i = 0; i < n; ++i) all.push_back(i);
            info.cores_per_node.push_back(filter_physical_cores(all));
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
            info.cores_per_node[nd] = filter_physical_cores(all_cpus);
        }
        numa_free_cpumask(cpumask);
        return info;
    }

    bool pin_to_node(int node) {
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

    bool set_memory_policy(int node) {
        struct bitmask* nodemask = numa_allocate_nodemask();
        numa_bitmask_setbit(nodemask, static_cast<unsigned int>(node));
        numa_set_membind(nodemask);
        numa_free_nodemask(nodemask);
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

        // Reset memory policy via libnuma
        numa_set_localalloc();
        std::cout << "[NUMA] Affinity and memory policy reset\n";
    }

    bool pin_omp_threads(int node) {
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

        // omp_set_dynamic(0): ensure the runtime creates exactly omp_get_max_threads() threads.
        // Affinity persists across omp_set_num_threads() changes because OpenMP runtimes
        // reuse OS threads from a persistent pool - sleeping workers retain their affinity mask.
        // Caller must call omp_set_num_threads(max) before this function.
        bool all_ok = true;
        omp_set_dynamic(0);
        #pragma omp parallel default(none) shared(all_ok, mask)
        {
            if(sched_setaffinity(0, sizeof(mask), &mask) != 0) {
                #pragma omp atomic write
                all_ok = false;
            }
        }

        if(all_ok) {
            std::cout << "[NUMA] Pinned OMP threads to node " << node << " cores:";
            for(int c : cores) std::cout << " " << c;
            std::cout << " (" << omp_get_max_threads() << " threads)\n";
        } else {
            std::cerr << "[NUMA] Some OMP threads failed to pin to node " << node << "\n";
        }
        return all_ok;
    }

    void reset_omp_threads() {
        int n = static_cast<int>(std::thread::hardware_concurrency());
        cpu_set_t mask;
        CPU_ZERO(&mask);
        for(int i = 0; i < n; ++i) CPU_SET(i, &mask);

        // omp_set_dynamic(0): ensure all pool threads participate in the reset.
        omp_set_dynamic(0);
        #pragma omp parallel default(none) shared(mask)
        {
            sched_setaffinity(0, sizeof(mask), &mask);
        }

        // Reset memory policy via libnuma
        numa_set_localalloc();
        std::cout << "[NUMA] OMP thread affinity and memory policy reset\n";
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
            info.cores_per_node.push_back(filter_physical_cores(all));
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
            info.cores_per_node[i] = filter_physical_cores(all_cpus);
        }
        return info;
    }

    bool pin_to_node(int node) {
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

    bool set_memory_policy(int node) {
        constexpr int max_node = sizeof(unsigned long) * 8 - 1;
        if(node < 0 || node > max_node) {
            std::cerr << "[NUMA] setMemoryPolicy: node " << node
                      << " out of range (max " << max_node << ")\n";
            return false;
        }
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

    bool pin_omp_threads(int node) {
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

        // omp_set_dynamic(0): ensure the runtime creates exactly omp_get_max_threads() threads.
        // Affinity persists across omp_set_num_threads() changes because OpenMP runtimes
        // reuse OS threads from a persistent pool - sleeping workers retain their affinity mask.
        // Caller must call omp_set_num_threads(max) before this function.
        bool all_ok = true;
        omp_set_dynamic(0);
        #pragma omp parallel default(none) shared(all_ok, mask)
        {
            if(sched_setaffinity(0, sizeof(mask), &mask) != 0) {
                #pragma omp atomic write
                all_ok = false;
            }
        }

        if(all_ok) {
            std::cout << "[NUMA] Pinned OMP threads to node " << node << " cores:";
            for(int c : cores) std::cout << " " << c;
            std::cout << " (" << omp_get_max_threads() << " threads)\n";
        } else {
            std::cerr << "[NUMA] Some OMP threads failed to pin to node " << node << "\n";
        }
        return all_ok;
    }

    void reset_omp_threads() {
        int n = static_cast<int>(std::thread::hardware_concurrency());
        cpu_set_t mask;
        CPU_ZERO(&mask);
        for(int i = 0; i < n; ++i) CPU_SET(i, &mask);

        // omp_set_dynamic(0): ensure all pool threads participate in the reset.
        omp_set_dynamic(0);
        #pragma omp parallel default(none) shared(mask)
        {
            sched_setaffinity(0, sizeof(mask), &mask);
        }

        // Reset memory policy to default
        syscall(SYS_set_mempolicy, MPOL_DEFAULT, nullptr, 0);
        std::cout << "[NUMA] OMP thread affinity and memory policy reset\n";
    }

    #endif // NUMA_USE_LIBNUMA

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

    bool pin_to_node(const int node) {
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

    bool set_memory_policy(int /*node*/) {
        // Best-effort on Windows - VirtualAllocExNuma could be used per-allocation.
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

    bool pin_omp_threads(const int node) {
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

        // omp_set_dynamic(0): ensure the runtime creates exactly omp_get_max_threads() threads.
        // Affinity persists across omp_set_num_threads() changes because OpenMP runtimes
        // reuse OS threads from a persistent pool - sleeping workers retain their affinity mask.
        // Caller must call omp_set_num_threads(max) before this function.
        bool all_ok = true;
        omp_set_dynamic(0);
        #pragma omp parallel default(none) shared(all_ok, mask)
        {
            if(!SetThreadAffinityMask(GetCurrentThread(), mask)) {
                #pragma omp atomic write
                all_ok = false;
            }
        }

        if(all_ok) {
            std::cout << "[NUMA] Pinned OMP threads to node " << node << " cores:";
            for(int c : cores) std::cout << " " << c;
            std::cout << " (" << omp_get_max_threads() << " threads)\n";
        } else {
            std::cerr << "[NUMA] Some OMP threads failed to pin to node " << node << "\n";
        }
        return all_ok;
    }

    void reset_omp_threads() {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        DWORD_PTR mask = (si.dwNumberOfProcessors >= 64)
            ? ~static_cast<DWORD_PTR>(0)
            : (static_cast<DWORD_PTR>(1) << si.dwNumberOfProcessors) - 1;

        // omp_set_dynamic(0): ensure all pool threads participate in the reset.
        omp_set_dynamic(0);
        #pragma omp parallel default(none) shared(mask)
        {
            SetThreadAffinityMask(GetCurrentThread(), mask);
        }
        std::cout << "[NUMA] OMP thread affinity reset\n";
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

    bool pin_to_node(int) { return false; }
    bool set_memory_policy(int) { return false; }
    void reset() {}
    bool pin_omp_threads(int) { return false; }
    void reset_omp_threads() {}

    #endif

} // namespace numa
