#pragma once

/**
 * @file main_common.h
 * @brief Shared setup helpers for server and cli entry points.
 */

#include <build_service.h>
#include <config_utils.h>
#include <filesystem>
#include <iostream>
#include <project_utils.h>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

// ============================================================================
// Shutdown flag
// ============================================================================

#include <atomic>
inline std::atomic<bool> g_running{true};

#ifdef _WIN32
inline BOOL WINAPI consoleHandler(DWORD signal) {
    if(signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT) {
        g_running = false;
        return TRUE;
    }
    return FALSE;
}
#else
#include <csignal>
inline void signalHandler(int) { g_running = false; }
#endif

// ============================================================================
// Common config and setup
// ============================================================================

struct common_config {
    fs::path exe_dir;
    fs::path project_root;
    fs::path config_dir;
    fs::path adapters_dir;
    fs::path providers_dir;
    BuildService::BuildConfig build_config;
};

inline std::string findLibNearExe(const fs::path& exe_dir, const std::string& name) {
    fs::path candidate = exe_dir / name;
    if(fs::exists(candidate)) return candidate.string();
    return "";
}

inline common_config setupCommon(const fs::path& exe_path) {
    common_config c;
    c.exe_dir = exe_path.parent_path();
    c.project_root = project::findRoot(exe_path);
    if(c.project_root.empty()) c.project_root = fs::current_path();

    #ifdef _WIN32
    std::string default_engine_lib = findLibNearExe(c.exe_dir, "libtest_engine.dll");
    std::string default_parallel_lib = findLibNearExe(c.exe_dir, "libparallel_lib.dll");
    #else
    std::string default_engine_lib = findLibNearExe(c.exe_dir, "libtest_engine.so");
    std::string default_parallel_lib = findLibNearExe(c.exe_dir, "libparallel_lib.so");
    #endif

    c.build_config.engine_lib_path = config::getEnv("ENGINE_LIB_PATH", default_engine_lib);
    c.build_config.engine_include_path = config::getEnv(
        "ENGINE_INCLUDE_PATH",
        (c.project_root / "test_engine" / "src").string()
    );
    c.build_config.parallel_lib_path = config::getEnv("PARALLEL_LIB_PATH", default_parallel_lib);
    c.build_config.parallel_include_path = config::getEnv(
        "PARALLEL_INCLUDE_PATH",
        (c.project_root / "parallel_lib" / "include").string()
    );
    c.build_config.cmake_executable = config::getEnv("CMAKE_EXECUTABLE", "cmake");
    #ifdef _WIN32
    c.build_config.generator = config::getEnv("CMAKE_GENERATOR", "MinGW Makefiles");
    #else
    c.build_config.generator = config::getEnv("CMAKE_GENERATOR", "Ninja");
    #endif
    c.build_config.exe_dir = c.exe_dir.string();
    c.build_config.memory_inject_path = (c.project_root / "parallel_lib" / "src" / "memory_limit_inject.cpp").string();

    std::string cw_env = config::getEnv("CORRECTNESS_WORKERS", "");
    if(!cw_env.empty()) {
        try { c.build_config.correctness_workers = std::stoi(cw_env); } catch(...) {}
    }

    c.adapters_dir = fs::path(config::getEnv("ADAPTERS_DIR", (c.exe_dir / "adapters").string()));
    c.providers_dir = fs::path(config::getEnv("PROVIDERS_DIR", (c.exe_dir / "resource_providers").string()));
    c.config_dir = fs::path(config::getEnv("CONFIG_DIR", (c.project_root / "config").string()));
    return c;
}