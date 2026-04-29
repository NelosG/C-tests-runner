/**
 * @file cli.cpp
 * @brief Standalone CLI test runner - single or multi-solution test runs, no adapters.
 *
 * Supports both local paths and git URLs for test/solution sources.
 * Mode is determined by assignment config.json in the test project.
 *
 * Usage:
 *   cli --test-dir <dir> --test-id <id> --solution <dir> [--solution <dir2> ...]
 *   cli --test-url <url> --test-id <id> --solution-url <url> [--branch main] [--token <pat>]
 */

#include <cpu_isolator.h>
#include <main_common.h>
#include <resource_manager.h>
#include <sandbox_launcher.h>
#include <test_runner_service.h>
#include <api_types.h>
#include <fstream>
#include <future>
#include <iostream>
#include <nlohmann/json.hpp>


namespace {

    void print_help(const char* exe) {
        std::cout << "Usage: " << exe << " [options]\n"
            << "\nLocal sources (at least one required):\n"
            << "  --test-dir <dir>         Path to test directory\n"
            << "  --solution <dir>         Path to student solution (repeatable)\n"
            << "\nGit sources (alternative to local):\n"
            << "  --test-url <url>         Git URL for test project\n"
            << "  --solution-url <url>     Git URL for student solution (repeatable)\n"
            << "  --branch <branch>        Git branch (default: main)\n"
            << "  --token <token>          Git auth token (PAT)\n"
            << "\nCommon:\n"
            << "  --test-id <id>           Test identifier (required)\n"
            << "  --threads <N>            Number of threads (default: 4)\n"
            << "  --memory-limit <MB>      Memory limit per test (default: 1024)\n"
            << "  --numa-node <N>          NUMA node, -1=auto (default: -1)\n"
            << "  --wall-time <sec>        Wall-clock limit per test (default: 60)\n"
            << "  --cpu-time <sec>         CPU time limit per test (default: 30)\n"
            << "  --output <file>          Output JSON file (default: result/output.json)\n";
    }

    int run_cli(const common_config& cfg, int argc, char** argv) {
        // Local sources
        std::vector<std::string> solution_dirs;
        std::string test_dir;

        // Git sources
        std::vector<std::string> solution_urls;
        std::string test_url;
        std::string branch = "main";
        std::string token;

        // Common
        std::string test_id;
        int threads = 4;
        long long memory_limit_mb = 1024;
        int numa_node = -1;
        int wall_time_sec = 60;
        int cpu_time_sec = 30;
        std::string output_file = "result/output.json";

        for(int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if(arg == "--help" || arg == "-h") { print_help(argv[0]); return 0; }
            if(i + 1 >= argc) {
                std::cerr << "Missing value for " << arg << "\n";
                return 1;
            }
            if(arg == "--solution") solution_dirs.push_back(argv[++i]);
            else if(arg == "--test-dir") test_dir = argv[++i];
            else if(arg == "--solution-url") solution_urls.push_back(argv[++i]);
            else if(arg == "--test-url") test_url = argv[++i];
            else if(arg == "--branch") branch = argv[++i];
            else if(arg == "--token") token = argv[++i];
            else if(arg == "--test-id") test_id = argv[++i];
            else if(arg == "--threads") {
                try { threads = std::stoi(argv[++i]); } catch(...) {
                    std::cerr << "[CLI] Invalid --threads\n"; return 1;
                }
            } else if(arg == "--memory-limit") {
                try { memory_limit_mb = std::stoll(argv[++i]); } catch(...) {
                    std::cerr << "[CLI] Invalid --memory-limit\n"; return 1;
                }
            } else if(arg == "--numa-node") {
                try { numa_node = std::stoi(argv[++i]); } catch(...) {
                    std::cerr << "[CLI] Invalid --numa-node\n"; return 1;
                }
            } else if(arg == "--wall-time") {
                try { wall_time_sec = std::stoi(argv[++i]); } catch(...) {
                    std::cerr << "[CLI] Invalid --wall-time\n"; return 1;
                }
            } else if(arg == "--cpu-time") {
                try { cpu_time_sec = std::stoi(argv[++i]); } catch(...) {
                    std::cerr << "[CLI] Invalid --cpu-time\n"; return 1;
                }
            } else if(arg == "--output") output_file = argv[++i];
            else { std::cerr << "Unknown option: " << arg << "\n"; return 1; }
        }

        if(test_id.empty()) { std::cerr << "ERROR: --test-id is required\n"; return 1; }

        // Determine source mode: local or git
        bool use_git = !test_url.empty() || !solution_urls.empty();
        bool use_local = !test_dir.empty() || !solution_dirs.empty();

        if(!use_git && !use_local) {
            std::cerr << "ERROR: provide --test-dir + --solution (local) or --test-url + --solution-url (git)\n";
            return 1;
        }
        if(use_git && use_local) {
            std::cerr << "ERROR: cannot mix local (--test-dir/--solution) and git (--test-url/--solution-url)\n";
            return 1;
        }

        if(use_local) {
            if(test_dir.empty()) { std::cerr << "ERROR: --test-dir is required\n"; return 1; }
            if(solution_dirs.empty()) { std::cerr << "ERROR: --solution is required\n"; return 1; }
        } else {
            if(test_url.empty()) { std::cerr << "ERROR: --test-url is required\n"; return 1; }
            if(solution_urls.empty()) { std::cerr << "ERROR: --solution-url is required\n"; return 1; }
        }

        // Set up ResourceManager
        ResourceManager resource_manager(cfg.providers_dir, cfg.config_dir);

        if(use_local) {
            nlohmann::json local_config = {{"baseDirs", nlohmann::json::object()}};
            std::string load_error;
            if(!resource_manager.load("local", local_config, &load_error)) {
                std::cerr << "[CLI] Failed to load 'local' provider: " << load_error << "\n";
                return 1;
            }
        }

        if(use_git) {
            std::string load_error;
            if(!resource_manager.load("git", {}, &load_error)) {
                std::cerr << "[CLI] Failed to load 'git' provider: " << load_error << "\n";
                return 1;
            }
        }

        // Sandbox and CPU config
        SandboxLauncher::Config sandbox_config;
        CpuIsolator::Config cpu_config;
        cpu_config.numa_node = numa_node;

        auto build_config = cfg.build_config;
        build_config.default_memory_limit_mb = memory_limit_mb;
        TestRunnerService runner(build_config, sandbox_config, cpu_config, resource_manager);

        // Build solution list (local dirs or git URLs)
        struct SolutionEntry { std::string label; std::string source_type; nlohmann::json source; };
        std::vector<SolutionEntry> solutions;

        if(use_local) {
            for(const auto& dir : solution_dirs) {
                std::string label = fs::path(dir).filename().string();
                for(char& c : label) {
                    if(!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') c = '_';
                }
                solutions.push_back({label, "local", {{"path", dir}}});
            }
        } else {
            for(const auto& url : solution_urls) {
                auto pos = url.rfind('/');
                std::string label = (pos != std::string::npos) ? url.substr(pos + 1) : url;
                if(label.size() > 4 && label.substr(label.size() - 4) == ".git")
                    label.resize(label.size() - 4);
                for(char& c : label) {
                    if(!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') c = '_';
                }
                nlohmann::json src = {{"url", url}, {"branch", branch}};
                if(!token.empty()) src["token"] = token;
                solutions.push_back({label, "git", src});
            }
        }

        // Test source
        std::string test_source_type = use_local ? "local" : "git";
        nlohmann::json test_source;
        if(use_local) {
            test_source = {{"path", test_dir}};
        } else {
            test_source = {{"url", test_url}, {"branch", branch}};
            if(!token.empty()) test_source["token"] = token;
        }

        // Run each solution
        nlohmann::json all_reports = nlohmann::json::array();
        int failed_count = 0;

        for(size_t s = 0; s < solutions.size(); ++s) {
            const auto& sol = solutions[s];
            std::string job_id = "cli-" + std::to_string(s) + "-" + sol.label;

            if(solutions.size() > 1) {
                std::cout << "\n[CLI] ========== Solution " << (s + 1) << "/"
                    << solutions.size() << ": " << sol.label << " ==========\n";
            }

            std::cout << "[CLI] Running tests...\n";

            nlohmann::json request = {
                {"jobId", job_id},
                {"testId", test_id},
                {"solutionSourceType", sol.source_type},
                {"solutionSource", sol.source},
                {"testSourceType", test_source_type},
                {"testSource", test_source},
                {"threads", threads},
                {"memoryLimitMb", memory_limit_mb},
                {"wallTimeSec", wall_time_sec},
                {"cpuTimeSec", cpu_time_sec}
            };

            std::promise<nlohmann::json> promise;
            auto future = promise.get_future();
            runner.submit(std::move(request),
                [&promise](const nlohmann::json& result) { promise.set_value(result); });
            auto report = future.get();

            if(report.value("status", "") == "failed") {
                std::cerr << "[CLI] Error: " << report.value("error", "unknown") << "\n";
                ++failed_count;
            }

            all_reports.push_back(std::move(report));
        }

        // Write output
        nlohmann::json output = (all_reports.size() == 1) ? all_reports[0] : all_reports;

        fs::path out_path(output_file);
        if(out_path.has_parent_path()) fs::create_directories(out_path.parent_path());

        std::ofstream out(output_file);
        if(!out.is_open()) {
            std::cerr << "[CLI] Cannot write to " << output_file << "\n";
            std::cout << output.dump(4) << "\n";
            return 1;
        }
        out << output.dump(4) << "\n";
        std::cout << "[CLI] Results written to " << output_file << "\n";

        if(solutions.size() > 1) {
            std::cout << "[CLI] Summary: " << (solutions.size() - failed_count)
                << "/" << solutions.size() << " solutions passed\n";
        }

        return failed_count > 0 ? 1 : 0;
    }

} // anonymous namespace

int main(int argc, char** argv) {
    #ifdef _WIN32
    SetConsoleCtrlHandler(console_handler, TRUE);
    #else
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    #endif

    fs::path exe_path = fs::weakly_canonical(fs::path(argv[0]));
    auto cfg = setup_common(exe_path);
    return run_cli(cfg, argc, argv);
}
