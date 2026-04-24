#pragma once

/**
 * @file adapter_manager.h
 * @brief Manages dynamically loaded adapter DLLs.
 *
 * Scans the adapters directory for available DLLs, maintains a name -> path
 * index, and allows loading/unloading adapters at runtime with arbitrary
 * JSON configs.
 *
 * Provides ManagementAPI C callbacks so that adapters (e.g. HTTP) can
 * expose management endpoints without crossing C++ ABI boundaries.
 */

#include <adapter_api.h>
#include <adapter_context.h>
#include <adapter_status.h>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <nlohmann/json.hpp>

class ResourceManager;
class TestRunnerService;
class TransportAdapter;

class AdapterManager {
    public:
        AdapterManager(
            TestRunnerService& runner,
            std::filesystem::path adapters_dir,
            std::filesystem::path config_dir = {},
            std::filesystem::path exe_dir = {},
            std::string node_id = {},
            ResourceManager* resource_manager = nullptr
        );

        ~AdapterManager();

        /// Get the ManagementAPI struct to pass to adapters.
        const ManagementAPI& management_api() const { return mgmt_api_; }

        /// Load, create, and start an adapter by canonical name.
        /// @param name    Adapter name (e.g. "rabbit") - must match adapter_name() from DLL.
        /// @param config  JSON config passed to create_adapter().
        /// @return true on success.
        bool load(const std::string& name, const nlohmann::json& config);

        /// Stop and unload an adapter by name.
        bool unload(const std::string& name);

        /// List available and loaded adapters.
        nlohmann::json list() const;

        /// Re-scan the adapters directory for new DLLs.
        void rescan();

        /// Stop all loaded adapters.
        void stop_all();

    private:
        using CreateFn = TransportAdapter* (*)(TestRunnerService*, const ManagementAPI*, const AdapterContext*);
        using DestroyFn = void (*)(TransportAdapter*);

        struct ManagedAdapter {
            void* dll_handle = nullptr;
            TransportAdapter* adapter = nullptr;
            DestroyFn destroy = nullptr;
            std::string name;
            nlohmann::json config;
        };

        std::string rel_path(const std::filesystem::path& p) const;
        nlohmann::json build_adapter_list_locked() const;
        void build_management_api();
        bool load_locked(const std::string& name, const nlohmann::json& config);
        bool unload_locked(const std::string& name);
        void scan_available_adapters();

        TestRunnerService& runner_;
        std::filesystem::path adapters_dir_;
        std::filesystem::path config_dir_;
        std::filesystem::path exe_dir_;
        std::string node_id_;
        ResourceManager* resource_manager_;
        ManagementAPI mgmt_api_{};

        std::map<std::string, std::string> available_;   ///< name -> DLL path
        std::map<std::string, ManagedAdapter> loaded_;   ///< name -> running adapter
        mutable std::recursive_mutex mutex_;
};
