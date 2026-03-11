#pragma once

/**
 * @file plugin_loader.h
 * @brief Cross-platform dynamic library loader for test plugins.
 */

#include <string>
#include <vector>

/**
 * @brief Loads and manages dynamically linked test plugin libraries.
 *
 * Scans a directory for .dll (Windows) or .so (Linux) files, loads each one
 * via LoadLibraryA / dlopen, triggering static REGISTER_TEST() initializers
 * that populate the global TestRegistry.
 *
 * @note unloadAll() clears the TestRegistry before closing library handles
 *       to prevent dangling vtable pointers.
 */
class PluginLoader {
    public:
        PluginLoader() = default;
        ~PluginLoader();

        PluginLoader(const PluginLoader&) = delete;
        PluginLoader& operator=(const PluginLoader&) = delete;

        PluginLoader(PluginLoader&&) noexcept;
        PluginLoader& operator=(PluginLoader&&) noexcept;

        /**
     * @brief Load all plugins from a directory.
     * @param directory Path to the directory containing .dll/.so files.
     * @return Number of successfully loaded plugins.
     */
        size_t loadPluginsFromDirectory(const std::string& directory);

        /**
     * @brief Load a single plugin by file path.
     * @param plugin_path Full path to the .dll/.so file.
     * @return True if the plugin was loaded successfully.
     */
        bool loadPlugin(const std::string& plugin_path);

        /**
     * @brief Get paths of all currently loaded plugins.
     * @return Vector of file paths.
     */
        std::vector<std::string> getLoadedPlugins() const;

        /**
     * @brief Unload all plugins safely.
     *
     * First clears TestRegistry (destroying test objects whose vtables live
     * in the plugin), then closes all library handles.
     */
        void unloadAll();

    private:
        std::vector<std::pair<std::string, void*>> plugin_handles_;

        std::vector<std::string> findPluginFiles(const std::string& directory);
};