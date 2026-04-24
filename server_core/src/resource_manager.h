#pragma once

/**
 * @file resource_manager.h
 * @brief Manages dynamically loaded resource provider DLLs.
 *
 * Scans the resource_providers directory for available DLLs, maintains a
 * name -> path index, and allows loading/unloading providers at runtime.
 *
 * Each provider resolves a JSON source descriptor to a local filesystem path.
 * Routing: resolve(type, descriptor) -> finds provider by type name -> calls resolve().
 *
 * Lifecycle mirrors AdapterManager:
 *   - Scan on construction
 *   - load(): validate_config() -> start()
 *   - Two-phase shutdown in stop_all()
 */

#include <config_utils.h>
#include <dll_plugin_helpers.h>
#include <dll_utils.h>
#include <log_utils.h>
#include <path_utils.h>
#include <filesystem>
#include <map>
#include <mutex>
#include <resource_context.h>
#include <resource_provider.h>
#include <resource_provider_status.h>
#include <string>
#include <utility>
#include <vector>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

class ResourceManager {
    public:
        ResourceManager(fs::path providers_dir, fs::path config_dir = {})
            : providers_dir_(std::move(providers_dir)),
              config_dir_(std::move(config_dir)) {
            scan_available_providers();
        }

        ~ResourceManager() {
            stop_all();
        }

        /**
     * @brief Resolve a source descriptor to a local path.
     * @param type        Provider name ("git", "local")
     * @param descriptor  JSON descriptor for the source
     * @throws std::runtime_error if provider not loaded or resolution fails
     */
        std::filesystem::path resolve(const std::string& type, const nlohmann::json& descriptor) {
            std::lock_guard lock(mutex_);
            auto it = loaded_.find(type);
            if(it == loaded_.end()) {
                throw std::runtime_error(
                    "No resource provider loaded for type '" + type + "'. "
                    "Loaded: " + list_loaded_types()
                );
            }
            return it->second.provider->resolve(descriptor);
        }

        /**
     * @brief Load and start a resource provider by canonical name.
     * @param name    Provider name (e.g. "git") - must match provider_name() from DLL.
     * @param config  JSON config passed to create_provider().
     * @param error   Optional output for error description.
     * @return true on success.
     */
        bool load(const std::string& name, const nlohmann::json& config, std::string* error = nullptr) {
            std::lock_guard lock(mutex_);
            return load_locked(name, config, error);
        }

        /// Stop and unload a resource provider by name.
        bool unload(const std::string& name) {
            std::lock_guard lock(mutex_);
            return unload_locked(name);
        }

        /// Get JSON array of all providers (available + loaded).
        nlohmann::json list() const {
            std::lock_guard lock(mutex_);
            return build_provider_list_locked();
        }

        /// Re-scan the providers directory for new DLLs.
        void rescan() {
            std::lock_guard lock(mutex_);
            scan_available_providers();
        }

        /// Stop all loaded providers (two-phase shutdown).
        void stop_all() {
            std::lock_guard lock(mutex_);

            struct Cleanup {
                void* handle;
                ResourceProvider* provider;
                DestroyFn destroy;
            };
            std::vector<Cleanup> to_cleanup;
            to_cleanup.reserve(loaded_.size());

            // Snapshot names BEFORE iterating: provider->stop() runs untrusted
            // DLL code that may (via management API re-entry on the same thread)
            // call back into unload and erase from loaded_, which would
            // invalidate a range-for iterator. See AdapterManager::stop_all
            // for the same pattern.
            std::vector<std::string> names;
            names.reserve(loaded_.size());
            for(auto& [name, _] : loaded_) names.push_back(name);

            for(const auto& name : names) {
                auto it = loaded_.find(name);
                if(it == loaded_.end()) continue; // erased by re-entrant unload
                auto& mp = it->second;
                LOG("ResourceManager") << "Stopping '" << name << "'\n";
                try {
                    mp.provider->stop();
                } catch(const std::exception& e) {
                    LOG_ERR("ResourceManager") << "Error stopping '" << name << "': " << e.what() << "\n";
                } catch(...) {
                    LOG_ERR("ResourceManager") << "Unknown error stopping '" << name << "'\n";
                }
                it = loaded_.find(name);
                if(it == loaded_.end()) continue;
                to_cleanup.push_back({it->second.dll_handle, it->second.provider, it->second.destroy});
            }
            loaded_.clear();

            // Destroy and unload outside any active exceptions
            for(auto& c : to_cleanup) {
                try { c.destroy(c.provider); } catch(...) {}
                dll::free(c.handle);
            }
        }

        /// Return JSON string of all providers (heap-allocated, caller must free with delete[]).
        const char* list_providers_alloc() const {
            std::lock_guard lock(mutex_);
            std::string str = build_provider_list_locked().dump();
            char* buf = new char[str.size() + 1];
            std::memcpy(buf, str.c_str(), str.size() + 1);
            return buf;
        }

        /// Return JSON string of available (not yet loaded) providers. Caller must free with delete[].
        const char* list_available_providers_alloc() const {
            std::lock_guard lock(mutex_);
            auto all = build_provider_list_locked();
            auto available = nlohmann::json::array();
            for(auto& entry : all) {
                if(entry.value("status", "") == to_string(resource_provider_status::available)) {
                    available.push_back(entry);
                }
            }
            std::string str = available.dump();
            char* buf = new char[str.size() + 1];
            std::memcpy(buf, str.c_str(), str.size() + 1);
            return buf;
        }

    private:
        using CreateFn = ResourceProvider* (*)(const ResourceContext*);
        using DestroyFn = void (*)(ResourceProvider*);
        using NameFn = const char* (*)();

        struct ManagedProvider {
            void* dll_handle{};
            ResourceProvider* provider{};
            DestroyFn destroy{};
            std::string name;
            nlohmann::json config;
        };

        fs::path providers_dir_;
        fs::path config_dir_;
        std::map<std::string, std::string> available_; ///< name -> DLL path
        std::map<std::string, ManagedProvider> loaded_;    ///< name -> active provider
        mutable std::mutex mutex_;

        /// Display path relative to providers_dir_ with forward slashes.
        std::string rel_path(const fs::path& p) const {
            return path_utils::rel(p, providers_dir_);
        }

        std::string list_loaded_types() const {
            std::string result;
            for(auto& [name, _] : loaded_) {
                if(!result.empty()) result += ", ";
                result += name;
            }
            return result.empty() ? "(none)" : result;
        }

        nlohmann::json build_provider_list_locked() const {
            auto arr = nlohmann::json::array();
            for(auto& [name, path] : available_) {
                bool is_loaded = loaded_.count(name) > 0;
                nlohmann::json entry = {
                    {"name", name},
                    {
                        "status",
                        to_string(
                            is_loaded
                            ? resource_provider_status::running
                            : resource_provider_status::available
                        )
                    }
                };
                if(is_loaded) {
                    entry["config"] = loaded_.at(name).config;
                }
                arr.push_back(entry);
            }
            return arr;
        }

        bool load_locked(const std::string& name, const nlohmann::json& config, std::string* error) {
            if(loaded_.count(name)) {
                if(error) *error = "Provider '" + name + "' is already loaded";
                LOG_ERR("ResourceManager") << "'" << name << "' is already loaded\n";
                return false;
            }

            nlohmann::json effective_config = dll_plugin_helpers::effective_config(
                config,
                config_dir_,
                "resource-" + name + ".json"
            );

            auto it = available_.find(name);
            if(it == available_.end()) {
                scan_available_providers();
                it = available_.find(name);
                if(it == available_.end()) {
                    if(error) *error = "No DLL found for resource provider '" + name + "'";
                    LOG_ERR("ResourceManager") << "No DLL found for provider '" << name << "'\n";
                    return false;
                }
            }

            std::string dll_path = it->second;
            LOG("ResourceManager") << "Loading '" << name << "' from " << dll_path << "\n";

            auto loaded = dll_plugin_helpers::load_and_resolve(
                dll_path,
                "create_provider",
                "destroy_provider"
            );
            if(!loaded.handle) {
                if(error) *error = loaded.error;
                LOG_ERR("ResourceManager") << dll_path << ": " << loaded.error << "\n";
                return false;
            }
            void* handle = loaded.handle;
            const auto create_fn = reinterpret_cast<CreateFn>(loaded.create_sym);
            const auto destroy_fn = reinterpret_cast<DestroyFn>(loaded.destroy_sym);

            // Create provider instance
            ResourceContext ctx{effective_config};
            ResourceProvider* provider = create_fn(&ctx);
            if(!provider) {
                if(error) *error = "create_provider returned nullptr";
                LOG_ERR("ResourceManager") << "'" << name << "' create_provider returned nullptr\n";
                dll::free(handle);
                return false;
            }

            // Validate config before starting
            std::string validate_error;
            bool valid = false;
            try {
                valid = provider->validate_config(effective_config, validate_error);
            } catch(const std::exception& e) {
                validate_error = std::string("validateConfig threw: ") + e.what();
            } catch(...) {
                validate_error = "validateConfig threw unknown exception";
            }
            if(!valid) {
                if(error) *error = validate_error;
                LOG_ERR("ResourceManager") << "'" << name
                    << "' config validation failed: " << validate_error << "\n";
                try { destroy_fn(provider); } catch(...) {}
                dll::free(handle);
                return false;
            }

            // Start provider (e.g. background cleanup thread)
            bool start_failed = false;
            std::string start_error;
            try {
                provider->start();
            } catch(const std::exception& e) {
                start_error = e.what();
                start_failed = true;
            } catch(...) {
                start_error = "unknown error";
                start_failed = true;
            }
            if(start_failed) {
                if(error) *error = "Provider failed to start: " + start_error;
                LOG_ERR("ResourceManager") << "'" << name
                    << "' failed to start: " << start_error << "\n";
                try { destroy_fn(provider); } catch(...) {}
                dll::free(handle);
                return false;
            }

            loaded_[name] = {handle, provider, destroy_fn, name, effective_config};
            LOG("ResourceManager") << "'" << name << "' loaded and started\n";
            return true;
        }

        bool unload_locked(const std::string& name) {
            auto it = loaded_.find(name);
            if(it == loaded_.end()) return false;

            LOG("ResourceManager") << "Stopping '" << name << "'\n";
            void* handle_to_free = it->second.dll_handle;
            ResourceProvider* provider_to_destroy = it->second.provider;
            const DestroyFn destroy = it->second.destroy;

            try {
                provider_to_destroy->stop();
            } catch(const std::exception& e) {
                LOG_ERR("ResourceManager") << "Error stopping '" << name << "': " << e.what() << "\n";
            } catch(...) {}

            try { destroy(provider_to_destroy); } catch(...) {}
            loaded_.erase(it);
            dll::free(handle_to_free);
            LOG("ResourceManager") << "'" << name << "' unloaded\n";
            return true;
        }

        void scan_available_providers() {
            available_ = dll_plugin_helpers::scan_plugin_dir(
                providers_dir_,
                {/*prefix*/"resource_", /*suffix*/""},
                "ResourceManager"
            );
        }
};
