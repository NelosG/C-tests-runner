#pragma once

/**
 * @file http_adapter.h
 * @brief HTTP REST API adapter using cpp-httplib.
 *
 * Provides a REST interface for test job submission and management:
 *   POST   /api/run           - submit a test job
 *   GET    /api/jobs/:id      - query job status/result
 *   DELETE /api/jobs/:id      - cancel a queued job
 *   GET    /api/status        - queue overview
 *   GET    /api/health        - health check
 *   GET    /api/node/status   - detailed node status for orchestrator
 *   PUT    /api/config        - dynamically update engine configuration
 *
 * If ManagementAPI is provided, also exposes adapter management endpoints:
 *   GET    /api/adapters          - list available/running adapters
 *   POST   /api/adapters/:name   - load and start an adapter
 *   DELETE /api/adapters/:name   - stop and unload an adapter
 */

#include <adapter_api.h>
#include <adapter_context.h>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <httplib.h>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <test_execution_adapter.h>
#include <test_runner_service.h>
#include <thread>

class HttpAdapter : public TestExecutionAdapter {
    public:
        struct Config {
            std::string host;
            int port = 8080;
            std::string register_url;
            int registration_timeout_sec = 10;
            int listen_timeout_sec = 60;
            std::string node_id;
            std::string adapter_name;
            std::string api_key;
        };

        HttpAdapter(TestRunnerService& runner, const ManagementAPI* management, const AdapterContext& ctx);
        ~HttpAdapter() override;

        std::string name() const override { return "HTTP"; }
        void start() override;
        void stop() override;
        void notify_online() override;

    private:
        bool do_register();
        void do_deregister();
        void setup_test_routes();
        void setup_management_routes();

        // --- Progress event publisher (separate worker thread) ---
        /// Drain the bounded queue, POSTing each event to progress_url_.
        /// Drops events on network failure. Exits when progress_stop_ is set.
        void progress_worker_loop();
        /// Push an event onto the queue, evicting oldest if full.
        void enqueue_progress(nlohmann::json event);

        TestRunnerService& runner_;
        const ManagementAPI* management_;  ///< May be nullptr if no management needed.
        Config config_;

        std::string auth_token_;

        httplib::Server svr_;
        std::shared_ptr<std::atomic<bool>> alive_;
        std::atomic<bool> stop_{false};
        std::thread server_thread_;
        std::atomic<bool> listen_failed_{false};
        std::atomic<bool> registered_{false};

        // --- Progress publisher state ---
        std::thread progress_thread_;
        std::deque<nlohmann::json> progress_queue_;
        std::mutex progress_mutex_;
        std::condition_variable progress_cv_;
        std::atomic<bool> progress_stop_{false};
        std::string progress_url_;  ///< Derived from register_url at start().
        static constexpr size_t kProgressQueueMax = 100;

        // --- Heartbeat / registration retry (single background loop) ---
        // Re-POSTs the node-online event so an orchestrator that restarted /
        // forgot us repopulates its node table. Idempotent on the
        // orchestrator side. Disabled when register_url is empty.
        // While not yet registered, runs at the shorter retry interval so the
        // engine recovers quickly when the orchestrator comes online; after
        // the first successful registration it falls back to the long
        // heartbeat cadence.
        std::thread heartbeat_thread_;
        std::mutex heartbeat_mutex_;
        std::condition_variable heartbeat_cv_;
        std::atomic<bool> heartbeat_stop_{false};
        void heartbeat_worker_loop();
        static constexpr int kHeartbeatIntervalSec = 300;       // 5 min, after registered
        static constexpr int kRegistrationRetryIntervalSec = 30; // while not registered

        // Counters for visibility into progress delivery health. All published
        // events are counted (HTTP 2xx); drops are split between "no room in the
        // local queue" (orchestrator can't keep up / engine bursting too fast)
        // and "HTTP send to orchestrator failed" (network / server unhealthy).
        std::atomic<uint64_t> progress_published_{0};
        std::atomic<uint64_t> progress_dropped_send_{0};
        std::atomic<uint64_t> progress_dropped_queue_{0};
};
