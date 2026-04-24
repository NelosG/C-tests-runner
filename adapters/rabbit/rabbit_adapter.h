#pragma once

/**
 * @file rabbit_adapter.h
 * @brief RabbitMQ transport adapter - async AMQP-CPP + libuv.
 *
 * Architecture:
 *   Single event loop thread handles ALL AMQP I/O:
 *     - Multiple channels on one connection
 *     - Task consumer (single queue, mode in message body)
 *     - Control listener (exclusive queue on node.fanout)
 *     - Result publishing
 *   Task execution delegated to JobQueue (same as HttpAdapter).
 *
 * Control messages (via node.fanout, RPC with reply_to):
 *   statusRequest           - node status and queue load
 *   queueStatus             - job queue status (lanes, positions, running jobs)
 *   getJobInfo              - query a specific job's status/result
 *   cancelJob               - cancel a queued job by job_id
 *   listAdapters            - available and running adapters
 *   listAvailableAdapters   - only adapters that can be loaded
 *   loadAdapter             - load and start an adapter by name
 *   unloadAdapter           - stop and unload a running adapter
 *   updateConfig            - update engine config (config: {maxCorrectnessWorkers?, jobRetentionSeconds?})
 *
 * Topology:
 *   Exchange: test.direct (direct)
 *     Queue: test.tasks              <- routing keys "correctness", "performance", "all"
 *     Queue: test.results            <- routing key "results"
 *   Exchange: node.fanout (fanout)
 *     Queue: node.events
 *   Exchange: node.control.direct (direct)
 *     Bound to exclusive control queue with routing key = nodeId
 */

#include <adapter_api.h>
#include <adapter_context.h>
#include <amqpcpp.h>
#include <atomic>
#include <chrono>
#include <control_type.h>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <test_execution_adapter.h>
#include <test_runner_service.h>
#include <thread>
#include <unordered_map>
#include <uv.h>
#include <nlohmann/json.hpp>

class UvAmqpHandler;

class RabbitAdapter : public TestExecutionAdapter {
    public:
        struct Config {
            std::string host = "localhost";
            int port = 5672;
            std::string user = "guest";
            std::string password = "guest";
            std::string vhost = "/";
            std::string node_id;
            int connection_timeout_sec = 10;
        };

        RabbitAdapter(TestRunnerService& runner, const ManagementAPI* management, const AdapterContext& ctx);
        ~RabbitAdapter() override;

        std::string name() const override { return "RabbitMQ"; }
        void start() override;
        void stop() override;
        void notify_online() override;

    private:
        // --- Event loop (runs in event_loop_thread_) ---
        uv_loop_t loop_{};
        std::thread event_loop_thread_;
        uv_async_t async_handle_{};
        std::mutex pending_mutex_;
        std::queue<std::function<void()>> pending_callbacks_;
        bool loop_initialized_ = false;

        // --- AMQP-CPP objects (accessed ONLY from event loop thread) ---
        std::unique_ptr<UvAmqpHandler> handler_;
        std::unique_ptr<AMQP::Connection> connection_;
        std::unique_ptr<AMQP::Channel> task_channel_;
        std::unique_ptr<AMQP::Channel> status_channel_;
        std::unique_ptr<AMQP::Channel> publish_channel_;   ///< Topology + general publishes (progress, RPC replies). NOT in confirm mode.
        std::unique_ptr<AMQP::Channel> result_channel_;    ///< Result publishes ONLY. confirm-select enabled, sequence numbers match broker's.

        // --- State ---
        TestRunnerService& runner_;
        const ManagementAPI* management_;
        Config config_;
        std::shared_ptr<std::atomic<bool>> alive_;
        std::atomic<bool> stop_{false};
        std::atomic<bool> started_{false};

        // --- Durability state (manual-ack + publisher confirms) ---
        // All accessed ONLY from event loop thread, no mutex needed.
        //
        // job_to_tag_: keeps the consume delivery_tag alive between task receipt
        // and final result publish so cancelJob can ack-and-discard a queued job
        // (otherwise broker would redeliver after consumer_timeout).
        std::unordered_map<std::string, uint64_t> job_to_tag_;

        // pending_confirms_: per-publish state, keyed by AMQP-CPP server-assigned
        // publish sequence number. Result is acked on confirm-ack, requeued on
        // confirm-nack, or requeued after a 30s timeout (scanned periodically).
        struct PendingConfirm {
            uint64_t consume_tag;        ///< delivery_tag from test.tasks consumer
            std::string job_id;
            std::chrono::steady_clock::time_point published_at;
        };
        std::map<uint64_t, PendingConfirm> pending_confirms_;
        uint64_t next_publish_seq_ = 1;
        uv_timer_t confirm_timeout_timer_{};
        bool confirm_timer_initialized_ = false;
        static constexpr int kConfirmTimeoutSec = 30;

        // --- Event loop methods ---
        void event_loop_main();
        void setup_channels();
        void declare_topology();
        void start_consumers();

        // --- Cross-thread communication ---
        void post_to_event_loop(std::function<void()> fn);
        static void on_async_callback(uv_async_t* handle);

        // --- Message handlers (event loop thread) ---
        void on_task_received(const AMQP::Message& msg, uint64_t tag, AMQP::Channel* channel);
        void on_control_message(const AMQP::Message& msg, uint64_t tag, bool redelivered);

        // --- Control message dispatch ---
        using ReplyFn = std::function<void(const std::string &, nlohmann::json)>;
        using ControlHandler = std::function<void(const nlohmann::json &, const ReplyFn &)>;
        std::unordered_map<control_type, ControlHandler> control_handlers_;
        void setup_control_handlers();

        // --- Publishing (event loop thread only) ---
        /// Publish a JSON RPC reply to a direct reply queue.
        void publish_reply(
            const std::string& reply_to,
            const nlohmann::json& message,
            const std::string& correlation_id = ""
        );
        /// Common publish: exchange + routing_key + optional correlation_id.
        void publish(
            const std::string& exchange,
            const std::string& routing_key,
            const nlohmann::json& message,
            const std::string& correlation_id = ""
        );

        // --- Durability: result publish with confirms + ack ---
        /// Publish a TaskResult to test.direct/results, register a pending
        /// confirm so the consume delivery_tag is acked only after the broker
        /// confirms the publish. Connection-level publish failure -> silently
        /// leaves the consume tag unacked (broker will redeliver).
        void publish_result_with_confirm(
            const std::string& job_id,
            uint64_t consume_tag,
            const nlohmann::json& message
        );
        /// AMQP-CPP confirm callbacks.
        void on_publish_ack(uint64_t delivery_tag, bool multiple);
        void on_publish_nack(uint64_t delivery_tag, bool multiple);
        /// Periodic timer callback - requeues pending publishes that did not
        /// receive a confirm within kConfirmTimeoutSec.
        void scan_confirm_timeouts();
        /// Common per-entry cleanup helper used by ack/nack/timeout paths.
        void resolve_pending(uint64_t publish_seq, bool ack_consume_tag);

        // --- Helpers ---
        /// Check management_ is available; sends error reply and returns false if not.
        bool require_management(const ReplyFn& reply, control_type ct);
};