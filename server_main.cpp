/**
 * @file server_main.cpp
 * @brief Server entry point — adapter-based server or standalone CLI runner.
 *
 * Modes:
 *   1. Server mode (no args) — loads adapter DLLs, interactive console.
 *   2. CLI mode              — run tests once, write result to output.json.
 *
 * CLI usage:
 *   server --test-dir <dir> --test-id <id> --solution <dir> [--mode all] [--threads 4]
 *
 * Environment overrides:
 *   ENGINE_LIB_PATH, ENGINE_INCLUDE_PATH, PARALLEL_LIB_PATH,
 *   PARALLEL_INCLUDE_PATH, CMAKE_EXECUTABLE, CMAKE_GENERATOR,
 *   CONFIG_DIR, ADAPTERS_DIR
 */

#include <adapter_manager.h>
#include <atomic>
#include <build_service.h>
#include <config_utils.h>
#include <csignal>
#include <filesystem>
#include <functional>
#include <iostream>
#include <project_utils.h>
#include <string>
#include <test_runner_service.h>
#include <unordered_map>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

// ============================================================================
// Shutdown handling
// ============================================================================

static std::atomic<bool> g_running{true};

#ifdef _WIN32
static BOOL WINAPI consoleHandler(DWORD signal) {
    if(signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT) {
        g_running = false;
        return TRUE;
    }
    return FALSE;
}
#else
static void signalHandler(int) {
    g_running = false;
}
#endif

// ============================================================================
// File-local helpers
// ============================================================================

namespace {

std::string findLibNearExe(const fs::path& exe_dir, const std::string& name) {
    fs::path candidate = exe_dir / name;
    if(fs::exists(candidate)) return candidate.string();
    return "";
}

std::string relPath(const fs::path& p, const fs::path& base) {
    if(base.empty() || p.empty()) return p.generic_string();
    try { return fs::proximate(p, base).generic_string(); }
    catch(...) { return p.generic_string(); }
}

void trimRight(std::string& s) {
    while(!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ')) {
        s.pop_back();
    }
}

// ============================================================================
// Common setup
// ============================================================================

struct common_config {
    fs::path exe_dir;
    fs::path project_root;
    fs::path config_dir;
    fs::path adapters_dir;
    BuildService::BuildConfig build_config;
};

common_config setupCommon(const fs::path& exe_path) {
    common_config c;
    c.exe_dir = exe_path.parent_path();
    c.project_root = project::findRoot(exe_path);
    if(c.project_root.empty()) {
        c.project_root = fs::current_path();
    }

    #ifdef _WIN32
    std::string default_engine_lib = findLibNearExe(c.exe_dir, "libtest_engine.dll");
    std::string default_parallel_lib = findLibNearExe(c.exe_dir, "libparallel_lib.dll");
    #else
    std::string default_engine_lib = findLibNearExe(c.exe_dir, "libtest_engine.so");
    std::string default_parallel_lib = findLibNearExe(c.exe_dir, "libparallel_lib.so");
    #endif

    c.build_config.engine_lib_path = config::getEnv("ENGINE_LIB_PATH", default_engine_lib);
    c.build_config.engine_include_path = config::getEnv("ENGINE_INCLUDE_PATH",
        (c.project_root / "test_engine" / "src").string());
    c.build_config.parallel_lib_path = config::getEnv("PARALLEL_LIB_PATH", default_parallel_lib);
    c.build_config.parallel_include_path = config::getEnv("PARALLEL_INCLUDE_PATH",
        (c.project_root / "parallel_lib" / "include").string());
    c.build_config.cmake_executable = config::getEnv("CMAKE_EXECUTABLE", "cmake");
    #ifdef _WIN32
    c.build_config.generator = config::getEnv("CMAKE_GENERATOR", "MinGW Makefiles");
    #else
    c.build_config.generator = config::getEnv("CMAKE_GENERATOR", "Ninja");
    #endif

    c.build_config.exe_dir = c.exe_dir.string();

    // correctness_workers will be overridden from config/env in server mode
    std::string cw_env = config::getEnv("CORRECTNESS_WORKERS", "");
    if(!cw_env.empty()) {
        try { c.build_config.correctness_workers = std::stoi(cw_env); } catch(...) {}
    }

    c.adapters_dir = fs::path(config::getEnv("ADAPTERS_DIR", (c.exe_dir / "adapters").string()));
    c.config_dir = fs::path(config::getEnv("CONFIG_DIR", (c.project_root / "config").string()));

    return c;
}

// ============================================================================
// CLI mode — single test run
// ============================================================================

int runCli(const common_config& cfg, int argc, char** argv) {
    std::string solution_dir;
    std::string test_dir;
    std::string test_id;
    std::string mode = "all";
    int threads = 4;
    std::string output_file = "result/output.json";

    for(int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if(arg == "--solution" && i + 1 < argc) solution_dir = argv[++i];
        else if(arg == "--test-dir" && i + 1 < argc) test_dir = argv[++i];
        else if(arg == "--test-id" && i + 1 < argc) test_id = argv[++i];
        else if(arg == "--mode" && i + 1 < argc) mode = argv[++i];
        else if(arg == "--threads" && i + 1 < argc) {
            try { threads = std::stoi(argv[++i]); }
            catch(const std::exception&) {
                std::cerr << "[CLI] Invalid --threads value: " << argv[i] << "\n";
                return 1;
            }
        }
        else if(arg == "--output" && i + 1 < argc) output_file = argv[++i];
        else if(arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                << "\nRequired:\n"
                << "  --test-dir <dir>       Path to test directory\n"
                << "  --test-id <id>         Test identifier (for caching)\n"
                << "  --solution <dir>       Path to student solution directory\n"
                << "\nOptions:\n"
                << "  --mode <mode>          correctness | performance | all (default: all)\n"
                << "  --threads <N>          Number of threads (default: 4)\n"
                << "  --output <file>        Output file (default: result/output.json)\n"
                << "\nWith no arguments, starts in server mode.\n";
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            return 1;
        }
    }

    if(solution_dir.empty()) {
        std::cerr << "ERROR: --solution is required\n";
        return 1;
    }

    if(test_id.empty()) {
        std::cerr << "ERROR: --test-id is required\n";
        return 1;
    }

    if(test_dir.empty()) {
        std::cerr << "ERROR: --test-dir is required\n";
        return 1;
    }

    TestRunnerService runner(cfg.build_config);

    auto run_one = [&](const std::string& run_mode) -> nlohmann::json {
        nlohmann::json request = {
            {"jobId", "cli-run"},
            {"solutionDir", solution_dir},
            {"testId", test_id},
            {"testDir", test_dir},
            {"mode", run_mode},
            {"threads", threads}
        };

        auto noop = [](JobQueue::JobStatus) {};
        return runner.execute(request, noop);
    };

    nlohmann::json report;

    try {
        std::cout << "[CLI] Running " << mode << " tests...\n";
        report = run_one(mode);
    } catch(const std::exception& e) {
        std::cerr << "[CLI] Error: " << e.what() << "\n";
        report["status"] = "failed";
        report["error"] = e.what();
    }

    // Write output
    fs::path out_path(output_file);
    if(out_path.has_parent_path()) {
        fs::create_directories(out_path.parent_path());
    }
    std::ofstream out(output_file);
    if(!out.is_open()) {
        std::cerr << "[CLI] Cannot write to " << output_file << "\n";
        std::cout << report.dump(4) << "\n";
        return 1;
    }
    out << report.dump(4) << "\n";
    out.close();
    std::cout << "[CLI] Results written to " << output_file << "\n";

    return report.value("status", "") == "failed" ? 1 : 0;
}

// ============================================================================
// Interactive console — command dispatch
// ============================================================================

using command_handler = std::function<bool(AdapterManager&, const std::string&)>;

const std::unordered_map<std::string, std::string> command_aliases = {
    {"l", "list"},
    {"u", "unload"},
    {"h", "help"},
    {"q", "quit"},
};

std::unordered_map<std::string, command_handler> buildCommandHandlers() {
    std::unordered_map<std::string, command_handler> handlers;

    handlers["list"] = [](AdapterManager& mgr, const std::string&) {
        auto adapters = mgr.list();
        std::cout << "[Server] Adapters:\n" << adapters.dump(2) << "\n";
        return true;
    };

    handlers["load"] = [](AdapterManager& mgr, const std::string& args) {
        std::string rest = args;
        while(!rest.empty() && rest[0] == ' ') rest.erase(rest.begin());
        auto space_pos = rest.find(' ');
        std::string name;
        nlohmann::json lc;
        if(space_pos == std::string::npos) {
            name = rest;
        } else {
            name = rest.substr(0, space_pos);
            std::string json_str = rest.substr(space_pos + 1);
            while(!json_str.empty() && json_str[0] == ' ') json_str.erase(json_str.begin());
            if(!json_str.empty()) {
                try {
                    lc = nlohmann::json::parse(json_str);
                } catch(const std::exception& e) {
                    std::cerr << "[Server] Invalid JSON: " << e.what() << "\n";
                    return true;
                }
            }
        }
        if(name.empty()) {
            std::cout << "[Server] Usage: load <adapter_name> [json_config]\n";
        } else {
            std::cout << "[Server] Loading adapter '" << name << "'...\n";
            if(mgr.load(name, lc)) {
                std::cout << "[Server] '" << name << "' loaded successfully\n";
            } else {
                std::cerr << "[Server] Failed to load '" << name << "'\n";
            }
        }
        return true;
    };

    handlers["unload"] = [](AdapterManager& mgr, const std::string& args) {
        std::string rest = args;
        while(!rest.empty() && rest[0] == ' ') rest.erase(rest.begin());
        if(rest.empty()) {
            std::cout << "[Server] Usage: unload <adapter_name>\n";
        } else {
            std::cout << "[Server] Unloading adapter '" << rest << "'...\n";
            if(mgr.unload(rest)) {
                std::cout << "[Server] '" << rest << "' unloaded\n";
            } else {
                std::cerr << "[Server] '" << rest << "' not found or not running\n";
            }
        }
        return true;
    };

    handlers["rescan"] = [](AdapterManager& mgr, const std::string&) {
        mgr.rescan();
        std::cout << "[Server] Adapter directory rescanned\n";
        auto adapters = mgr.list();
        std::cout << adapters.dump(2) << "\n";
        return true;
    };

    handlers["help"] = [](AdapterManager&, const std::string&) {
        std::cout << "[Server] Commands:\n"
            << "  l / list              — list all adapters (available + running)\n"
            << "  load <name> [json]    — load and start an adapter\n"
            << "  unload <name> / u     — stop and unload an adapter\n"
            << "  rescan                — re-scan adapters directory for new DLLs\n"
            << "  q / quit              — shutdown server\n";
        return true;
    };

    handlers["quit"] = [](AdapterManager&, const std::string&) {
        return false;
    };

    return handlers;
}

void runConsole(AdapterManager& manager) {
    auto handlers = buildCommandHandlers();

    std::string line;
    while(g_running && std::getline(std::cin, line)) {
        trimRight(line);
        if(line.empty()) continue;

        // Parse command and arguments
        auto space_pos = line.find(' ');
        std::string cmd = (space_pos == std::string::npos) ? line : line.substr(0, space_pos);
        std::string args = (space_pos == std::string::npos) ? "" : line.substr(space_pos + 1);

        // Resolve alias
        auto alias_it = command_aliases.find(cmd);
        if(alias_it != command_aliases.end()) {
            cmd = alias_it->second;
        }

        // Dispatch
        auto handler_it = handlers.find(cmd);
        if(handler_it != handlers.end()) {
            if(!handler_it->second(manager, args)) break;
        } else {
            std::cout << "[Server] Unknown command: '" << line << "'. Type 'help' for commands.\n";
        }
    }
}

// ============================================================================
// Server mode — adapter-based with interactive console
// ============================================================================

int runServer(const common_config& cfg) {
    auto rel = [&](const auto& p) { return relPath(p, cfg.exe_dir); };

    std::cout << "[Server] Build config:\n"
        << "  engine_lib:       " << rel(cfg.build_config.engine_lib_path) << "\n"
        << "  engine_include:   " << rel(cfg.build_config.engine_include_path) << "\n"
        << "  parallel_lib:     " << rel(cfg.build_config.parallel_lib_path) << "\n"
        << "  parallel_include: " << rel(cfg.build_config.parallel_include_path) << "\n"
        << "  cmake:            " << cfg.build_config.cmake_executable << "\n"
        << "  generator:        " << cfg.build_config.generator << "\n"
        << "[Server] Adapters dir: " << rel(cfg.adapters_dir) << "\n"
        << "[Server] Config dir:   " << rel(cfg.config_dir) << "\n";

    // Read server config (env var overrides file)
    auto server_config = config::ServerConfig::load(cfg.config_dir / "server.json");

    auto build_config = cfg.build_config;
    if(server_config.correctnessWorkers) {
        build_config.correctness_workers = *server_config.correctnessWorkers;
    }
    std::string cw = config::getEnv("CORRECTNESS_WORKERS", "");
    if(!cw.empty()) {
        try { build_config.correctness_workers = std::stoi(cw); } catch(...) {}
    }

    std::cout << "[Server] Correctness workers: " << build_config.correctness_workers << "\n";

    TestRunnerService runner(build_config);

    AdapterManager manager(runner, cfg.adapters_dir, cfg.config_dir, cfg.exe_dir);

    for(const auto& name : server_config.defaultAdapters) {
        nlohmann::json adapter_config = config::readJsonFile(cfg.config_dir / (name + ".json"));
        std::cout << "[Server] Auto-loading adapter: " << name << "\n";
        if(!manager.load(name, adapter_config)) {
            std::cerr << "[Server] Warning: failed to auto-load '" << name << "'\n";
        }
    }

    std::cout << "[Server] Running. Type 'help' for commands, 'q' to quit.\n";
    runConsole(manager);

    std::cout << "\n[Server] Shutting down...\n";
    manager.stopAll();
    std::cout << "[Server] Stopped.\n";
    return 0;
}

} // anonymous namespace

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    #ifdef _WIN32
    SetConsoleCtrlHandler(consoleHandler, TRUE);
    #else
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    #endif

    fs::path exe_path = fs::weakly_canonical(fs::path(argv[0]));
    auto cfg = setupCommon(exe_path);

    // CLI mode if any arguments are passed, server mode otherwise
    if(argc > 1) {
        return runCli(cfg, argc, argv);
    }
    return runServer(cfg);
}
