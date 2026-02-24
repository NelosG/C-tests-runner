#pragma once

/**
 * @file adapter_manager.h
 * @brief Manages dynamically loaded adapter DLLs.
 *
 * Scans the adapters directory for available DLLs, maintains a name → path
 * index, and allows loading/unloading adapters at runtime with arbitrary
 * JSON configs.
 *
 * Provides ManagementAPI C callbacks so that adapters (e.g. HTTP) can
 * expose management endpoints without crossing C++ ABI boundaries.
 */

#include <adapter_api.h>
#include <config_utils.h>
#include <dll_utils.h>
#include <filesystem>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <test_runner_service.h>
#include <transport_adapter.h>
#include <utility>
#include <vector>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

class AdapterManager {
    public:
        AdapterManager(TestRunnerService& runner, fs::path adapters_dir,
                       fs::path config_dir = {}, fs::path exe_dir = {})
            : runner_(runner), adapters_dir_(std::move(adapters_dir)),
              config_dir_(std::move(config_dir)), exe_dir_(std::move(exe_dir)) {
            scanAvailableAdapters();
            buildManagementAPI();
        }

        ~AdapterManager() {
            stopAll();
        }

        /// Get the ManagementAPI struct to pass to adapters.
        const ManagementAPI& managementAPI() const { return mgmt_api_; }

        /// Load, create, and start an adapter by canonical name.
    /// @param name    Adapter name (e.g. "rabbit") — must match adapter_name() from DLL.
    /// @param config  JSON config passed to create_adapter().
    /// @return true on success.
        bool load(const std::string& name, const nlohmann::json& config) {
            std::lock_guard lock(mutex_);
            return loadLocked(name, config);
        }

        /// Stop and unload an adapter by name.
        bool unload(const std::string& name) {
            std::lock_guard lock(mutex_);
            return unloadLocked(name);
        }

        /// List available and loaded adapters.
        nlohmann::json list() const {
            std::lock_guard lock(mutex_);
            return buildAdapterListLocked();
        }

        /// Re-scan the adapters directory for new DLLs.
        void rescan() {
            std::lock_guard lock(mutex_);
            scanAvailableAdapters();
        }

        /// Stop all loaded adapters.
        void stopAll() {
            std::lock_guard lock(mutex_);

            // Two-phase shutdown: first stop() all adapters (may throw DLL
            // exceptions), then destroy + dll::free outside any catch scope.
            struct Cleanup {
                void* handle;
                TransportAdapter* adapter;
                DestroyFn destroy;
            };
            std::vector<Cleanup> to_cleanup;
            to_cleanup.reserve(loaded_.size());

            for(auto& [name, ma] : loaded_) {
                std::cout << "[AdapterManager] Stopping '" << name << "'\n";
                try {
                    ma.adapter->stop();
                } catch(const std::exception& e) {
                    std::cerr << "[AdapterManager] Error stopping '" << name << "': " << e.what() << "\n";
                } catch(...) {
                    std::cerr << "[AdapterManager] Unknown error stopping '" << name << "'\n";
                }
                to_cleanup.push_back({ma.dll_handle, ma.adapter, ma.destroy});
            }
            loaded_.clear();

            // Destroy adapters and unload DLLs — no active exceptions here
            for(auto& c : to_cleanup) {
                try { c.destroy(c.adapter); } catch(...) {}
                dll::free(c.handle);
            }
        }

    private:
        using CreateFn = TransportAdapter* (*)(TestRunnerService*, const ManagementAPI*, const char*);
        using DestroyFn = void (*)(TransportAdapter*);
        using NameFn = const char* (*)();

        struct ManagedAdapter {
            void* dll_handle{};
            TransportAdapter* adapter{};
            DestroyFn destroy{};
            std::string name;
            nlohmann::json config;
        };

        TestRunnerService& runner_;
        fs::path adapters_dir_;
        fs::path config_dir_;
        fs::path exe_dir_;
        ManagementAPI mgmt_api_{};

        /// Display path relative to exe_dir_ with forward slashes.
        std::string relPath(const fs::path& p) const {
            if(exe_dir_.empty() || p.empty()) return p.generic_string();
            try { return fs::proximate(p, exe_dir_).generic_string(); }
            catch(...) { return p.generic_string(); }
        }

        std::map<std::string, std::string> available_;   ///< name → DLL path
        std::map<std::string, ManagedAdapter> loaded_;    ///< name → running adapter
        mutable std::mutex mutex_;

        /// Build JSON array of all adapters — caller must hold mutex_.
        nlohmann::json buildAdapterListLocked() const {
            auto arr = nlohmann::json::array();
            for(auto& [name, path] : available_) {
                bool is_loaded = loaded_.count(name) > 0;
                nlohmann::json entry = {
                    {"name", name},
                    {"dllPath", relPath(path)},
                    {"status", is_loaded ? "running" : "available"}
                };
                if(is_loaded) {
                    entry["config"] = loaded_.at(name).config;
                }
                arr.push_back(entry);
            }
            return arr;
        }

        /// Build the ManagementAPI callback struct pointing to this manager.
        void buildManagementAPI() {
            mgmt_api_.context = static_cast<void*>(this);

            mgmt_api_.load_adapter = [](void* ctx, const char* name, const nlohmann::json& config) -> bool {
                auto* self = static_cast<AdapterManager*>(ctx);
                std::lock_guard lock(self->mutex_);
                return self->loadLocked(name, config);
            };

            mgmt_api_.unload_adapter = [](void* ctx, const char* name) -> bool {
                auto* self = static_cast<AdapterManager*>(ctx);
                std::lock_guard lock(self->mutex_);
                return self->unloadLocked(name);
            };

            mgmt_api_.list_adapters = [](void* ctx) -> const char* {
                auto* self = static_cast<AdapterManager*>(ctx);
                std::lock_guard lock(self->mutex_);

                std::string str = self->buildAdapterListLocked().dump();
                char* buf = new char[str.size() + 1];
                std::memcpy(buf, str.c_str(), str.size() + 1);
                return buf;
            };

            mgmt_api_.free_string = [](void*, const char* str) {
                delete[] str;
            };
        }

        /// Internal load — caller must hold mutex_.
        bool loadLocked(const std::string& name, const nlohmann::json& config) {
            if(loaded_.count(name)) {
                std::cerr << "[AdapterManager] '" << name << "' is already loaded\n";
                return false;
            }

            // If no config provided, try loading from config/<name>.json
            nlohmann::json effective_config = config;
            if(effective_config.empty() && !config_dir_.empty()) {
                fs::path cfg_path = config_dir_ / (name + ".json");
                effective_config = config::readJsonFile(cfg_path);
                if(!effective_config.empty()) {
                    std::cout << "[AdapterManager] Using config from " << relPath(cfg_path) << "\n";
                }
            }

            auto it = available_.find(name);
            if(it == available_.end()) {
                // DLL may have been added after startup — rescan and retry
                scanAvailableAdapters();
                it = available_.find(name);
                if(it == available_.end()) {
                    std::cerr << "[AdapterManager] No DLL found for adapter '" << name << "'\n";
                    return false;
                }
            }

            std::string dll_path = it->second;
            std::cout << "[AdapterManager] Loading '" << name << "' from " << relPath(dll_path) << "\n";

            // Load DLL
            void* handle = dll::load(dll_path);
            if(!handle) {
                std::cerr << "[AdapterManager] Failed to load " << relPath(dll_path)
                    << ": " << dll::lastError() << "\n";
                return false;
            }

            // Resolve symbols
            const auto create_fn = reinterpret_cast<CreateFn>(dll::getSym(handle, "create_adapter"));
            const auto destroy_fn = reinterpret_cast<DestroyFn>(dll::getSym(handle, "destroy_adapter"));

            if(!create_fn || !destroy_fn) {
                std::cerr << "[AdapterManager] " << relPath(dll_path)
                    << " missing create_adapter/destroy_adapter\n";
                dll::free(handle);
                return false;
            }

            // Create adapter — pass runner, management callbacks, and config
            const std::string config_str = effective_config.dump();
            TransportAdapter* adapter = create_fn(&runner_, &mgmt_api_, config_str.c_str());
            if(!adapter) {
                std::cerr << "[AdapterManager] '" << name
                    << "' create_adapter returned nullptr\n";
                dll::free(handle);
                return false;
            }

            // Start adapter
            // On failure, both destroy_fn and dll::free must be called OUTSIDE the
            // catch block.  While inside catch, the exception object (whose
            // vtable / typeinfo live in the DLL) is still alive; calling back
            // into the DLL (destroy_fn) or unloading it (dll::free) while the
            // C++ runtime is still managing that exception causes ACCESS_VIOLATION.
            bool start_failed = false;
            try {
                adapter->start();
            } catch(const std::exception& e) {
                std::cerr << "[AdapterManager] '" << name
                    << "' failed to start: " << e.what() << "\n";
                start_failed = true;
            } catch(...) {
                std::cerr << "[AdapterManager] '" << name
                    << "' failed to start (unknown error)\n";
                start_failed = true;
            }
            if(start_failed) {
                // Exception is fully destroyed here — safe to call DLL code
                try { destroy_fn(adapter); } catch(...) {}
                dll::free(handle);
                return false;
            }

            loaded_[name] = {handle, adapter, destroy_fn, name, effective_config};
            std::cout << "[AdapterManager] '" << name << "' started\n";
            return true;
        }

        /// Internal unload — caller must hold mutex_.
        bool unloadLocked(const std::string& name) {
            auto it = loaded_.find(name);
            if(it == loaded_.end()) return false;

            std::cout << "[AdapterManager] Stopping '" << name << "'\n";

            void* handle_to_free = it->second.dll_handle;
            TransportAdapter* adapter_to_destroy = it->second.adapter;
            const DestroyFn destroy = it->second.destroy;

            try {
                adapter_to_destroy->stop();
            } catch(const std::exception& e) {
                std::cerr << "[AdapterManager] Error stopping '" << name << "': " << e.what() << "\n";
            } catch(...) {
                std::cerr << "[AdapterManager] Unknown error stopping '" << name << "'\n";
            }
            // Destroy and unload outside catch — exception fully destroyed here
            try { destroy(adapter_to_destroy); } catch(...) {}
            loaded_.erase(it);
            dll::free(handle_to_free);
            std::cout << "[AdapterManager] '" << name << "' unloaded\n";
            return true;
        }

        /// Scan adapters directory and build the available_ index.
        void scanAvailableAdapters() {
            available_.clear();

            if(!fs::is_directory(adapters_dir_)) {
                std::cerr << "[AdapterManager] Adapters dir not found: "
                    << relPath(adapters_dir_) << "\n";
                return;
            }

            #ifdef _WIN32
            const std::string ext = ".dll";
            #else
            const std::string ext = ".so";
            #endif

            const std::string adapter_suffix = "_adapter";
            for(const auto& entry : fs::directory_iterator(adapters_dir_)) {
                if(!entry.is_regular_file()) continue;
                if(entry.path().extension().string() != ext) continue;

                std::string stem = entry.path().stem().string(); // "http_adapter"
                std::string adapter_name;
                if(stem.size() > adapter_suffix.size()
                   && stem.compare(stem.size() - adapter_suffix.size(),
                                   adapter_suffix.size(), adapter_suffix) == 0) {
                    adapter_name = stem.substr(0, stem.size() - adapter_suffix.size());
                } else {
                    adapter_name = stem;
                }
                available_[adapter_name] = entry.path().string();
            }

            std::cout << "[AdapterManager] Found " << available_.size()
                << " adapter(s) in " << relPath(adapters_dir_) << "\n";
            for(auto& [name, path] : available_) {
                std::cout << "  - " << name << " (" << relPath(path) << ")\n";
            }
        }

};