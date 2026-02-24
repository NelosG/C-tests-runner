#include <adapter_utils.h>
#include <chrono>
#include <http_adapter.h>
#include <iostream>
#include <register_adapter.h>
#include <time_utils.h>

// ============================================================================
// HttpAdapter
// ============================================================================

HttpAdapter::HttpAdapter(TestRunnerService& runner, const ManagementAPI* management, const nlohmann::json& json_config)
    : runner_(runner), management_(management),
      alive_(std::make_shared<std::atomic<bool>>(true)) {
    config_.port = json_config.value("port", 8080);
    config_.register_url = json_config.value("registerUrl", "");
    config_.registration_timeout_sec = json_config.value("registrationTimeoutSec", 10);
    config_.listen_timeout_sec = json_config.value("listenTimeoutSec", 60);
    config_.node_id = json_config.value("nodeId", "");

    if(config_.port < 1 || config_.port > 65535) {
        throw std::runtime_error("[HTTP] Invalid port: " + std::to_string(config_.port));
    }

    if(config_.node_id.empty()) {
        config_.node_id = generateNodeId();
    }
    auth_token_ = generateAuthToken();

    // Prevent multiple processes from binding to the same port.
    // Default SO_REUSEADDR on Windows allows port sharing silently.
    #ifdef _WIN32
    svr_.set_socket_options([](socket_t sock) {
        int yes = 1;
        setsockopt(sock, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
            reinterpret_cast<const char*>(&yes), sizeof(yes));
    });
    #endif

    // Bearer token auth middleware — reject unauthorized requests (except GET /api/health)
    svr_.set_pre_routing_handler(
        [this](const httplib::Request& req, httplib::Response& res) -> httplib::Server::HandlerResponse {
            if(req.method == "GET" && req.path == "/api/health") {
                return httplib::Server::HandlerResponse::Unhandled;
            }
            std::string expected = "Bearer " + auth_token_;
            auto it = req.headers.find("Authorization");
            if(it == req.headers.end() || it->second != expected) {
                res.status = 401;
                res.set_content(
                    R"({"error":"Unauthorized: invalid or missing Bearer token"})",
                    "application/json"
                );
                return httplib::Server::HandlerResponse::Handled;
            }
            return httplib::Server::HandlerResponse::Unhandled;
        }
    );

    setupTestRoutes();
    setupManagementRoutes();
}

HttpAdapter::~HttpAdapter() {
    try {
        HttpAdapter::stop();
    } catch(const std::exception& e) {
        std::cerr << "[HTTP] Error during shutdown: " << e.what() << "\n";
    } catch(...) {}
}

void HttpAdapter::setupTestRoutes() {
    svr_.Post(
        "/api/run",
        [this](const httplib::Request& req, httplib::Response& res) {
            try {
                auto json = nlohmann::json::parse(req.body);

                const bool has_solution = json.contains("solutionDir") || json.contains("solutionGitUrl");
                const bool has_tests = json.contains("testDir") || json.contains("testGitUrl");
                if(!has_solution || !has_tests) {
                    res.status = 400;
                    res.set_content(
                        R"({"error":"Missing required: solutionDir (or solutionGitUrl) and testDir (or testGitUrl)})",
                        "application/json"
                    );
                    return;
                }

                std::string callback_url = json.value("callbackUrl", "");
                auto alive = alive_;
                std::string orch_token = orchestrator_token_;
                std::string node_id = config_.node_id;
                auto start_time = std::chrono::steady_clock::now();

                auto on_complete = [alive, callback_url, orch_token, node_id, start_time](
                    const nlohmann::json& result
                ) {
                    if(!alive->load()) return;

                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start_time
                    ).count();
                    std::string job_id = result.value("jobId", "");
                    auto msg = adapter_utils::buildCompletionResult(result, job_id, node_id, duration);

                    if(callback_url.empty()) return;
                    try {
                        const size_t scheme_end = callback_url.find("://");
                        if(scheme_end == std::string::npos)
                            throw std::runtime_error("Invalid callback URL: " + callback_url);
                        const size_t path_start = callback_url.find('/', scheme_end + 3);
                        const std::string host_port = (path_start != std::string::npos)
                            ? callback_url.substr(0, path_start) : callback_url;
                        const std::string path = (path_start != std::string::npos)
                            ? callback_url.substr(path_start) : "/";
                        httplib::Client cli(host_port);
                        cli.set_connection_timeout(10);
                        cli.set_read_timeout(30);
                        if(!orch_token.empty())
                            cli.set_bearer_token_auth(orch_token);
                        auto r = cli.Put(path, msg.dump(), "application/json");
                        if(!r)
                            std::cerr << "[HTTP] Callback PUT " << path << " failed: "
                                << httplib::to_string(r.error()) << "\n";
                        else if(r->status >= 400)
                            std::cerr << "[HTTP] Callback PUT " << path << " returned HTTP "
                                << r->status << "\n";
                    } catch(const std::exception& e) {
                        std::cerr << "[HTTP] Callback failed: " << e.what() << "\n";
                    }
                };

                auto job_id = runner_.submit(std::move(json), std::move(on_complete));
                auto info = runner_.getJobInfo(job_id);

                std::cout << "[HTTP] POST /api/run -> job " << job_id
                    << " (pos=" << info.queue_position << ")\n";
                res.status = 202;
                res.set_content(
                    nlohmann::json{
                        {"jobId", job_id},
                        {"status", "queued"},
                        {"nodeId", config_.node_id},
                        {"position", info.queue_position},
                        {"timestamp", nowISO8601()}
                    }.dump(),
                    "application/json"
                );
            } catch(const std::exception& e) {
                res.status = 400;
                res.set_content(
                    nlohmann::json{{"error", e.what()}}.dump(),
                    "application/json"
                );
            }
        }
    );

    svr_.Get(
        "/api/status",
        [this](const httplib::Request&, httplib::Response& res) {
            res.set_content(runner_.getQueueStatus().dump(), "application/json");
        }
    );

    svr_.Get(
        R"(/api/jobs/([a-zA-Z0-9_-]+))",
        [this](const httplib::Request& req, httplib::Response& res) {
            try {
                auto info = runner_.getJobInfo(req.matches[1]);
                res.set_content(adapter_utils::buildJobInfoJson(info).dump(), "application/json");
            } catch(const std::exception& e) {
                res.status = 404;
                res.set_content(
                    nlohmann::json{{"error", e.what()}}.dump(),
                    "application/json"
                );
            }
        }
    );

    svr_.Delete(
        R"(/api/jobs/([a-zA-Z0-9_-]+))",
        [this](const httplib::Request& req, httplib::Response& res) {
            const bool ok = runner_.cancel(req.matches[1]);
            if(ok) {
                res.set_content(
                    nlohmann::json{
                        {"jobId", std::string(req.matches[1])},
                        {"status", "cancelled"}
                    }.dump(),
                    "application/json"
                );
            } else {
                res.status = 409;
                res.set_content(
                    nlohmann::json{
                        {"error", "Cannot cancel job (not queued or not found)"}
                    }.dump(),
                    "application/json"
                );
            }
        }
    );

    svr_.Get(
        "/api/health",
        [](const httplib::Request&, httplib::Response& res) {
            res.set_content(R"({"status":"ok"})", "application/json");
        }
    );

    // GET /api/node/status — detailed node status for orchestrator polling
    svr_.Get(
        "/api/node/status",
        [this](const httplib::Request&, httplib::Response& res) {
            auto status = adapter_utils::buildNodeStatus(runner_, config_.node_id, "http");
            status["type"] = "statusResponse";
            status["port"] = config_.port;
            res.set_content(status.dump(), "application/json");
        }
    );

    // PUT /api/config — dynamically update engine configuration
    svr_.Put(
        "/api/config",
        [this](const httplib::Request& req, httplib::Response& res) {
            try {
                auto json = nlohmann::json::parse(req.body);
                if(json.contains("maxCorrectnessWorkers")) {
                    int n = json["maxCorrectnessWorkers"].get<int>();
                    runner_.setMaxCorrectnessWorkers(n);
                    std::cout << "[HTTP] PUT /api/config -> maxCorrectnessWorkers=" << n << "\n";
                }
                if(json.contains("jobRetentionSeconds")) {
                    int sec = json["jobRetentionSeconds"].get<int>();
                    runner_.setJobRetentionSeconds(sec);
                    std::cout << "[HTTP] PUT /api/config -> jobRetentionSeconds=" << sec << "\n";
                }
                res.set_content(runner_.getQueueStatus().dump(), "application/json");
            } catch(const std::exception& e) {
                res.status = 400;
                res.set_content(
                    nlohmann::json{{"error", e.what()}}.dump(),
                    "application/json"
                );
            }
        }
    );
}

void HttpAdapter::setupManagementRoutes() {
    if(!management_) {
        std::cout << "[HTTP] Management API not provided, skipping management routes\n";
        return;
    }

    // GET /api/adapters — list available and running adapters
    svr_.Get(
        "/api/adapters",
        [this](const httplib::Request&, httplib::Response& res) {
            const char* json_str = management_->list_adapters(management_->context);
            if(!json_str) {
                res.status = 500;
                res.set_content(R"({"error":"Failed to list adapters"})", "application/json");
                return;
            }
            res.set_content(json_str, "application/json");
            management_->free_string(management_->context, json_str);
        }
    );

    // GET /api/adapters/available — list only adapters that can be loaded
    svr_.Get(
        "/api/adapters/available",
        [this](const httplib::Request&, httplib::Response& res) {
            auto available = adapter_utils::filterAvailableAdapters(management_);
            res.set_content(available.dump(), "application/json");
        }
    );

    // POST /api/adapters/:name — load and start an adapter (config in body)
    svr_.Post(
        R"(/api/adapters/([a-zA-Z0-9_-]+))",
        [this](const httplib::Request& req, httplib::Response& res) {
            std::string adapter = req.matches[1];

            nlohmann::json config;
            if(!req.body.empty()) {
                try {
                    config = nlohmann::json::parse(req.body);
                } catch(const std::exception& e) {
                    res.status = 400;
                    res.set_content(
                        nlohmann::json{{"error", "Invalid JSON: " + std::string(e.what())}}.dump(),
                        "application/json"
                    );
                    return;
                }
            }

            if(management_->load_adapter(management_->context, adapter.c_str(), config)) {
                res.status = 201;
                res.set_content(
                    nlohmann::json{
                        {"adapter", adapter},
                        {"status", "started"}
                    }.dump(),
                    "application/json"
                );
            } else {
                res.status = 400;
                res.set_content(
                    nlohmann::json{
                        {"error", "Failed to load adapter '" + adapter + "'. Check server logs."}
                    }.dump(),
                    "application/json"
                );
            }
        }
    );

    // DELETE /api/adapters/:name — stop and unload an adapter
    svr_.Delete(
        R"(/api/adapters/([a-zA-Z0-9_-]+))",
        [this](const httplib::Request& req, httplib::Response& res) {
            std::string adapter = req.matches[1];

            if(management_->unload_adapter(management_->context, adapter.c_str())) {
                res.set_content(
                    nlohmann::json{
                        {"adapter", adapter},
                        {"status", "stopped"}
                    }.dump(),
                    "application/json"
                );
            } else {
                res.status = 404;
                res.set_content(
                    nlohmann::json{
                        {"error", "Adapter '" + adapter + "' not found or not running"}
                    }.dump(),
                    "application/json"
                );
            }
        }
    );
}

void HttpAdapter::start() {
    listen_failed_ = false;

    server_thread_ = std::thread(
        [this]() {
            std::cout << "[HTTP] Auth token: ..."
                << auth_token_.substr(auth_token_.size() - std::min<size_t>(8, auth_token_.size())) << "\n";
            std::cout << "[HTTP] Starting on 0.0.0.0:" << config_.port << "\n";
            if(!svr_.listen("0.0.0.0", config_.port)) {
                std::cerr << "[HTTP] Failed to bind to port " << config_.port
                    << " (is it already in use?)\n";
                listen_failed_ = true;
            }
            std::cout << "[HTTP] Server stopped\n";
        }
    );

    // Wait for the server to actually start (or fail to bind)
    auto deadline = std::chrono::steady_clock::now()
        + std::chrono::seconds(config_.listen_timeout_sec);
    while(!svr_.is_running() && !listen_failed_
          && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if(!svr_.is_running()) {
        if(server_thread_.joinable()) server_thread_.join();
        std::string reason = listen_failed_
            ? "port " + std::to_string(config_.port) + " is already in use"
            : "timed out after " + std::to_string(config_.listen_timeout_sec) + "s";
        throw std::runtime_error("[HTTP] Failed to listen — " + reason);
    }
    std::cout << "[HTTP] Listening on 0.0.0.0:" << config_.port << "\n";

    // Registration (optional — only if register_url is configured)
    if(!config_.register_url.empty()) {
        if(!doRegister()) {
            stop();
            throw std::runtime_error("[HTTP] Registration with orchestrator failed");
        }
    }
}

void HttpAdapter::stop() {
    if(stop_.exchange(true)) return;
    alive_->store(false);
    doDeregister();
    svr_.stop();
    if(server_thread_.joinable()) {
        server_thread_.join();
    }
}

// ============================================================================
// Registration / deregistration via HTTP POST
// ============================================================================

/// Parse "scheme://host:port/path" into components.
static void parseUrl(const std::string& raw, std::string& scheme,
                     std::string& host_port, std::string& path) {
    scheme = "http";
    path = "/";
    std::string url = raw;

    auto scheme_pos = url.find("://");
    if(scheme_pos != std::string::npos) {
        scheme = url.substr(0, scheme_pos);
        url = url.substr(scheme_pos + 3);
    }
    auto path_pos = url.find('/');
    if(path_pos != std::string::npos) {
        host_port = url.substr(0, path_pos);
        path = url.substr(path_pos);
    } else {
        host_port = url;
    }
}

bool HttpAdapter::doRegister() {
    if(config_.register_url.empty()) return true;

    std::cout << "[HTTP] Attempting registration via POST " << config_.register_url
        << " (timeout: " << config_.registration_timeout_sec << "s)...\n";

    std::string scheme, host_port, path;
    parseUrl(config_.register_url, scheme, host_port, path);

    nlohmann::json body = {
        {"type", "register"},
        {"nodeId", config_.node_id},
        {"transport", "http"},
        {"port", config_.port},
        {"authToken", auth_token_},
        {
            "capabilities",
            {
                {"maxConcurrentCorrectness", runner_.getQueueStatus().value("maxCorrectnessWorkers", 0)},
                {"maxOmpThreads", static_cast<int>(std::thread::hardware_concurrency())}
            }
        }
    };

    try {
        httplib::Client client(scheme + "://" + host_port);
        client.set_connection_timeout(config_.registration_timeout_sec);
        client.set_read_timeout(config_.registration_timeout_sec);

        auto res = client.Post(path, body.dump(), "application/json");

        if(!res) {
            std::cerr << "[HTTP] Registration request failed: "
                << httplib::to_string(res.error()) << "\n";
            return false;
        }

        if(res->status == 200) {
            try {
                auto resp = nlohmann::json::parse(res->body);
                if(resp.value("status", "") == "registered") {
                    orchestrator_token_ = resp.value("orchestratorAuthToken", "");
                    registered_ = true;
                    std::cout << "[HTTP] Registration confirmed by orchestrator";
                    if(!orchestrator_token_.empty())
                        std::cout << " (orchestrator token received)";
                    std::cout << "\n";
                    return true;
                }
                std::cerr << "[HTTP] Unexpected registration response: " << res->body << "\n";
            } catch(const std::exception& e) {
                std::cerr << "[HTTP] Invalid registration response JSON: " << e.what() << "\n";
            }
        } else {
            std::cerr << "[HTTP] Registration failed with HTTP " << res->status << "\n";
        }
    } catch(const std::exception& e) {
        std::cerr << "[HTTP] Registration error: " << e.what() << "\n";
    }

    return false;
}

void HttpAdapter::doDeregister() {
    if(config_.register_url.empty() || !registered_) return;

    std::cout << "[HTTP] Sending deregister to orchestrator...\n";

    std::string scheme, host_port, path;
    parseUrl(config_.register_url, scheme, host_port, path);

    nlohmann::json body = {
        {"type", "deregister"},
        {"nodeId", config_.node_id}
    };

    try {
        httplib::Client client(scheme + "://" + host_port);
        client.set_connection_timeout(5);
        client.set_read_timeout(5);
        if(!orchestrator_token_.empty())
            client.set_bearer_token_auth(orchestrator_token_);

        auto res = client.Post(path, body.dump(), "application/json");
        if(res && res->status == 200) {
            std::cout << "[HTTP] Deregistered from orchestrator\n";
        } else if(res) {
            std::cerr << "[HTTP] Deregister returned HTTP " << res->status << "\n";
        } else {
            std::cerr << "[HTTP] Deregister failed: "
                << httplib::to_string(res.error()) << "\n";
        }
    } catch(const std::exception& e) {
        std::cerr << "[HTTP] Deregister error: " << e.what() << "\n";
    }
}

// ============================================================================
// DLL Factory (generated by macro)
// ============================================================================

REGISTER_ADAPTER(HttpAdapter, "http")