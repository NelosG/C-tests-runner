#pragma once

/**
 * @file progress_callback.h
 * @brief Optional progress reporting callback threaded through the pipeline.
 *
 * Adapters build a callback that publishes events to their transport
 * (AMQP test.direct/progress, HTTP POST /api/callback/progress) and pass it
 * to TestRunnerService::submit. The pipeline + sandbox executor invoke the
 * callback at phase transitions and around each test invocation.
 *
 * Semantics: best-effort. The callback MUST NOT throw and MUST NOT block.
 * AMQP path posts to the event loop (non-blocking); HTTP path pushes to a
 * dedicated worker thread with a bounded queue (drops oldest on overflow).
 *
 * Event JSON shapes (camelCase, ISO8601 timestamps):
 *
 * Phase-level:
 *   { "jobId":"j-...", "nodeId":"runner-...", "phase":"buildRunner",
 *     "message"?:"...", "progress"?:0.0..1.0, "timestamp":"...Z" }
 *
 * Per-test (phase = "test"):
 *   { "jobId":"j-...", "nodeId":"runner-...", "phase":"test",
 *     "scenario":"Correctness.Basic", "test":"sorted_small",
 *     "threadCount":4, "status":"running"|"passed"|"failed",
 *     "timeMs"?:123.4, "message"?:"...", "timestamp":"...Z" }
 *
 * Phase names match pipeline[].step in TaskResult.
 */

#include <functional>
#include <nlohmann/json.hpp>


namespace progress {
    using callback = std::function<void(const nlohmann::json& event)>;
}
