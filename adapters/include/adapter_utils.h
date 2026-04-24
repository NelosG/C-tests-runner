#pragma once

/**
 * @file adapter_utils.h
 * @brief Shared utility functions for adapter implementations.
 *
 * Eliminates duplication of node status building and adapter filtering
 * between HTTP and RabbitMQ adapters.
 */

#include <adapter_api.h>
#include <adapter_status.h>
#include <api_types.h>
#include <node_event_type.h>
#include <request_helpers.h>
#include <test_runner_service.h>
#include <time_utils.h>
#include <filesystem>
#include <thread>
#include <nlohmann/json.hpp>


namespace adapter_utils {

    /// Remove sensitive keys from a config JSON copy.
    inline nlohmann::json sanitize_config(nlohmann::json config) {
        for(const auto& key : {"password", "apiKey", "authToken"}) {
            config.erase(key);
        }
        return config;
    }

    /// Map adapter canonical name (from DLL filename) to transport type label.
    inline std::string adapter_to_transport_type(const std::string& name) {
        if(name == "rabbit") return "amqp";
        return name;
    }

    /// Build capabilities from a pre-fetched queue status snapshot.
    /// `maxThreads` is the upper bound `validate_run_request` accepts for the
    /// `threads` field of any framework, currently hardware_concurrency() * 2.
    inline nlohmann::json build_capabilities(const nlohmann::json& queue_status) {
        const int hw = static_cast<int>(std::thread::hardware_concurrency());
        return {
            {"maxConcurrentCorrectness", queue_status.value("maxCorrectnessWorkers", 0)},
            {"maxThreads", hw * 2}
        };
    }

    /// Snapshot of the engine's per-job defaults + tuning knobs. Sent inside
    /// `info` events so the orchestrator can read what the engine actually
    /// uses (especially when engine started with non-default server.json).
    /// Mirrors the fields accepted by apply_config().
    inline nlohmann::json build_engine_config(const TestRunnerService& runner) {
        // Live atomic snapshot - values reflect any in-flight `updateConfig`.
        auto snap = runner.engine_config_snapshot();
        return {
            {"maxCorrectnessWorkers", runner.build_service_config().correctness_workers},
            {"jobRetentionSeconds", runner.job_retention_seconds()},
            {"defaultMemoryLimitMb", snap.default_memory_limit_mb},
            {"defaultThreads", snap.default_threads},
            {"defaultWallTimeSec", snap.default_wall_time_sec},
            {"defaultCpuTimeSec", snap.default_cpu_time_sec},
            {"sandboxProcessMultiplier", snap.sandbox_process_multiplier}
        };
    }


    namespace detail {
        /// Fetch a JSON array via the ManagementAPI, RAII-freeing the C string.
        /// Returns an empty array on null input or parse failure.
        using management_fn = const char* (*)(void*);

        inline nlohmann::json fetch_management_json(const ManagementAPI* management, management_fn fn) {
            if(!management) return nlohmann::json::array();
            const char* json_str = fn(management->context);
            if(!json_str) return nlohmann::json::array();
            struct FreeGuard {
                const ManagementAPI* m;
                const char* s;
                ~FreeGuard() { m->free_string(m->context, s); }
            } guard{management, json_str};
            auto parsed = nlohmann::json::parse(json_str, nullptr, false);
            if(parsed.is_discarded() || !parsed.is_array()) return nlohmann::json::array();
            return parsed;
        }
    }


    /// Build transports array from all adapters (running + available).
    /// @param sanitize  true -> strip sensitive keys (password, apiKey, authToken);
    ///                  false -> include full config (for registration / online events).
    inline nlohmann::json build_transports_list(const ManagementAPI* management, bool sanitize = true) {
        auto all = detail::fetch_management_json(management, management ? management->list_adapters : nullptr);
        auto transports = nlohmann::json::array();
        for(auto& entry : all) {
            if(entry.value("status", "") != to_string(adapter_status::running))
                continue;
            nlohmann::json t;
            t["type"] = adapter_to_transport_type(entry.value("name", ""));
            t["status"] = entry.value("status", "");
            if(entry.contains("config"))
                t["config"] = sanitize ? sanitize_config(entry["config"]) : entry["config"];
            transports.push_back(t);
        }
        return transports;
    }

    /// Build resourceProviders array (running + available), sensitive keys stripped.
    inline nlohmann::json build_resource_providers_list(const ManagementAPI* management) {
        auto all = detail::fetch_management_json(
            management,
            management ? management->list_resource_providers : nullptr
        );
        auto providers = nlohmann::json::array();
        for(auto& entry : all) {
            nlohmann::json p;
            p["name"] = entry.value("name", "");
            p["status"] = entry.value("status", "");
            if(entry.contains("config"))
                p["config"] = sanitize_config(entry["config"]);
            providers.push_back(p);
        }
        return providers;
    }

    /// Filter adapter list to only those with status "available".
    inline nlohmann::json filter_available_adapters(const ManagementAPI* management) {
        auto all = detail::fetch_management_json(management, management ? management->list_adapters : nullptr);
        auto available = nlohmann::json::array();
        for(auto& entry : all) {
            if(entry.value("status", "") == to_string(adapter_status::available))
                available.push_back(entry);
        }
        return available;
    }

    /// Validate run request fields. Returns {true, ""} on success or {false, error_msg}.
    /// Shared between HTTP and RabbitMQ adapters to avoid copy-paste.
    inline std::pair<bool, std::string> validate_run_request(const nlohmann::json& json) {
        if(!json.contains("testId") || !json["testId"].is_string() || json["testId"].get<std::string>().empty())
            return {false, "testId must be a non-empty string"};

        if(!json.contains("solutionSourceType") || !json.contains("solutionSource"))
            return {false, "Missing required: solutionSourceType + solutionSource"};
        if(!json.contains("testSourceType") || !json.contains("testSource"))
            return {false, "Missing required: testSourceType + testSource"};

        if(json.contains("jobId")) {
            if(!json["jobId"].is_string())
                return {false, "jobId must be a string"};
            std::string jid = json["jobId"].get<std::string>();
            bool safe = !jid.empty() && std::all_of(
                jid.begin(),
                jid.end(),
                [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_'; }
            );
            if(!safe) return {false, "Invalid jobId: must contain only alphanumeric, hyphen, underscore"};
        }

        if(json.contains("threads")) {
            auto& v = json["threads"];
            if(!v.is_number_integer())
                return {false, "threads must be an integer"};
            int threads = v.get<int>();
            int max_hw = static_cast<int>(std::thread::hardware_concurrency()) * 2;
            if(threads < 1 || threads > max_hw)
                return {
                    false,
                    "Invalid threads: " + std::to_string(threads) + ". Must be 1.." + std::to_string(max_hw)
                };
        }

        if(json.contains("memoryLimitMb")) {
            auto& v = json["memoryLimitMb"];
            if(!v.is_number_integer() || v.get<long long>() < 0)
                return {false, "memoryLimitMb must be non-negative integer"};
        }

        if(json.contains("wallTimeSec")) {
            auto& v = json["wallTimeSec"];
            if(!v.is_number_integer() || v.get<int>() < 1)
                return {false, "wallTimeSec must be a positive integer"};
        }

        if(json.contains("cpuTimeSec")) {
            auto& v = json["cpuTimeSec"];
            if(!v.is_number_integer() || v.get<int>() < 1)
                return {false, "cpuTimeSec must be a positive integer"};
        }

        if(json.contains("maxProcesses")) {
            auto& v = json["maxProcesses"];
            if(!v.is_number_integer() || v.get<int>() < 1)
                return {false, "maxProcesses must be a positive integer"};
        }

        return {true, ""};
    }

    /// Validate and apply engine config fields. Returns {true, ""} on success or {false, error_msg}.
    /// Validates all fields first, applies only if all are valid (atomic semantics).
    inline std::pair<bool, std::string> apply_config(
        TestRunnerService& runner,
        const nlohmann::json& cfg
    ) {
        // Validate all fields before applying any
        if(cfg.contains("maxCorrectnessWorkers")) {
            auto& v = cfg["maxCorrectnessWorkers"];
            if(!v.is_number_integer()) return {false, "maxCorrectnessWorkers must be an integer"};
            if(v.get<int>() < 1) return {false, "maxCorrectnessWorkers must be >= 1"};
        }
        if(cfg.contains("jobRetentionSeconds")) {
            auto& v = cfg["jobRetentionSeconds"];
            if(!v.is_number_integer()) return {false, "jobRetentionSeconds must be an integer"};
            if(v.get<int>() < 1) return {false, "jobRetentionSeconds must be >= 1"};
        }
        if(cfg.contains("defaultMemoryLimitMb")) {
            auto& v = cfg["defaultMemoryLimitMb"];
            if(!v.is_number_integer()) return {false, "defaultMemoryLimitMb must be integer"};
            long long mb = v.get<long long>();
            if(mb < 0) return {false, "defaultMemoryLimitMb must be >= 0"};
        }
        for(const char* key : {
                "defaultThreads",
                "defaultWallTimeSec",
                "defaultCpuTimeSec",
                "sandboxProcessMultiplier"
            }) {
            if(cfg.contains(key)) {
                auto& v = cfg[key];
                if(!v.is_number_integer() || v.get<int>() < 1)
                    return {false, std::string(key) + " must be a positive integer"};
            }
        }
        // Apply
        if(cfg.contains("maxCorrectnessWorkers"))
            runner.set_max_correctness_workers(cfg["maxCorrectnessWorkers"].get<int>());
        if(cfg.contains("jobRetentionSeconds"))
            runner.set_job_retention_seconds(cfg["jobRetentionSeconds"].get<int>());
        if(cfg.contains("defaultMemoryLimitMb"))
            runner.set_default_memory_limit_mb(cfg["defaultMemoryLimitMb"].get<long long>());
        if(cfg.contains("defaultThreads"))
            runner.set_default_threads(cfg["defaultThreads"].get<int>());
        if(cfg.contains("defaultWallTimeSec"))
            runner.set_default_wall_time_sec(cfg["defaultWallTimeSec"].get<int>());
        if(cfg.contains("defaultCpuTimeSec"))
            runner.set_default_cpu_time_sec(cfg["defaultCpuTimeSec"].get<int>());
        if(cfg.contains("sandboxProcessMultiplier"))
            runner.set_sandbox_process_multiplier(cfg["sandboxProcessMultiplier"].get<int>());
        return {true, ""};
    }

    /// Build a node lifecycle / status event JSON.
    ///   offline -> minimal: type, nodeId, timestamp
    ///   online  -> + capabilities, transports (full config incl. authToken), resourceProviders
    ///   info    -> + capabilities, transports (sanitized), resourceProviders, currentLoad
    inline nlohmann::json build_node_event(
        node_event_type type,
        const std::string& node_id,
        const TestRunnerService& runner,
        const ManagementAPI* management
    ) {
        nlohmann::json event;
        event["type"] = to_string(type);
        event["nodeId"] = node_id;
        event["timestamp"] = now_iso8601();

        if(type == node_event_type::offline) return event;

        // online events expose full config (orchestrator needs authToken/port);
        // info (status query) sanitizes secrets.
        bool sanitize = (type != node_event_type::online);

        auto queue_status = runner.get_queue_status();
        event["capabilities"] = build_capabilities(queue_status);
        event["transports"] = build_transports_list(management, sanitize);
        event["resourceProviders"] = build_resource_providers_list(management);

        if(type == node_event_type::info) {
            event["currentLoad"] = queue_status;
            event["engineConfig"] = build_engine_config(runner);
        }

        return event;
    }

    /// Build a unified completion result JSON by flattening raw test runner output.
    inline nlohmann::json build_completion_result(
        const nlohmann::json& raw_result,
        const std::string& job_id,
        const std::string& node_id,
        int64_t duration_ms
    ) {
        // Start from the raw result (contains: jobId, solution, mode, status,
        // correctness[], performance[], buildInfo, error, buildOutput)
        nlohmann::json msg = raw_result;

        // Ensure adapter-level metadata is present (overwrites if already set)
        msg["jobId"] = job_id;
        msg["nodeId"] = node_id;
        msg["durationMs"] = duration_ms;
        msg["timestamp"] = now_iso8601();

        return msg;
    }

    /// Build a unified job info JSON from a JobQueue::JobInfo snapshot.
    inline nlohmann::json build_job_info_json(const JobQueue::JobInfo& info) {
        nlohmann::json json;
        json["jobId"] = info.job_id;
        json["status"] = to_string(info.status);

        if(!info.request.is_null()) {
            std::string solution = request_helpers::extract_solution_name(info.request);
            if(!solution.empty()) json["solution"] = solution;
        }

        if(info.status == job_status::queued)
            json["position"] = info.queue_position;
        if(info.status == job_status::completed && !info.result.is_null())
            json["result"] = info.result;
        if(info.status == job_status::failed && !info.error.empty())
            json["error"] = info.error;

        return json;
    }

} // namespace adapter_utils
