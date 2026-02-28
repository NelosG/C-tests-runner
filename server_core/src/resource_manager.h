#pragma once

/**
 * @file resource_manager.h
 * @brief Manages dynamically loaded resource provider DLLs.
 *
 * Scans the resource_providers directory for available DLLs, maintains a
 * name → path index, and allows loading/unloading providers at runtime.
 *
 * Each provider resolves a JSON source descriptor to a local filesystem path.
 * Routing: resolve(type, descriptor) → finds provider by type name → calls resolve().
 *
 * Lifecycle mirrors AdapterManager:
 *   - Scan on construction
 *   - load(): validateConfig() → start()
 *   - Two-phase shutdown in stopAll()
 */

#include <config_utils.h>
#include <dll_utils.h>
#include <filesystem>
#include <iostream>
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
            scanAvailableProviders();
        }

        ~ResourceManager() {
            stopAll();
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
                    "Loaded: " + listLoadedTypes()
                );
            }
            return it->second.provider->resolve(descriptor);
        }

        /**
     * @brief Load and start a resource provider by canonical name.
     * @param name    Provider name (e.g. "git") — must match provider_name() from DLL.
     * @param config  JSON config passed to create_provider().
     * @param error   Optional output for error description.
     * @return true on success.
     */
        bool load(const std::string& name, const nlohmann::json& config, std::string* error = nullptr) {
            std::lock_guard lock(mutex_);
            return loadLocked(name, config, error);
        }

        /// Stop and unload a resource provider by name.
        bool unload(const std::string& name) {
            std::lock_guard lock(mutex_);
            return unloadLocked(name);
        }

        /// Get JSON array of all providers (available + loaded).
        nlohmann::json list() const {
            std::lock_guard lock(mutex_);
            return buildProviderListLocked();
        }

        /// Re-scan the providers directory for new DLLs.
        void rescan() {
            std::lock_guard lock(mutex_);
            scanAvailableProviders();
        }

        /// Stop all loaded providers (two-phase shutdown).
        void stopAll() {
            std::lock_guard lock(mutex_);

            struct Cleanup {
                void* handle;
                ResourceProvider* provider;
                DestroyFn destroy;
            };
            std::vector<Cleanup> to_cleanup;
            to_cleanup.reserve(loaded_.size());

            for(auto& [name, mp] : loaded_) {
                std::cout << "[ResourceManager] Stopping '" << name << "'\n";
                try {
                    mp.provider->stop();
                } catch(const std::exception& e) {
                    std::cerr << "[ResourceManager] Error stopping '" << name << "': " << e.what() << "\n";
                } catch(...) {
                    std::cerr << "[ResourceManager] Unknown error stopping '" << name << "'\n";
                }
                to_cleanup.push_back({mp.dll_handle, mp.provider, mp.destroy});
            }
            loaded_.clear();

            // Destroy and unload outside any active exceptions
            for(auto& c : to_cleanup) {
                try { c.destroy(c.provider); } catch(...) {}
                dll::free(c.handle);
            }
        }

        /// Return JSON string of all providers (heap-allocated, caller must free with delete[]).
        const char* listProvidersAlloc() const {
            std::lock_guard lock(mutex_);
            std::string str = buildProviderListLocked().dump();
            char* buf = new char[str.size() + 1];
            std::memcpy(buf, str.c_str(), str.size() + 1);
            return buf;
        }

        /// Return JSON string of available (not yet loaded) providers. Caller must free with delete[].
        const char* listAvailableProvidersAlloc() const {
            std::lock_guard lock(mutex_);
            auto all = buildProviderListLocked();
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
        std::map<std::string, std::string> available_; ///< name → DLL path
        std::map<std::string, ManagedProvider> loaded_;    ///< name → active provider
        mutable std::mutex mutex_;

        /// Display path relative to providers_dir_ with forward slashes.
        std::string relPath(const fs::path& p) const {
            if(providers_dir_.empty() || p.empty()) return p.generic_string();
            try { return fs::proximate(p, providers_dir_).generic_string(); } catch(...) { return p.generic_string(); }
        }

        std::string listLoadedTypes() const {
            std::string result;
            for(auto& [name, _] : loaded_) {
                if(!result.empty()) result += ", ";
                result += name;
            }
            return result.empty() ? "(none)" : result;
        }

        nlohmann::json buildProviderListLocked() const {
            auto arr = nlohmann::json::array();
            for(auto& [name, path] : available_) {
                bool is_loaded = loaded_.count(name) > 0;
                nlohmann::json entry = {
                    {"name", name},
                    {"dllPath", relPath(path)},
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

        bool loadLocked(const std::string& name, const nlohmann::json& config, std::string* error) {
            if(loaded_.count(name)) {
                if(error) *error = "Provider '" + name + "' is already loaded";
                std::cerr << "[ResourceManager] '" << name << "' is already loaded\n";
                return false;
            }

            // Default config fallback: read resource-{name}.json from config_dir
            nlohmann::json effective_config = config;
            if(effective_config.empty() && !config_dir_.empty()) {
                fs::path cfg_path = config_dir_ / ("resource-" + name + ".json");
                effective_config = config::readJsonFile(cfg_path);
                if(!effective_config.empty()) {
                    std::cout << "[ResourceManager] Using config from resource-" << name << ".json\n";
                }
            }

            auto it = available_.find(name);
            if(it == available_.end()) {
                scanAvailableProviders();
                it = available_.find(name);
                if(it == available_.end()) {
                    if(error) *error = "No DLL found for resource provider '" + name + "'";
                    std::cerr << "[ResourceManager] No DLL found for provider '" << name << "'\n";
                    return false;
                }
            }

            std::string dll_path = it->second;
            std::cout << "[ResourceManager] Loading '" << name << "' from " << dll_path << "\n";

            void* handle = dll::load(dll_path);
            if(!handle) {
                if(error) *error = "Failed to load DLL: " + dll::lastError();
                std::cerr << "[ResourceManager] Failed to load " << dll_path
                    << ": " << dll::lastError() << "\n";
                return false;
            }

            const auto create_fn = reinterpret_cast<CreateFn>(dll::getSym(handle, "create_provider"));
            const auto destroy_fn = reinterpret_cast<DestroyFn>(dll::getSym(handle, "destroy_provider"));
            if(!create_fn || !destroy_fn) {
                if(error) *error = "DLL missing create_provider/destroy_provider symbols";
                std::cerr << "[ResourceManager] " << dll_path
                    << " missing create_provider/destroy_provider\n";
                dll::free(handle);
                return false;
            }

            // Create provider instance
            ResourceContext ctx{effective_config};
            ResourceProvider* provider = create_fn(&ctx);
            if(!provider) {
                if(error) *error = "create_provider returned nullptr";
                std::cerr << "[ResourceManager] '" << name << "' create_provider returned nullptr\n";
                dll::free(handle);
                return false;
            }

            // Validate config before starting
            std::string validate_error;
            if(!provider->validateConfig(effective_config, validate_error)) {
                if(error) *error = validate_error;
                std::cerr << "[ResourceManager] '" << name
                    << "' config validation failed: " << validate_error << "\n";
                bool start_failed = false;
                try { destroy_fn(provider); } catch(...) { start_failed = true; }
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
                std::cerr << "[ResourceManager] '" << name
                    << "' failed to start: " << start_error << "\n";
                try { destroy_fn(provider); } catch(...) {}
                dll::free(handle);
                return false;
            }

            loaded_[name] = {handle, provider, destroy_fn, name, effective_config};
            std::cout << "[ResourceManager] '" << name << "' loaded and started\n";
            return true;
        }

        bool unloadLocked(const std::string& name) {
            auto it = loaded_.find(name);
            if(it == loaded_.end()) return false;

            std::cout << "[ResourceManager] Stopping '" << name << "'\n";
            void* handle_to_free = it->second.dll_handle;
            ResourceProvider* provider_to_destroy = it->second.provider;
            const DestroyFn destroy = it->second.destroy;

            try {
                provider_to_destroy->stop();
            } catch(const std::exception& e) {
                std::cerr << "[ResourceManager] Error stopping '" << name << "': " << e.what() << "\n";
            } catch(...) {}

            try { destroy(provider_to_destroy); } catch(...) {}
            loaded_.erase(it);
            dll::free(handle_to_free);
            std::cout << "[ResourceManager] '" << name << "' unloaded\n";
            return true;
        }

        void scanAvailableProviders() {
            available_.clear();

            if(!fs::is_directory(providers_dir_)) {
                std::cerr << "[ResourceManager] Providers dir not found: "
                    << providers_dir_.generic_string() << "\n";
                return;
            }

            #ifdef _WIN32
            const std::string ext = ".dll";
            #else
            const std::string ext = ".so";
            #endif
            const std::string provider_prefix = "resource_";

            for(const auto& entry : fs::directory_iterator(providers_dir_)) {
                if(!entry.is_regular_file()) continue;
                if(entry.path().extension().string() != ext) continue;

                std::string stem = entry.path().stem().string(); // "resource_git"
                std::string pname;
                if(stem.size() > provider_prefix.size()
                    && stem.compare(0, provider_prefix.size(), provider_prefix) == 0) {
                    pname = stem.substr(provider_prefix.size()); // "git"
                } else {
                    pname = stem;
                }
                available_[pname] = entry.path().string();
            }

            std::cout << "[ResourceManager] Found " << available_.size()
                << " provider(s) in " << providers_dir_.generic_string() << "\n";
            for(auto& [name, path] : available_) {
                std::cout << "  - " << name << " (" << path << ")\n";
            }
        }
};