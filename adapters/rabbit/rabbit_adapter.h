#pragma once

/**
 * @file rabbit_adapter.h
 * @brief RabbitMQ transport adapter — async AMQP-CPP + libuv.
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
 *   statusRequest           — node status and queue load
 *   queueStatus             — job queue status (lanes, positions, running jobs)
 *   getJobInfo              — query a specific job's status/result
 *   cancelJob               — cancel a queued job by job_id
 *   listAdapters            — available and running adapters
 *   listAvailableAdapters   — only adapters that can be loaded
 *   loadAdapter             — load and start an adapter by name
 *   unloadAdapter           — stop and unload a running adapter
 *   updateConfig            — update engine config (config: {maxCorrectnessWorkers?, jobRetentionSeconds?})
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
#include <control_type.h>
#include <functional>
#include <node_event_type.h>
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
        void notifyOnline() override;

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
        std::unique_ptr<AMQP::Channel> publish_channel_;

        // --- State ---
        TestRunnerService& runner_;
        const ManagementAPI* management_;
        Config config_;
        std::shared_ptr<std::atomic<bool>> alive_;
        std::atomic<bool> stop_{false};
        std::atomic<bool> started_{false};

        // --- Event loop methods ---
        void eventLoopMain();
        void setupChannels();
        void declareTopology();
        void startConsumers();

        // --- Cross-thread communication ---
        void postToEventLoop(std::function<void()> fn);
        static void onAsyncCallback(uv_async_t* handle);

        // --- Message handlers (event loop thread) ---
        void onTaskReceived(const AMQP::Message& msg, uint64_t tag, AMQP::Channel* channel);
        void onControlMessage(const AMQP::Message& msg, uint64_t tag, bool redelivered);

        // --- Control message dispatch ---
        using ReplyFn = std::function<void(const std::string &, nlohmann::json)>;
        using ControlHandler = std::function<void(const nlohmann::json &, const ReplyFn &)>;
        std::unordered_map<control_type, ControlHandler> control_handlers_;
        void setupControlHandlers();

        // --- Publishing (event loop thread only) ---
        /// Publish a JSON RPC reply to a direct reply queue.
        void publishReply(
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

        // --- Node lifecycle ---
        void publishNodeEvent(node_event_type type);

};