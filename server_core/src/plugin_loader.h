#pragma once

/**
 * @file plugin_loader.h
 * @brief Cross-platform dynamic library loader for test plugins.
 *
 * Loads .dll (Windows) / .so (Linux) files via LoadLibraryA / dlopen, triggering
 * static REGISTER_TEST() initializers that populate the active TestRegistry.
 *
 * unload_all() does NOT clear the registry - that's the caller's responsibility,
 * because the registry holds objects whose vtables live in the loaded DLLs and
 * must be destroyed BEFORE the library handles are closed.
 */

#include <string>
#include <utility>
#include <vector>

class PluginLoader {
    public:
        PluginLoader() = default;
        ~PluginLoader();

        PluginLoader(const PluginLoader&) = delete;
        PluginLoader& operator=(const PluginLoader&) = delete;

        PluginLoader(PluginLoader&&) noexcept;
        PluginLoader& operator=(PluginLoader&&) noexcept;

        /// Load a single plugin by file path. Returns true on success.
        bool load_plugin(const std::string& plugin_path);

        /// Close all loaded library handles. Caller MUST clear any registry that
        /// holds objects from these DLLs first.
        void unload_all();

    private:
        std::vector<std::pair<std::string, void*>> plugin_handles_;
};
