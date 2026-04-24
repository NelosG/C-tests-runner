#include "adapter_manager.h"

#include <config_utils.h>
#include <cstring>
#include <dll_plugin_helpers.h>
#include <dll_utils.h>
#include <log_utils.h>
#include <path_utils.h>
#include <resource_manager.h>
#include <test_runner_service.h>
#include <transport_adapter.h>
#include <utility>
#include <vector>

namespace fs = std::filesystem;


namespace {

    /// Allocate a heap-owned C string copy that the caller must `delete[]`.
    /// Used for ManagementAPI list_* callbacks crossing the C ABI boundary.
    const char* dup_alloc(const std::string& s) {
        char* buf = new char[s.size() + 1];
        std::memcpy(buf, s.c_str(), s.size() + 1);
        return buf;
    }

    const char* empty_json_array_alloc() {
        return dup_alloc("[]");
    }

} // namespace

// ============================================================================
// Construction / destruction
// ============================================================================

AdapterManager::AdapterManager(
    TestRunnerService& runner,
    fs::path adapters_dir,
    fs::path config_dir,
    fs::path exe_dir,
    std::string node_id,
    ResourceManager* resource_manager
)
    : runner_(runner),
      adapters_dir_(std::move(adapters_dir)),
      config_dir_(std::move(config_dir)),
      exe_dir_(std::move(exe_dir)),
      node_id_(std::move(node_id)),
      resource_manager_(resource_manager) {
    scan_available_adapters();
    build_management_api();
}

AdapterManager::~AdapterManager() {
    stop_all();
}

// ============================================================================
// Public API (thin lock + delegate)
// ============================================================================

bool AdapterManager::load(const std::string& name, const nlohmann::json& config) {
    std::lock_guard lock(mutex_);
    return load_locked(name, config);
}

bool AdapterManager::unload(const std::string& name) {
    std::lock_guard lock(mutex_);
    return unload_locked(name);
}

nlohmann::json AdapterManager::list() const {
    std::lock_guard lock(mutex_);
    return build_adapter_list_locked();
}

void AdapterManager::rescan() {
    std::lock_guard lock(mutex_);
    scan_available_adapters();
}

// ============================================================================
// Two-phase shutdown - stop all, then destroy + unload outside catch scope
// ============================================================================

void AdapterManager::stop_all() {
    std::lock_guard lock(mutex_);

    struct Cleanup {
        void* handle;
        TransportAdapter* adapter;
        DestroyFn destroy;
    };
    std::vector<Cleanup> to_cleanup;
    to_cleanup.reserve(loaded_.size());

    // Snapshot names BEFORE iterating: adapter->stop() runs untrusted DLL code
    // that might (via management API re-entry on the same thread) call back into
    // unload_adapter and erase from loaded_, which would invalidate a range-for
    // iterator. recursive_mutex permits the re-entry but cannot guard the
    // iterator against mutation. Iterate by name and re-check presence each
    // pass instead.
    std::vector<std::string> names;
    names.reserve(loaded_.size());
    for(auto& [name, _] : loaded_) names.push_back(name);

    for(const auto& name : names) {
        auto it = loaded_.find(name);
        if(it == loaded_.end()) continue; // already removed by re-entrant unload
        auto& ma = it->second;
        LOG("AdapterManager") << "Stopping '" << name << "'\n";
        try {
            ma.adapter->stop();
        } catch(const std::exception& e) {
            LOG_ERR("AdapterManager") << "Error stopping '" << name << "': " << e.what() << "\n";
        } catch(...) {
            LOG_ERR("AdapterManager") << "Unknown error stopping '" << name << "'\n";
        }
        // Re-find: stop() may have triggered re-entrant erase. The DLL handle /
        // destroy fn we recorded earlier could now point to a freed entry, so
        // re-check before snapshotting them for the cleanup phase.
        it = loaded_.find(name);
        if(it == loaded_.end()) continue;
        to_cleanup.push_back({it->second.dll_handle, it->second.adapter, it->second.destroy});
    }
    loaded_.clear();

    for(auto& c : to_cleanup) {
        try { c.destroy(c.adapter); } catch(...) {}
        dll::free(c.handle);
    }
}

// ============================================================================
// Private helpers
// ============================================================================

std::string AdapterManager::rel_path(const fs::path& p) const {
    return path_utils::rel(p, exe_dir_);
}

nlohmann::json AdapterManager::build_adapter_list_locked() const {
    auto arr = nlohmann::json::array();
    for(auto& [name, path] : available_) {
        bool is_loaded = loaded_.count(name) > 0;
        nlohmann::json entry = {
            {"name", name},
            {"status", to_string(is_loaded ? adapter_status::running : adapter_status::available)}
        };
        if(is_loaded) entry["config"] = loaded_.at(name).config;
        arr.push_back(entry);
    }
    return arr;
}

void AdapterManager::scan_available_adapters() {
    available_ = dll_plugin_helpers::scan_plugin_dir(
        adapters_dir_,
        {/*prefix*/"", /*suffix*/"_adapter"},
        "AdapterManager"
    );
}

// ============================================================================
// loadLocked - full DLL lifecycle: open, create, pre-register, start, notify
// ============================================================================

bool AdapterManager::load_locked(const std::string& name, const nlohmann::json& config) {
    if(loaded_.count(name)) {
        LOG_ERR("AdapterManager") << "'" << name << "' is already loaded\n";
        return false;
    }

    nlohmann::json effective_config = dll_plugin_helpers::effective_config(
        config,
        config_dir_,
        name + ".json"
    );

    auto it = available_.find(name);
    if(it == available_.end()) {
        scan_available_adapters();
        it = available_.find(name);
        if(it == available_.end()) {
            LOG_ERR("AdapterManager") << "No DLL found for adapter '" << name << "'\n";
            return false;
        }
    }

    const std::string dll_path = it->second;
    LOG("AdapterManager") << "Loading '" << name << "' from " << rel_path(dll_path) << "\n";

    auto loaded = dll_plugin_helpers::load_and_resolve(
        dll_path,
        "create_adapter",
        "destroy_adapter"
    );
    if(!loaded.handle) {
        LOG_ERR("AdapterManager") << rel_path(dll_path) << ": " << loaded.error << "\n";
        return false;
    }
    void* handle = loaded.handle;
    const auto create_fn = reinterpret_cast<CreateFn>(loaded.create_sym);
    const auto destroy_fn = reinterpret_cast<DestroyFn>(loaded.destroy_sym);

    AdapterContext ctx{effective_config, node_id_, name};
    TransportAdapter* adapter = create_fn(&runner_, &mgmt_api_, &ctx);
    if(!adapter) {
        LOG_ERR("AdapterManager") << "'" << name << "' create_adapter returned nullptr\n";
        dll::free(handle);
        return false;
    }

    // Pre-register so the adapter sees itself in transports during start()
    // and can call update_adapter_config to enrich its stored config.
    loaded_[name] = {handle, adapter, destroy_fn, name, effective_config};

    // Start adapter - destroy_fn / dll::free called OUTSIDE catch block.
    // Inside catch, the exception object's vtable lives in the DLL; calling
    // back into the DLL while exception is active causes ACCESS_VIOLATION.
    bool start_failed = false;
    try {
        adapter->start();
    } catch(const std::exception& e) {
        LOG_ERR("AdapterManager") << "'" << name << "' failed to start: " << e.what() << "\n";
        start_failed = true;
    } catch(...) {
        LOG_ERR("AdapterManager") << "'" << name << "' failed to start (unknown error)\n";
        start_failed = true;
    }
    if(start_failed) {
        loaded_.erase(name);
        try { destroy_fn(adapter); } catch(...) {}
        dll::free(handle);
        return false;
    }
    LOG("AdapterManager") << "'" << name << "' started\n";

    // notifyOnline AFTER start succeeded and adapter is in loaded_ map,
    // so buildTransportsList sees this adapter + all previously loaded.
    bool notify_failed = false;
    try {
        adapter->notify_online();
    } catch(const std::exception& e) {
        LOG_ERR("AdapterManager") << "'" << name << "' online notification failed: " << e.what() << "\n";
        notify_failed = true;
    } catch(...) {
        LOG_ERR("AdapterManager") << "'" << name << "' online notification failed (unknown error)\n";
        notify_failed = true;
    }
    if(notify_failed) {
        loaded_.erase(name);
        try { adapter->stop(); } catch(...) {}
        try { destroy_fn(adapter); } catch(...) {}
        dll::free(handle);
        return false;
    }

    return true;
}

// ============================================================================
// unloadLocked - stop, destroy, unload (mirror of load's exception protocol)
// ============================================================================

bool AdapterManager::unload_locked(const std::string& name) {
    auto it = loaded_.find(name);
    if(it == loaded_.end()) return false;

    LOG("AdapterManager") << "Stopping '" << name << "'\n";

    void* handle_to_free = it->second.dll_handle;
    TransportAdapter* adapter_to_destroy = it->second.adapter;
    const DestroyFn destroy = it->second.destroy;

    try {
        adapter_to_destroy->stop();
    } catch(const std::exception& e) {
        LOG_ERR("AdapterManager") << "Error stopping '" << name << "': " << e.what() << "\n";
    } catch(...) {
        LOG_ERR("AdapterManager") << "Unknown error stopping '" << name << "'\n";
    }
    try { destroy(adapter_to_destroy); } catch(...) {}
    loaded_.erase(it);
    dll::free(handle_to_free);
    LOG("AdapterManager") << "'" << name << "' unloaded\n";
    return true;
}

// ============================================================================
// buildManagementAPI - C-ABI callbacks for adapters to call back into us
// ============================================================================

void AdapterManager::build_management_api() {
    mgmt_api_.context = static_cast<void*>(this);

    mgmt_api_.load_adapter = [](void* ctx, const char* name, const nlohmann::json& config) -> bool {
        auto* self = static_cast<AdapterManager*>(ctx);
        std::lock_guard lock(self->mutex_);
        return self->load_locked(name, config);
    };
    mgmt_api_.unload_adapter = [](void* ctx, const char* name) -> bool {
        auto* self = static_cast<AdapterManager*>(ctx);
        std::lock_guard lock(self->mutex_);
        return self->unload_locked(name);
    };
    mgmt_api_.list_adapters = [](void* ctx) -> const char* {
        auto* self = static_cast<AdapterManager*>(ctx);
        std::lock_guard lock(self->mutex_);
        return dup_alloc(self->build_adapter_list_locked().dump());
    };
    mgmt_api_.free_string = [](void*, const char* str) { delete[] str; };

    mgmt_api_.load_resource_provider = [](
        void* ctx,
        const char* name,
        const nlohmann::json& config
    ) -> bool {
            auto* self = static_cast<AdapterManager*>(ctx);
            return self->resource_manager_ && self->resource_manager_->load(name, config);
        };
    mgmt_api_.unload_resource_provider = [](void* ctx, const char* name) -> bool {
        auto* self = static_cast<AdapterManager*>(ctx);
        return self->resource_manager_ && self->resource_manager_->unload(name);
    };
    mgmt_api_.list_resource_providers = [](void* ctx) -> const char* {
        auto* self = static_cast<AdapterManager*>(ctx);
        if(!self->resource_manager_) return empty_json_array_alloc();
        return self->resource_manager_->list_providers_alloc();
    };
    mgmt_api_.list_available_resource_providers = [](void* ctx) -> const char* {
        auto* self = static_cast<AdapterManager*>(ctx);
        if(!self->resource_manager_) return empty_json_array_alloc();
        return self->resource_manager_->list_available_providers_alloc();
    };

    mgmt_api_.update_adapter_config = [](
        void* ctx,
        const char* name,
        const nlohmann::json& patch
    ) {
            auto* self = static_cast<AdapterManager*>(ctx);
            std::lock_guard lock(self->mutex_);
            auto it = self->loaded_.find(name);
            if(it != self->loaded_.end()) it->second.config.merge_patch(patch);
        };
}
