/**
 * @file server.cpp
 * @brief Adapter-based server entry point.
 *
 * Usage:
 *   server [--node-id <id>]
 *
 * nodeId is required — either via --node-id arg or 'nodeId' in config/server.json.
 * CLI args override config file values.
 *
 * Resource providers (git, local) are loaded from output-Dir/resource_providers/
 * and configured via config/resource-{name}.json.
 */

#include <adapter_manager.h>
#include <main_common.h>
#include <resource_manager.h>
#include <test_runner_service.h>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>


namespace {

    void trimRight(std::string& s) {
        while(!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' '))
            s.pop_back();
    }

    std::string relPath(const fs::path& p, const fs::path& base) {
        if(base.empty() || p.empty()) return p.generic_string();
        try { return fs::proximate(p, base).generic_string(); } catch(...) { return p.generic_string(); }
    }

    // ============================================================================
    // Interactive console
    // ============================================================================

    using command_handler = std::function<bool(AdapterManager &, const std::string &)>;

    const std::unordered_map<std::string, std::string> command_aliases = {
        {"l", "list"},
        {"u", "unload"},
        {"h", "help"},
        {"q", "quit"},
    };

    std::unordered_map<std::string, command_handler> buildCommandHandlers() {
        std::unordered_map<std::string, command_handler> handlers;

        handlers["list"] = [](AdapterManager& mgr, const std::string&) {
            std::cout << "[Server] Adapters:\n" << mgr.list().dump(2) << "\n";
            return true;
        };

        handlers["load"] = [](AdapterManager& mgr, const std::string& args) {
            std::string rest = args;
            while(!rest.empty() && rest[0] == ' ') rest.erase(rest.begin());
            auto sp = rest.find(' ');
            std::string name = (sp == std::string::npos) ? rest : rest.substr(0, sp);
            nlohmann::json lc;
            if(sp != std::string::npos) {
                std::string js = rest.substr(sp + 1);
                while(!js.empty() && js[0] == ' ') js.erase(js.begin());
                if(!js.empty()) {
                    try { lc = nlohmann::json::parse(js); } catch(const std::exception& e) {
                        std::cerr << "[Server] Invalid JSON: " << e.what() << "\n";
                        return true;
                    }
                }
            }
            if(name.empty()) {
                std::cout << "[Server] Usage: load <adapter_name> [json_config]\n";
            } else {
                std::cout << "[Server] Loading adapter '" << name << "'...\n";
                if(mgr.load(name, lc))
                    std::cout << "[Server] '" << name << "' loaded successfully\n";
                else
                    std::cerr << "[Server] Failed to load '" << name << "'\n";
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
                if(mgr.unload(rest))
                    std::cout << "[Server] '" << rest << "' unloaded\n";
                else
                    std::cerr << "[Server] '" << rest << "' not found or not running\n";
            }
            return true;
        };

        handlers["rescan"] = [](AdapterManager& mgr, const std::string&) {
            mgr.rescan();
            std::cout << "[Server] Adapter directory rescanned\n";
            std::cout << mgr.list().dump(2) << "\n";
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

        handlers["quit"] = [](AdapterManager&, const std::string&) { return false; };

        return handlers;
    }

    void runConsole(AdapterManager& manager) {
        auto handlers = buildCommandHandlers();
        std::string line;
        while(g_running&& std::getline(std::cin, line)) {
            trimRight(line);
            if(line.empty()) continue;
            auto sp = line.find(' ');
            auto cmd = (sp == std::string::npos) ? line : line.substr(0, sp);
            auto args = (sp == std::string::npos) ? std::string{} : line.substr(sp + 1);
            auto alias_it = command_aliases.find(cmd);
            if(alias_it != command_aliases.end()) cmd = alias_it->second;
            auto handler_it = handlers.find(cmd);
            if(handler_it != handlers.end()) {
                if(!handler_it->second(manager, args)) break;
            } else {
                std::cout << "[Server] Unknown command: '" << line << "'. Type 'help'.\n";
            }
        }
    }

    // ============================================================================
    // Server mode
    // ============================================================================

    int runServer(const common_config& cfg, const std::string& node_id_arg) {
        auto rel = [&](const auto& p) { return relPath(p, cfg.exe_dir); };

        auto server_config = config::ServerConfig::load(cfg.config_dir / "server.json");

        // Resolve nodeId: arg overrides config
        std::string node_id = node_id_arg.empty()
            ? server_config.nodeId.value_or("")
            : node_id_arg;
        if(node_id.empty()) {
            std::cerr << "[Server] ERROR: node ID is required.\n"
                << "  Use --node-id <id> or set 'nodeId' in config/server.json\n";
            return 1;
        }

        auto build_config = cfg.build_config;
        if(server_config.correctnessWorkers)
            build_config.correctness_workers = *server_config.correctnessWorkers;
        build_config.default_memory_limit_mb = server_config.defaultMemoryLimitMb;
        std::string cw = config::getEnv("CORRECTNESS_WORKERS", "");
        if(!cw.empty()) { try { build_config.correctness_workers = std::stoi(cw); } catch(...) {} }

        std::cout << "[Server] node_id:             " << node_id << "\n"
            << "[Server] engine_lib:          " << rel(build_config.engine_lib_path) << "\n"
            << "[Server] parallel_lib:        " << rel(build_config.parallel_lib_path) << "\n"
            << "[Server] cmake:               " << build_config.cmake_executable << "\n"
            << "[Server] correctness workers: " << build_config.correctness_workers << "\n"
            << "[Server] adapters_dir:        " << rel(cfg.adapters_dir) << "\n"
            << "[Server] config_dir:          " << rel(cfg.config_dir) << "\n"
            << "[Server] providers_dir:       " << rel(cfg.providers_dir) << "\n";

        // Create ResourceManager and load default providers
        ResourceManager resource_manager(cfg.providers_dir, cfg.config_dir);
        for(const auto& name : server_config.defaultResourceProviders) {
            std::cout << "[Server] Auto-loading resource provider: " << name << "\n";
            std::string load_error;
            if(!resource_manager.load(name, {}, &load_error)) {
                std::cerr << "[Server] Warning: failed to auto-load provider '" << name << "'"
                    << (load_error.empty() ? "" : ": " + load_error) << "\n";
            }
        }

        TestRunnerService runner(build_config, resource_manager);
        AdapterManager adapter_manager(
            runner,
            cfg.adapters_dir,
            cfg.config_dir,
            cfg.exe_dir,
            node_id,
            &resource_manager
        );

        for(const auto& name : server_config.defaultAdapters) {
            nlohmann::json adapter_config = config::readJsonFile(cfg.config_dir / (name + ".json"));
            std::cout << "[Server] Auto-loading adapter: " << name << "\n";
            if(!adapter_manager.load(name, adapter_config)) {
                std::cerr << "[Server] Warning: failed to auto-load '" << name << "'\n";
            }
        }

        std::cout << "[Server] Running. Type 'help' for commands, 'q' to quit.\n";
        runConsole(adapter_manager);
        std::cout << "\n[Server] Shutting down...\n";
        adapter_manager.stopAll();
        std::cout << "[Server] Stopped.\n";
        return 0;
    }

} // anonymous namespace

int main(int argc, char** argv) {
    #ifdef _WIN32
    SetConsoleCtrlHandler(consoleHandler, TRUE);
    #else
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    #endif

    fs::path exe_path = fs::weakly_canonical(fs::path(argv[0]));
    auto cfg = setupCommon(exe_path);

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

    return runServer(cfg, node_id_arg);
}