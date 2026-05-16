/**
 * @file server.cpp
 * @brief Adapter-based server entry point.
 *
 * Usage:
 *   server [--node-id <id>]
 *
 * nodeId is required - either via --node-id arg or 'nodeId' in config/server.json.
 * CLI args override config file values.
 *
 * Resource providers (git, local) are loaded from output-Dir/resource_providers/
 * and configured via config/resource-{name}.json.
 */

#include <adapter_manager.h>
#include <thread>
#include <cpu_isolator.h>
#include <main_common.h>
#include <path_utils.h>
#include <resource_manager.h>
#include <sandbox_launcher.h>
#include <test_runner_service.h>
#include <iostream>
#include <string>
#include <nlohmann/json.hpp>


namespace {

    std::string rel_path(const fs::path& p, const fs::path& base) {
        return path_utils::rel(p, base);
    }

    /// Block until the signal handler flips g_running. Management (load/unload
    /// adapter, queue status etc.) is performed via the HTTP/RabbitMQ control
    /// surface, not the console - so the server has no stdin reader and won't
    /// deadlock libc cleanup on detached I/O threads at exit.
    void wait_for_shutdown() {
        std::cout << "[Server] Running. Send SIGTERM/SIGINT to stop.\n";
        while(g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    // ============================================================================
    // Server mode helpers
    // ============================================================================

    /// Resolve nodeId: --node-id arg wins over server.json. Empty result is fatal.
    std::string resolve_node_id(const std::string& arg, const config::ServerConfig& sc) {
        return arg.empty() ? sc.nodeId.value_or("") : arg;
    }

    /// Apply server.json + CORRECTNESS_WORKERS env onto the build_config skeleton.
    void apply_server_config_to_build_config(const config::ServerConfig& sc,
                                         BuildService::BuildConfig& bc) {
        if(sc.correctnessWorkers) bc.correctness_workers = *sc.correctnessWorkers;
        bc.default_memory_limit_mb     = sc.defaultMemoryLimitMb;
        bc.default_threads             = sc.defaultThreads;
        bc.default_wall_time_sec       = sc.defaultWallTimeSec;
        bc.default_cpu_time_sec        = sc.defaultCpuTimeSec;
        bc.sandbox_process_multiplier  = sc.sandboxProcessMultiplier;
        std::string env_cw = config::get_env("CORRECTNESS_WORKERS", "");
        if(!env_cw.empty()) {
            try { bc.correctness_workers = std::stoi(env_cw); } catch(...) {}
        }
    }

    /// Pull sandbox + cpuIsolation sub-objects out of server.json.
    struct RuntimeKnobs {
        SandboxLauncher::Config sandbox;
        CpuIsolator::Config cpu;
    };
    RuntimeKnobs load_runtime_knobs(const fs::path& server_json_path) {
        RuntimeKnobs out;
        auto j = config::read_json_file(server_json_path);
        if(j.contains("sandbox")) {
            const auto& s = j["sandbox"];
            out.sandbox.isolate_path = s.value("isolatePath", "/usr/bin/isolate");
            out.sandbox.max_box_id   = s.value("maxBoxId", 99);
        }
        if(j.contains("cpuIsolation")) {
            const auto& c = j["cpuIsolation"];
            out.cpu.numa_node     = c.value("numaNode", -1);
            out.cpu.infra_reserve = c.value("infraReserve", 1);
        }
        return out;
    }

    void log_server_info(const std::string& node_id, const common_config& cfg,
                       const BuildService::BuildConfig& bc) {
        auto rel = [&](const auto& p) { return rel_path(p, cfg.exe_dir); };
        std::cout << "[Server] node_id:             " << node_id << "\n"
            << "[Server] engine_lib:          " << rel(bc.engine_lib_path) << "\n"
            << "[Server] parallel_lib:        " << rel(bc.parallel_lib_path) << "\n"
            << "[Server] cmake:               " << bc.cmake_executable << "\n"
            << "[Server] correctness workers: " << bc.correctness_workers << "\n"
            << "[Server] adapters_dir:        " << rel(cfg.adapters_dir) << "\n"
            << "[Server] config_dir:          " << rel(cfg.config_dir) << "\n"
            << "[Server] providers_dir:       " << rel(cfg.providers_dir) << "\n";
    }

    /// Per-job framework availability is checked again at submission time.
    /// This is a startup probe only - failure is non-fatal.
    void probe_frameworks(const BuildService::BuildConfig& bc) {
        BuildService probe(bc);
        std::cout << "[Server] Framework availability:\n";
        for(const auto& fw : {"openmp", "parlay", "cilk"}) {
            auto [ok, err] = probe.validate_framework(fw);
            if(ok) std::cout << "  + " << fw << "\n";
            else   std::cout << "  - " << fw << " (" << err << ")\n";
        }
    }

    void bootstrap_resource_providers(ResourceManager& rm,
                                     const std::vector<std::string>& names) {
        for(const auto& name : names) {
            std::cout << "[Server] Auto-loading resource provider: " << name << "\n";
            std::string err;
            if(!rm.load(name, {}, &err)) {
                std::cerr << "[Server] Warning: failed to auto-load provider '" << name << "'"
                    << (err.empty() ? "" : ": " + err) << "\n";
            }
        }
    }

    /// Returns number of successfully loaded adapters.
    int bootstrap_adapters(AdapterManager& am, const std::vector<std::string>& names,
                           const fs::path& config_dir) {
        int loaded = 0;
        for(const auto& name : names) {
            auto cfg_json = config::read_json_file(config_dir / (name + ".json"));
            std::cout << "[Server] Auto-loading adapter: " << name << "\n";
            if(am.load(name, cfg_json)) ++loaded;
            else std::cerr << "[Server] Warning: failed to auto-load '" << name << "'\n";
        }
        return loaded;
    }

    // ============================================================================
    // Server mode entry point
    // ============================================================================

    int run_server(const common_config& cfg, const std::string& node_id_arg) {
        auto server_config = config::ServerConfig::load(cfg.config_dir / "server.json");
        std::string node_id = resolve_node_id(node_id_arg, server_config);
        if(node_id.empty()) {
            std::cerr << "[Server] ERROR: node ID is required.\n"
                << "  Use --node-id <id> or set 'nodeId' in config/server.json\n";
            return 1;
        }

        auto build_config = cfg.build_config;
        apply_server_config_to_build_config(server_config, build_config);

        log_server_info(node_id, cfg, build_config);

        auto knobs = load_runtime_knobs(cfg.config_dir / "server.json");
        probe_frameworks(build_config);

        ResourceManager resource_manager(cfg.providers_dir, cfg.config_dir);
        bootstrap_resource_providers(resource_manager, server_config.defaultResourceProviders);

        TestRunnerService runner(build_config, knobs.sandbox, knobs.cpu, resource_manager);
        runner.set_node_id(node_id);  // embedded in progress events from Pipeline::execute
        AdapterManager adapter_manager(
            runner, cfg.adapters_dir, cfg.config_dir,
            cfg.exe_dir, node_id, &resource_manager
        );

        int adapters_loaded = bootstrap_adapters(adapter_manager,
            server_config.defaultAdapters, cfg.config_dir);
        if(adapters_loaded == 0 && !server_config.defaultAdapters.empty()) {
            std::cerr << "[Server] FATAL: no adapters loaded. Cannot accept jobs. Exiting.\n";
            return 1;
        }

        wait_for_shutdown();
        std::cout << "\n[Server] Shutting down...\n";
        adapter_manager.stop_all();
        std::cout << "[Server] Stopped.\n";
        resource_manager.stop_all();
        std::cout << "[Server] Exit.\n";
        return 0;
    }

} // anonymous namespace

int main(int argc, char** argv) {
    // Line-buffer stdout/stderr so log lines appear in `docker logs` /
    // systemd-journal in real time. Default behaviour for std::cout when
    // attached to a non-TTY (e.g. Docker captures fd 1 as a pipe) is full
    // buffering - log lines would only flush at 4 KiB chunks or on exit.
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);

    #ifdef _WIN32
    SetConsoleCtrlHandler(console_handler, TRUE);
    #else
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    #endif

    fs::path exe_path = fs::weakly_canonical(fs::path(argv[0]));
    auto cfg = setup_common(exe_path);

    std::string node_id_arg;

    for(int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if(arg == "--node-id" && i + 1 < argc) node_id_arg = argv[++i];
        else if(arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                << "\nOptions:\n"
                << "  --node-id <id>    Node identifier (required; overrides server.json)\n";
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            return 1;
        }
    }

    return run_server(cfg, node_id_arg);
}