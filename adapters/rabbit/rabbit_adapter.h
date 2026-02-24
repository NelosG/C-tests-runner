#pragma once

/**
 * @file rabbit_adapter.h
 * @brief RabbitMQ transport adapter — async AMQP-CPP + libuv.
 *
 * Architecture:
 *   Single event loop thread handles ALL AMQP I/O:
 *     - Multiple channels on one connection
 *     - Correctness + performance consumers
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
 *   setMaxCorrectnessWorkers — dynamically resize correctness thread pool
 *
 * Topology:
 *   Exchange: test.direct (direct)
 *     Queue: test.tasks.correctness  <- routing key "correctness"
 *     Queue: test.tasks.performance  <- routing key "performance"
 *     Queue: test.results            <- routing key "result"
 *   Exchange: node.fanout (fanout)
 *     Queue: node.events
 */

#include <adapter_api.h>
#include <amqpcpp.h>
#include <atomic>
#include <functional>
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

        RabbitAdapter(TestRunnerService& runner, const ManagementAPI* management, const nlohmann::json& config);
        ~RabbitAdapter() override;

        std::string name() const override { return "RabbitMQ"; }
        void start() override;
        void stop() override;

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
        std::unique_ptr<AMQP::Channel> correctness_channel_;
        std::unique_ptr<AMQP::Channel> performance_channel_;
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
        using ReplyFn = std::function<void(const std::string&, nlohmann::json)>;
        using ControlHandler = std::function<void(const nlohmann::json&, const ReplyFn&)>;
        std::unordered_map<std::string, ControlHandler> control_handlers_;
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
        void publishNodeEvent(const std::string& type);

};