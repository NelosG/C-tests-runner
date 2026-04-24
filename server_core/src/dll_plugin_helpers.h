#pragma once

/**
 * @file dll_plugin_helpers.h
 * @brief Shared helpers for DLL-plugin managers (AdapterManager, ResourceManager).
 *
 * Each manager owns its own typed plugin map and lifecycle (factory args differ:
 * adapters get TestRunnerService + ManagementAPI; providers get only ResourceContext).
 * The DLL scanning, symbol resolution, and config fallback are identical though,
 * and they live here.
 */

#include <config_utils.h>
#include <dll_utils.h>
#include <filesystem>
#include <map>
#include <string>
#include <nlohmann/json.hpp>


namespace dll_plugin_helpers {

    /// How a plugin's DLL filename maps to its logical name. E.g.
    /// adapters use suffix `_adapter` => `http_adapter.dll` -> name "http".
    /// providers use prefix `resource_` => `resource_git.dll` -> name "git".
    struct NamingScheme {
        std::string dll_prefix;   ///< stripped from start of stem (e.g. "resource_")
        std::string dll_suffix;   ///< stripped from end of stem (e.g. "_adapter")

        /// Strip prefix/suffix to recover the logical name; returns the bare stem
        /// if no prefix/suffix matches (older plugin layouts).
        std::string name_from_stem(const std::string& stem) const;
    };

    /// Scan `dir` for *.dll/*.so files; return logical_name -> absolute_path.
    /// Symlinks and non-files are ignored. Errors are logged via std::cerr with
    /// `log_prefix` (e.g. "AdapterManager").
    std::map<std::string, std::string> scan_plugin_dir(
        const std::filesystem::path& dir,
        const NamingScheme& scheme,
        const std::string& log_prefix
    );

    /// Result of loadAndResolve: handle + the two required factory symbols.
    /// On failure all members except `error` are nullptr/empty.
    struct LoadedDll {
        void* handle = nullptr;
        void* create_sym = nullptr;
        void* destroy_sym = nullptr;
        std::string error;
    };

    /// dll::load + dll::getSym for create/destroy. Caller owns `handle` on success
    /// and must call dll::free on it.
    LoadedDll load_and_resolve(
        const std::string& dll_path,
        const std::string& create_symbol,
        const std::string& destroy_symbol
    );

    /// If `provided` is non-empty, return it. Otherwise read
    /// `config_dir/fallback_filename` and return that (or `{}` if absent).
    nlohmann::json effective_config(
        const nlohmann::json& provided,
        const std::filesystem::path& config_dir,
        const std::string& fallback_filename
    );

} // namespace dll_plugin_helpers
