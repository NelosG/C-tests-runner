/**
 * @file cli.cpp
 * @brief Standalone CLI test runner — single test run, no adapters.
 *
 * Usage:
 *   cli --test-dir <dir> --test-id <id> --solution <dir> [--mode all] [--threads 4]
 *       [--output result/output.json]
 *
 * Source paths are resolved via LocalResourceProvider (loaded from resource_providers/).
 * If the resource_providers directory is missing, falls back to direct local resolution.
 */

#include <main_common.h>
#include <resource_manager.h>
#include <test_runner_service.h>
#include <api_types.h>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>


namespace {

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
                try { threads = std::stoi(argv[++i]); } catch(const std::exception&) {
                    std::cerr << "[CLI] Invalid --threads value: " << argv[i] << "\n";
                    return 1;
                }
            } else if(arg == "--output" && i + 1 < argc) output_file = argv[++i];
            else if(arg == "--help" || arg == "-h") {
                std::cout << "Usage: " << argv[0] << " [options]\n"
                    << "\nRequired:\n"
                    << "  --test-dir <dir>       Path to test directory\n"
                    << "  --test-id <id>         Test identifier (for caching)\n"
                    << "  --solution <dir>       Path to student solution directory\n"
                    << "\nOptions:\n"
                    << "  --mode <mode>          correctness | performance | all (default: all)\n"
                    << "  --threads <N>          Number of threads (default: 4)\n"
                    << "  --output <file>        Output file (default: result/output.json)\n";
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

        // Set up ResourceManager with local provider (no base dirs for CLI — use paths as-is)
        ResourceManager resource_manager(cfg.providers_dir, cfg.config_dir);
        {
            std::string load_error;
            if(!resource_manager.load("local", {}, &load_error)) {
                std::cerr << "[CLI] Warning: failed to load 'local' provider"
                    << (load_error.empty() ? "" : ": " + load_error) << "\n"
                    << "[CLI] Ensure resource_providers/resource_local.dll is built.\n";
                return 1;
            }
        }

        TestRunnerService runner(cfg.build_config, resource_manager);

        auto run_one = [&](const std::string& run_mode) -> nlohmann::json {
            nlohmann::json request = {
                {"jobId", "cli-run"},
                {"testId", test_id},
                {"solutionSourceType", "local"},
                {"solutionSource", nlohmann::json{{"path", solution_dir}}},
                {"testSourceType", "local"},
                {"testSource", nlohmann::json{{"path", test_dir}}},
                {"mode", run_mode},
                {"threads", threads}
            };
            auto noop = [](job_status) {};
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

        fs::path out_path(output_file);
        if(out_path.has_parent_path()) fs::create_directories(out_path.parent_path());

        std::ofstream out(output_file);
        if(!out.is_open()) {
            std::cerr << "[CLI] Cannot write to " << output_file << "\n";
            std::cout << report.dump(4) << "\n";
            return 1;
        }
        out << report.dump(4) << "\n";
        std::cout << "[CLI] Results written to " << output_file << "\n";
        return report.value("status", "") == "failed" ? 1 : 0;
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
    return runCli(cfg, argc, argv);
}