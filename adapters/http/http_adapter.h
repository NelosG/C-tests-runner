#pragma once

/**
 * @file http_adapter.h
 * @brief HTTP REST API adapter using cpp-httplib.
 *
 * Provides a REST interface for test job submission and management:
 *   POST   /api/run           — submit a test job
 *   GET    /api/jobs/:id      — query job status/result
 *   DELETE /api/jobs/:id      — cancel a queued job
 *   GET    /api/status        — queue overview
 *   GET    /api/health        — health check
 *   GET    /api/node/status   — detailed node status for orchestrator
 *   PUT    /api/config        — dynamically update engine configuration
 *
 * If ManagementAPI is provided, also exposes adapter management endpoints:
 *   GET    /api/adapters          — list available/running adapters
 *   POST   /api/adapters/:name   — load and start an adapter
 *   DELETE /api/adapters/:name   — stop and unload an adapter
 */

#include <adapter_api.h>
#include <adapter_context.h>
#include <atomic>
#include <httplib.h>
#include <memory>
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
        void notifyOnline() override;

    private:
        bool doRegister();
        void doDeregister();
        void setupTestRoutes();
        void setupManagementRoutes();

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
};