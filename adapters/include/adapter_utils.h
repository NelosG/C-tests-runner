#pragma once

/**
 * @file adapter_utils.h
 * @brief Shared utility functions for adapter implementations.
 *
 * Eliminates duplication of node status building and adapter filtering
 * between HTTP and RabbitMQ adapters.
 */

#include <adapter_api.h>
#include <test_runner_service.h>
#include <time_utils.h>
#include <thread>
#include <nlohmann/json.hpp>

namespace adapter_utils {

/// Build a node status JSON object with queue status and capabilities.
inline nlohmann::json buildNodeStatus(
    const TestRunnerService& runner,
    const std::string& node_id,
    const std::string& transport = ""
) {
    auto queue_status = runner.getQueueStatus();
    nlohmann::json status = {
        {"nodeId", node_id},
        {"capabilities", {
            {"maxConcurrentCorrectness", queue_status.value("maxCorrectnessWorkers", 0)},
            {"maxOmpThreads", static_cast<int>(std::thread::hardware_concurrency())}
        }},
        {"currentLoad", queue_status}
    };
    if(!transport.empty()) {
        status["transport"] = transport;
    }
    return status;
}

/// Filter adapter list to only those with status "available".
inline nlohmann::json filterAvailableAdapters(const ManagementAPI* management) {
    if(!management) return nlohmann::json::array();

    const char* json_str = management->list_adapters(management->context);
    if(!json_str) return nlohmann::json::array();
    auto all = nlohmann::json::parse(json_str);
    management->free_string(management->context, json_str);

    auto available = nlohmann::json::array();
    for(auto& entry : all) {
        if(entry.value("status", "") == "available") {
            available.push_back(entry);
        }
    }
    return available;
}

/// Build a unified completion result JSON from raw test runner output.
inline nlohmann::json buildCompletionResult(
    const nlohmann::json& raw_result,
    const std::string& job_id,
    const std::string& node_id,
    int64_t duration_ms
) {
    nlohmann::json msg;
    if(raw_result.value("status", "") == "failed") {
        msg["status"] = "failed";
        msg["error"] = raw_result.value("error", "unknown error");
    } else {
        msg["status"] = "completed";
    }
    msg["jobId"] = job_id;
    msg["nodeId"] = node_id;
    msg["durationMs"] = duration_ms;
    msg["timestamp"] = nowISO8601();
    msg["result"] = raw_result;
    return msg;
}

/// Build a unified job info JSON from a JobQueue::JobInfo snapshot.
inline nlohmann::json buildJobInfoJson(const JobQueue::JobInfo& info) {
    nlohmann::json json;
    json["jobId"] = info.job_id;
    json["status"] = JobQueue::statusToString(info.status);

    if(info.status == JobQueue::JobStatus::QUEUED)
        json["position"] = info.queue_position;
    if(info.status == JobQueue::JobStatus::COMPLETED && !info.result.is_null())
        json["result"] = info.result;
    if(info.status == JobQueue::JobStatus::FAILED && !info.error.empty())
        json["error"] = info.error;

    return json;
}

} // namespace adapter_utils
