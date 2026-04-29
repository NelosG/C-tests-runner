#pragma once

/**
 * @file main_common.h
 * @brief Shared setup helpers for server and cli entry points.
 */

#include <build_service.h>
#include <config_utils.h>
#include <filesystem>
#include <iostream>
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
inline BOOL WINAPI console_handler(DWORD signal) {
    if(signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT) {
        g_running = false;
        return TRUE;
    }
    return FALSE;
}
#else
#include <csignal>
inline void signal_handler(int) { g_running = false; }
#endif

// ============================================================================
// Common config and setup
// ============================================================================

struct common_config {
    fs::path exe_dir;
    fs::path config_dir;
    fs::path adapters_dir;
    fs::path providers_dir;
    BuildService::BuildConfig build_config;
};

inline std::string lib_near_exe(const fs::path& exe_dir, const std::string& name) {
    fs::path candidate = exe_dir / name;
    return fs::exists(candidate) ? candidate.string() : "";
}

inline common_config setup_common(const fs::path& exe_path) {
    common_config c;
    c.exe_dir = exe_path.parent_path();

    // Flat layout: libs sit next to the exe; adapters/, resource_providers/, include/, config/
    // are subdirs of exe_dir. Env vars (ADAPTERS_DIR / PROVIDERS_DIR / CONFIG_DIR / *_LIB_PATH)
    // override every default below.
    const fs::path inc = c.exe_dir / "include";

    #ifdef _WIN32
    constexpr const char* lib_ext = ".dll";
    #else
    constexpr const char* lib_ext = ".so";
    #endif

    std::string default_engine_lib   = lib_near_exe(c.exe_dir, std::string("libtest_engine") + lib_ext);
    std::string default_parallel_lib = lib_near_exe(c.exe_dir, std::string("libparallel_lib") + lib_ext);

    c.build_config.engine_lib_path = config::get_env("ENGINE_LIB_PATH", default_engine_lib);
    c.build_config.engine_include_path = config::get_env(
        "ENGINE_INCLUDE_PATH", (inc / "test_engine").string());
    c.build_config.parallel_lib_path = config::get_env("PARALLEL_LIB_PATH", default_parallel_lib);
    c.build_config.parallel_include_path = config::get_env(
        "PARALLEL_INCLUDE_PATH", (inc / "parallel_lib").string());
    c.build_config.cmake_executable = config::get_env("CMAKE_EXECUTABLE", "cmake");
    #ifdef _WIN32
    c.build_config.generator = config::get_env("CMAKE_GENERATOR", "MinGW Makefiles");
    #else
    c.build_config.generator = config::get_env("CMAKE_GENERATOR", "Ninja");
    #endif
    c.build_config.exe_dir = c.exe_dir.string();

    c.build_config.runner_lib_path = config::get_env(
        "RUNNER_LIB_PATH",          lib_near_exe(c.exe_dir, "librunner_lib.a"));
    c.build_config.runner_include_path = config::get_env(
        "RUNNER_INCLUDE_PATH",      (inc / "runner_lib").string());
    c.build_config.shadow_omp_dir = config::get_env(
        "SHADOW_OMP_DIR",           (inc / "shadow_omp").string());

    c.build_config.runner_omp_lib_path = config::get_env(
        "RUNNER_OMP_LIB_PATH",      lib_near_exe(c.exe_dir, "librunner_omp.a"));
    c.build_config.runner_parlay_lib_path = config::get_env(
        "RUNNER_PARLAY_LIB_PATH",   lib_near_exe(c.exe_dir, "librunner_parlay.a"));
    c.build_config.runner_cilk_lib_path = config::get_env(
        "RUNNER_CILK_LIB_PATH",     lib_near_exe(c.exe_dir, "librunner_cilk.a"));
    c.build_config.runner_seq_lib_path = config::get_env(
        "RUNNER_SEQ_LIB_PATH",      lib_near_exe(c.exe_dir, "librunner_seq.a"));

    c.build_config.parlay_headers_path = config::get_env(
        "PARLAY_HEADERS_PATH",      inc.string());
    c.build_config.template_dir = config::get_env(
        "ENGINE_TEMPLATE_DIR",      (c.exe_dir / "cmake").string());

    std::string cw_env = config::get_env("CORRECTNESS_WORKERS", "");
    if(!cw_env.empty()) {
        try { c.build_config.correctness_workers = std::stoi(cw_env); } catch(...) {}
    }

    c.adapters_dir = fs::path(config::get_env("ADAPTERS_DIR", (c.exe_dir / "adapters").string()));
    c.providers_dir = fs::path(config::get_env("PROVIDERS_DIR", (c.exe_dir / "resource_providers").string()));
    c.config_dir = fs::path(config::get_env("CONFIG_DIR", (c.exe_dir / "config").string()));
    return c;
}