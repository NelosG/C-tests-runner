// hwloc-based implementation of numa:: API. Activated by NUMA_USE_HWLOC,
// set when CMake finds libhwloc. Replaces the legacy numa_utils.cpp on
// platforms where hwloc is available (Linux, Windows, macOS).
//
// What hwloc gives us that legacy libnuma + sysfs / Win32 GetLogicalProcessorInformationEx
// couldn't:
//   - Single code path for Linux / Windows / macOS / inside Docker
//   - Real SMT-physical detection on every OS (Win32 branch previously
//     returned all logical CPUs)
//   - L3 cache grouping (cores_per_l3) - lets CpuIsolator pick co-located
//     cores for cache locality before spilling across L3 / NUMA / spanning
//   - Built-in honoring of cgroup cpuset / sched_getaffinity via
//     hwloc_topology_get_allowed_cpuset()
//
// Implements only numa::discover(). The other entry points
// (pin_to_node / set_memory_policy / reset / pin_omp_threads /
// reset_omp_threads) are reserved-API stubs - currently no one calls them
// (see project_reserved_dead_code memory). If a caller appears, they can
// be wired to hwloc_set_cpubind / hwloc_set_membind one-to-one.

#include <numa_utils.h>

#include <iostream>
#include <hwloc.h>

namespace numa {

    namespace {

        struct HwlocTopology {
            hwloc_topology_t topo = nullptr;
            HwlocTopology() {
                if(hwloc_topology_init(&topo) != 0) { topo = nullptr; return; }
                // Don't restrict topology to allowed cpuset - we want to see
                // the whole hardware tree and filter ourselves. This keeps
                // L3/NUMA structure visible even when allowed_cpuset is sparse.
                if(hwloc_topology_load(topo) != 0) {
                    hwloc_topology_destroy(topo);
                    topo = nullptr;
                }
            }
            ~HwlocTopology() { if(topo) hwloc_topology_destroy(topo); }
            HwlocTopology(const HwlocTopology&) = delete;
            HwlocTopology& operator=(const HwlocTopology&) = delete;
        };

        // Pick the lowest PU index inside `core->cpuset` that is also in
        // `allowed`. Returns -1 if the entire SMT class is disallowed
        // (e.g. cgroup cpuset doesn't permit any of its siblings).
        int physical_representative(hwloc_obj_t core, hwloc_const_cpuset_t allowed) {
            hwloc_bitmap_t inter = hwloc_bitmap_alloc();
            hwloc_bitmap_and(inter, core->cpuset, allowed);
            int rep = hwloc_bitmap_first(inter);
            hwloc_bitmap_free(inter);
            return rep;
        }

        // Iterate every CORE that has any PU inside `parent_cpuset` (assumes
        // hwloc_topology already loaded). Returns physical-representative PU
        // indices that are also in `allowed`.
        std::vector<int> cores_in(hwloc_topology_t t,
                                  hwloc_const_cpuset_t parent_cpuset,
                                  hwloc_const_cpuset_t allowed) {
            std::vector<int> out;
            hwloc_obj_t core = nullptr;
            while((core = hwloc_get_next_obj_inside_cpuset_by_type(
                    t, parent_cpuset, HWLOC_OBJ_CORE, core)) != nullptr) {
                int rep = physical_representative(core, allowed);
                if(rep >= 0) out.push_back(rep);
            }
            return out;
        }

    } // namespace

    TopologyInfo discover(const std::set<int>& allowed_cpus) {
        TopologyInfo info;
        HwlocTopology h;
        if(!h.topo) {
            std::cerr << "[NUMA] hwloc_topology_init/load failed - empty topology\n";
            return info;
        }

        // Build the effective allowed cpuset. Caller may pass an explicit
        // set (e.g. sched_getaffinity result from CpuIsolator); otherwise
        // we use hwloc's own view of what's allowed (which already honors
        // cgroup cpuset, taskset, isolcpus, namespace limits).
        hwloc_bitmap_t allowed = hwloc_bitmap_alloc();
        if(allowed_cpus.empty()) {
            hwloc_const_cpuset_t hw_allowed = hwloc_topology_get_allowed_cpuset(h.topo);
            hwloc_bitmap_copy(allowed, hw_allowed);
            // Some platforms (Windows, restricted containers) return an empty
            // bitmap here - degrade to "everything in the topology".
            if(hwloc_bitmap_iszero(allowed)) {
                hwloc_bitmap_copy(allowed, hwloc_topology_get_topology_cpuset(h.topo));
            }
        } else {
            for(int c : allowed_cpus) hwloc_bitmap_set(allowed, c);
        }

        // NUMA grouping ------------------------------------------------------
        int n_nodes = hwloc_get_nbobjs_by_type(h.topo, HWLOC_OBJ_NUMANODE);
        if(n_nodes <= 0) {
            // Single-NUMA / non-NUMA machine - treat the whole machine as one node.
            hwloc_obj_t root = hwloc_get_root_obj(h.topo);
            info.node_count = 1;
            info.cores_per_node.push_back(cores_in(h.topo, root->cpuset, allowed));
        } else {
            info.node_count = n_nodes;
            info.cores_per_node.resize(n_nodes);
            for(int i = 0; i < n_nodes; ++i) {
                hwloc_obj_t node = hwloc_get_obj_by_type(h.topo, HWLOC_OBJ_NUMANODE, i);
                info.cores_per_node[i] = cores_in(h.topo, node->cpuset, allowed);
            }
        }

        // L3 grouping (bonus over libnuma/sysfs) -----------------------------
        int n_l3 = hwloc_get_nbobjs_by_type(h.topo, HWLOC_OBJ_L3CACHE);
        if(n_l3 > 0) {
            info.cores_per_l3.reserve(n_l3);
            for(int i = 0; i < n_l3; ++i) {
                hwloc_obj_t l3 = hwloc_get_obj_by_type(h.topo, HWLOC_OBJ_L3CACHE, i);
                auto cores = cores_in(h.topo, l3->cpuset, allowed);
                if(!cores.empty()) info.cores_per_l3.push_back(std::move(cores));
            }
        }

        hwloc_bitmap_free(allowed);
        return info;
    }

    // ---- Reserved-API stubs: not currently called, see memory/project_reserved_dead_code.md ----
    bool pin_to_node(int)         { return false; }
    bool set_memory_policy(int)   { return false; }
    void reset()                  {}
    bool pin_omp_threads(int)     { return false; }
    void reset_omp_threads()      {}

} // namespace numa
