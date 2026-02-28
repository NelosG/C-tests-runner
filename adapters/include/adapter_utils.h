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
#include <test_runner_service.h>
#include <time_utils.h>
#include <filesystem>
#include <thread>
#include <nlohmann/json.hpp>


namespace adapter_utils {

    /// Remove sensitive keys from a config JSON copy.
    inline nlohmann::json sanitizeConfig(nlohmann::json config) {
        for(const auto& key : {"password", "apiKey", "authToken"}) {
            config.erase(key);
        }
        return config;
    }

    /// Map adapter canonical name (from DLL filename) to transport type label.
    inline std::string adapterToTransportType(const std::string& name) {
        if(name == "rabbit") return "amqp";
        return name;
    }

    /// Build capabilities from a pre-fetched queue status snapshot.
    inline nlohmann::json buildCapabilities(const nlohmann::json& queue_status) {
        return {
            {"maxConcurrentCorrectness", queue_status.value("maxCorrectnessWorkers", 0)},
            {"maxOmpThreads", static_cast<int>(std::thread::hardware_concurrency())}
        };
    }

    /// Build capabilities object (extracted to allow use without currentLoad).
    inline nlohmann::json buildCapabilities(const TestRunnerService& runner) {
        return buildCapabilities(runner.getQueueStatus());
    }

    /// Build transports array from all adapters (running + available).
    /// @param sanitize  true → strip sensitive keys (password, apiKey, authToken);
    ///                  false → include full config (for registration / online events).
    inline nlohmann::json buildTransportsList(const ManagementAPI* management, bool sanitize = true) {
        if(!management) return nlohmann::json::array();
        const char* json_str = management->list_adapters(management->context);
        if(!json_str) return nlohmann::json::array();
        struct FreeGuard {
            const ManagementAPI* m;
            const char* s;
            ~FreeGuard() { m->free_string(m->context, s); }
        } guard{management, json_str};
        auto all = nlohmann::json::parse(json_str, nullptr, false);
        if(all.is_discarded()) return nlohmann::json::array();

        auto transports = nlohmann::json::array();
        for(auto& entry : all) {
            nlohmann::json t;
            t["type"] = adapterToTransportType(entry.value("name", ""));
            t["status"] = entry.value("status", "");
            if(entry.contains("config"))
                t["config"] = sanitize ? sanitizeConfig(entry["config"]) : entry["config"];
            transports.push_back(t);
        }
        return transports;
    }

    /// Build resourceProviders array (running + available), sensitive keys stripped.
    inline nlohmann::json buildResourceProvidersList(const ManagementAPI* management) {
        if(!management) return nlohmann::json::array();
        const char* json_str = management->list_resource_providers(management->context);
        if(!json_str) return nlohmann::json::array();
        struct FreeGuard {
            const ManagementAPI* m;
            const char* s;
            ~FreeGuard() { m->free_string(m->context, s); }
        } guard{management, json_str};
        auto all = nlohmann::json::parse(json_str, nullptr, false);
        if(all.is_discarded()) return nlohmann::json::array();

        auto providers = nlohmann::json::array();
        for(auto& entry : all) {
            nlohmann::json p;
            p["name"] = entry.value("name", "");
            p["status"] = entry.value("status", "");
            if(entry.contains("config"))
                p["config"] = sanitizeConfig(entry["config"]);
            providers.push_back(p);
        }
        return providers;
    }

    /// Filter adapter list to only those with status "available".
    inline nlohmann::json filterAvailableAdapters(const ManagementAPI* management) {
        if(!management) return nlohmann::json::array();

        const char* json_str = management->list_adapters(management->context);
        if(!json_str) return nlohmann::json::array();
        struct FreeGuard {
            const ManagementAPI* m;
            const char* s;
            ~FreeGuard() { m->free_string(m->context, s); }
        } guard{management, json_str};
        auto all = nlohmann::json::parse(json_str, nullptr, false);
        if(all.is_discarded()) return nlohmann::json::array();

        auto available = nlohmann::json::array();
        for(auto& entry : all) {
            if(entry.value("status", "") == to_string(adapter_status::available)) {
                available.push_back(entry);
            }
        }
        return available;
    }

    /// Validate and apply engine config fields. Returns {true, ""} on success or {false, error_msg}.
    /// Validates all fields first, applies only if all are valid (atomic semantics).
    inline std::pair<bool, std::string> applyConfig(
        TestRunnerService& runner,
        const nlohmann::json& cfg
    ) {
        // Validate all fields before applying any
        if(cfg.contains("maxCorrectnessWorkers")) {
            int n = cfg["maxCorrectnessWorkers"].get<int>();
            if(n < 1) return {false, "maxCorrectnessWorkers must be >= 1"};
        }
        if(cfg.contains("jobRetentionSeconds")) {
            int sec = cfg["jobRetentionSeconds"].get<int>();
            if(sec < 1) return {false, "jobRetentionSeconds must be >= 1"};
        }
        if(cfg.contains("defaultMemoryLimitMb")) {
            auto& v = cfg["defaultMemoryLimitMb"];
            if(!v.is_number_integer()) return {false, "defaultMemoryLimitMb must be integer"};
            long long mb = v.get<long long>();
            if(mb < 0) return {false, "defaultMemoryLimitMb must be >= 0"};
        }
        // Apply
        if(cfg.contains("maxCorrectnessWorkers"))
            runner.setMaxCorrectnessWorkers(cfg["maxCorrectnessWorkers"].get<int>());
        if(cfg.contains("jobRetentionSeconds"))
            runner.setJobRetentionSeconds(cfg["jobRetentionSeconds"].get<int>());
        if(cfg.contains("defaultMemoryLimitMb"))
            runner.set_default_memory_limit_mb(cfg["defaultMemoryLimitMb"].get<long long>());
        return {true, ""};
    }

    /// Build a node lifecycle / status event JSON.
    ///   offline → minimal: type, nodeId, timestamp
    ///   online  → + capabilities, transports (full config incl. authToken), resourceProviders
    ///   info    → + capabilities, transports (sanitized), resourceProviders, currentLoad
    inline nlohmann::json buildNodeEvent(
        node_event_type type,
        const std::string& node_id,
        const TestRunnerService& runner,
        const ManagementAPI* management
    ) {
        nlohmann::json event;
        event["type"]      = to_string(type);
        event["nodeId"]    = node_id;
        event["timestamp"] = nowISO8601();

        if(type == node_event_type::offline) return event;

        // online events expose full config (orchestrator needs authToken/port);
        // info (status query) sanitizes secrets.
        bool sanitize = (type != node_event_type::online);

        auto queue_status = runner.getQueueStatus();
        event["capabilities"]      = buildCapabilities(queue_status);
        event["transports"]        = buildTransportsList(management, sanitize);
        event["resourceProviders"] = buildResourceProvidersList(management);

        if(type == node_event_type::info) {
            event["currentLoad"] = queue_status;
        }

        return event;
    }

    /// Build a unified completion result JSON by flattening raw test runner output.
    inline nlohmann::json buildCompletionResult(
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
        msg["timestamp"] = nowISO8601();

        return msg;
    }

    /// Build a unified job info JSON from a JobQueue::JobInfo snapshot.
    inline nlohmann::json buildJobInfoJson(const JobQueue::JobInfo& info) {
        nlohmann::json json;
        json["jobId"] = info.job_id;
        json["status"] = to_string(info.status);

        // Extract context from stored request
        if(!info.request.is_null()) {
            std::string mode = info.request.value("mode", to_string(test_mode::correctness));
            json["mode"] = mode;
            json["lane"] = lane_for_mode(mode);

            std::string sol_dir = info.request.value("solutionDir", "");
            std::string sol_git = info.request.value("solutionGitUrl", "");
            if(!sol_dir.empty())
                json["solution"] = std::filesystem::path(sol_dir).filename().string();
            else if(!sol_git.empty())
                json["solution"] = sol_git;
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
