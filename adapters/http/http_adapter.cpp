// Include http_adapter.h first so httplib.h pulls in winsock2.h before windows.h
#include <http_adapter.h>
#include <adapter_status.h>
#include <adapter_utils.h>
#include <api_types.h>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <register_adapter.h>
#include <time_utils.h>

// ============================================================================
// HttpAdapter
// ============================================================================

HttpAdapter::HttpAdapter(TestRunnerService& runner, const ManagementAPI* management, const AdapterContext& ctx)
    : runner_(runner), management_(management),
      alive_(std::make_shared<std::atomic<bool>>(true)) {
    const auto& json_config = ctx.config;
    if(ctx.node_id.empty()) {
        throw std::runtime_error("[HTTP] node_id is required in AdapterContext");
    }
    config_.host = json_config.value("host", "");
    config_.port = json_config.value("port", 8080);
    config_.register_url = json_config.value("registerUrl", "");
    config_.registration_timeout_sec = json_config.value("registrationTimeoutSec", 10);
    config_.listen_timeout_sec = json_config.value("listenTimeoutSec", 60);
    config_.api_key = json_config.value("apiKey", "");

    if(config_.host.empty()) {
        throw std::runtime_error("[HTTP] 'host' is required in http.json");
    }

    if(config_.port < 1 || config_.port > 65535) {
        throw std::runtime_error("[HTTP] Invalid port: " + std::to_string(config_.port));
    }

    config_.node_id = ctx.node_id;
    config_.adapter_name = ctx.adapter_name;
    auth_token_ = generateAuthToken();

    // Prevent multiple processes from binding to the same port.
    // Default SO_REUSEADDR on Windows allows port sharing silently.
    #ifdef _WIN32
    svr_.set_socket_options(
        [](socket_t sock) {
            int yes = 1;
            setsockopt(
                sock,
                SOL_SOCKET,
                SO_EXCLUSIVEADDRUSE,
                reinterpret_cast<const char*>(&yes),
                sizeof(yes)
            );
        }
    );
    #endif

    // Limit request body to 1MB to prevent DoS via oversized payloads
    svr_.set_payload_max_length(1024 * 1024);

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

                const bool has_solution = json.contains("solutionSource") && json.contains("solutionSourceType");
                const bool has_tests = json.contains("testSource") && json.contains("testSourceType");
                if(!has_solution || !has_tests) {
                    res.status = 400;
                    res.set_content(
                        nlohmann::json{
                            {
                                "error",
                                "Missing required: solutionSourceType + solutionSource and testSourceType + testSource"
                            }
                        }.dump(),
                        "application/json"
                    );
                    return;
                }

                // Validate testId
                if(!json.contains("testId") || json["testId"].get<std::string>().empty()) {
                    res.status = 400;
                    res.set_content(
                        nlohmann::json{{"error", "Missing required field: testId"}}.dump(),
                        "application/json"
                    );
                    return;
                }

                // Validate mode
                std::string mode_str = json.value("mode", to_string(test_mode::correctness));
                if(!is_valid_test_mode(mode_str)) {
                    res.status = 400;
                    res.set_content(
                        nlohmann::json{
                            {
                                "error",
                                "Invalid mode: '" + mode_str + "'. Must be 'correctness', 'performance', or 'all'"
                            }
                        }.dump(),
                        "application/json"
                    );
                    return;
                }

                // Validate threads
                if(json.contains("threads")) {
                    int threads = json["threads"].get<int>();
                    int max_hw = static_cast<int>(std::thread::hardware_concurrency()) * 2;
                    if(threads < 1 || threads > max_hw) {
                        res.status = 400;
                        res.set_content(
                            nlohmann::json{
                                {
                                    "error",
                                    "Invalid threads: " + std::to_string(threads)
                                    + ". Must be 1.." + std::to_string(max_hw)
                                }
                            }.dump(),
                            "application/json"
                        );
                        return;
                    }
                }

                // Validate numaNode
                if(json.contains("numaNode")) {
                    int numa = json["numaNode"].get<int>();
                    if(numa < -1) {
                        res.status = 400;
                        res.set_content(
                            nlohmann::json{{"error", "Invalid numaNode: must be >= -1"}}.dump(),
                            "application/json"
                        );
                        return;
                    }
                }

                // Validate memoryLimitMb (if provided)
                if(json.contains("memoryLimitMb")) {
                    auto& v = json["memoryLimitMb"];
                    if(!v.is_number_integer() || v.get<long long>() < 0) {
                        res.status = 400;
                        res.set_content(
                            nlohmann::json{{"error", "memoryLimitMb must be non-negative integer"}}.dump(),
                            "application/json"
                        );
                        return;
                    }
                }

                // Validate callbackUrl format (if provided)
                std::string callback_url = json.value("callbackUrl", "");
                if(!callback_url.empty()) {
                    const size_t scheme_end = callback_url.find("://");
                    if(scheme_end == std::string::npos) {
                        res.status = 400;
                        res.set_content(
                            nlohmann::json{
                                {"error", "Invalid callbackUrl: missing scheme (expected http:// or https://)"}
                            }.dump(),
                            "application/json"
                        );
                        return;
                    }
                    std::string scheme = callback_url.substr(0, scheme_end);
                    if(scheme != "http" && scheme != "https") {
                        res.status = 400;
                        res.set_content(
                            nlohmann::json{
                                {"error", "Invalid callbackUrl scheme: '" + scheme + "'. Must be http or https"}
                            }.dump(),
                            "application/json"
                        );
                        return;
                    }
                }

                auto alive = alive_;
                std::string api_key = config_.api_key;
                std::string node_id = config_.node_id;
                auto start_time = std::chrono::steady_clock::now();

                // Extract solution name before json is moved into submit()
                std::string solution_name;

                auto on_complete = [alive, callback_url, api_key, node_id, start_time](
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
                            ? callback_url.substr(0, path_start)
                            : callback_url;
                        const std::string path = (path_start != std::string::npos)
                            ? callback_url.substr(path_start)
                            : "/";
                        httplib::Client cli(host_port);
                        cli.set_connection_timeout(10);
                        cli.set_read_timeout(30);
                        if(!api_key.empty())
                            cli.set_default_headers({{"X-API-Key", api_key}});
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

                // Extract fields for response before json is moved into submit()
                if(json.contains("solutionSource")) {
                    const auto& src = json["solutionSource"];
                    if(src.contains("path"))
                        solution_name = std::filesystem::path(src.value("path", "")).filename().string();
                    else if(src.contains("url"))
                        solution_name = src.value("url", "");
                }
                long long memory_limit_mb = json.value("memoryLimitMb", runner_.default_memory_limit_mb());

                auto job_id = runner_.submit(std::move(json), std::move(on_complete));
                auto info = runner_.getJobInfo(job_id);

                std::cout << "[HTTP] POST /api/run -> job " << job_id
                    << " (pos=" << info.queue_position << ")\n";
                res.status = 202;
                res.set_content(
                    nlohmann::json{
                        {"jobId", job_id},
                        {"status", to_string(job_status::queued)},
                        {"nodeId", config_.node_id},
                        {"position", info.queue_position},
                        {"mode", mode_str},
                        {"solution", solution_name},
                        {"memoryLimitMb", memory_limit_mb},
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
                        {"status", to_string(job_status::cancelled)}
                    }.dump(),
                    "application/json"
                );
            } else {
                res.status = 409;
                res.set_content(
                    nlohmann::json{
                        {"jobId", std::string(req.matches[1])},
                        {"status", to_string(response_status::error)},
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
            auto status = adapter_utils::buildNodeEvent(node_event_type::info, config_.node_id, runner_, management_);
            res.set_content(status.dump(), "application/json");
        }
    );

    // PUT /api/config — dynamically update engine configuration
    svr_.Put(
        "/api/config",
        [this](const httplib::Request& req, httplib::Response& res) {
            try {
                auto json = nlohmann::json::parse(req.body);
                auto [ok, err] = adapter_utils::applyConfig(runner_, json);
                if(!ok) {
                    res.status = 400;
                    res.set_content(
                        nlohmann::json{
                            {"status", to_string(response_status::error)},
                            {"error", err}
                        }.dump(),
                        "application/json"
                    );
                    return;
                }
                std::cout << "[HTTP] PUT /api/config: " << json.dump() << "\n";
                res.set_content(runner_.getQueueStatus().dump(), "application/json");
            } catch(const std::exception& e) {
                res.status = 400;
                res.set_content(
                    nlohmann::json{
                        {"status", to_string(response_status::error)},
                        {"error", e.what()}
                    }.dump(),
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
            auto adapters = nlohmann::json::parse(json_str);
            management_->free_string(management_->context, json_str);
            res.set_content(
                nlohmann::json{{"adapters", adapters}}.dump(),
                "application/json"
            );
        }
    );

    // GET /api/adapters/available — list only adapters that can be loaded
    svr_.Get(
        "/api/adapters/available",
        [this](const httplib::Request&, httplib::Response& res) {
            auto available = adapter_utils::filterAvailableAdapters(management_);
            res.set_content(
                nlohmann::json{{"adapters", available}}.dump(),
                "application/json"
            );
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
                        {"status", to_string(adapter_status::started)}
                    }.dump(),
                    "application/json"
                );
            } else {
                res.status = 400;
                res.set_content(
                    nlohmann::json{
                        {"adapter", adapter},
                        {"status", to_string(adapter_status::failed)},
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
                        {"status", to_string(adapter_status::stopped)}
                    }.dump(),
                    "application/json"
                );
            } else {
                res.status = 404;
                res.set_content(
                    nlohmann::json{
                        {"adapter", adapter},
                        {"status", to_string(adapter_status::failed)},
                        {"error", "Adapter '" + adapter + "' not found or not running"}
                    }.dump(),
                    "application/json"
                );
            }
        }
    );

    // GET /api/resource-providers — list available and running resource providers
    svr_.Get(
        "/api/resource-providers",
        [this](const httplib::Request&, httplib::Response& res) {
            const char* json_str = management_->list_resource_providers(management_->context);
            if(!json_str) {
                res.status = 500;
                res.set_content(R"({"error":"Failed to list resource providers"})", "application/json");
                return;
            }
            auto providers = nlohmann::json::parse(json_str);
            management_->free_string(management_->context, json_str);
            res.set_content(
                nlohmann::json{{"providers", providers}}.dump(),
                "application/json"
            );
        }
    );

    // GET /api/resource-providers/available — list only providers that can be loaded
    svr_.Get(
        "/api/resource-providers/available",
        [this](const httplib::Request&, httplib::Response& res) {
            const char* json_str = management_->list_available_resource_providers(management_->context);
            if(!json_str) {
                res.status = 500;
                res.set_content(R"({"error":"Failed to list available resource providers"})", "application/json");
                return;
            }
            auto providers = nlohmann::json::parse(json_str);
            management_->free_string(management_->context, json_str);
            res.set_content(
                nlohmann::json{{"providers", providers}}.dump(),
                "application/json"
            );
        }
    );

    // POST /api/resource-providers/:name — load and start a resource provider
    svr_.Post(
        R"(/api/resource-providers/([a-zA-Z0-9_-]+))",
        [this](const httplib::Request& req, httplib::Response& res) {
            std::string provider = req.matches[1];

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

            if(management_->load_resource_provider(management_->context, provider.c_str(), config)) {
                res.status = 201;
                res.set_content(
                    nlohmann::json{
                        {"provider", provider},
                        {"status", to_string(adapter_status::started)}
                    }.dump(),
                    "application/json"
                );
            } else {
                res.status = 400;
                res.set_content(
                    nlohmann::json{
                        {"provider", provider},
                        {"status", to_string(adapter_status::failed)},
                        {"error", "Failed to load resource provider '" + provider + "'. Check server logs."}
                    }.dump(),
                    "application/json"
                );
            }
        }
    );

    // DELETE /api/resource-providers/:name — stop and unload a resource provider
    svr_.Delete(
        R"(/api/resource-providers/([a-zA-Z0-9_-]+))",
        [this](const httplib::Request& req, httplib::Response& res) {
            std::string provider = req.matches[1];

            if(management_->unload_resource_provider(management_->context, provider.c_str())) {
                res.set_content(
                    nlohmann::json{
                        {"provider", provider},
                        {"status", to_string(adapter_status::stopped)}
                    }.dump(),
                    "application/json"
                );
            } else {
                res.status = 404;
                res.set_content(
                    nlohmann::json{
                        {"provider", provider},
                        {"status", to_string(adapter_status::failed)},
                        {"error", "Resource provider '" + provider + "' not found or not running"}
                    }.dump(),
                    "application/json"
                );
            }
        }
    );
}

void HttpAdapter::start() {
    listen_failed_ = false;

    // Store runtime values in managed config so buildTransportsList includes them
    if(management_) {
        management_->update_adapter_config(
            management_->context,
            config_.adapter_name.c_str(),
            {{"host", config_.host}, {"authToken", auth_token_}}
        );
    }

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

}

void HttpAdapter::notifyOnline() {
    if(config_.register_url.empty()) return;

    if(config_.api_key.empty()) {
        stop();
        throw std::runtime_error("[HTTP] apiKey is required for orchestrator registration (set in http.json)");
    }
    if(!doRegister()) {
        stop();
        throw std::runtime_error("[HTTP] Registration with orchestrator failed");
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
static void parseUrl(
    const std::string& raw,
    std::string& scheme,
    std::string& host_port,
    std::string& path
) {
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

    auto body = adapter_utils::buildNodeEvent(node_event_type::online, config_.node_id, runner_, management_);

    try {
        httplib::Client client(scheme + "://" + host_port);
        client.set_connection_timeout(config_.registration_timeout_sec);
        client.set_read_timeout(config_.registration_timeout_sec);
        if(!config_.api_key.empty())
            client.set_default_headers({{"X-API-Key", config_.api_key}});

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
                    registered_ = true;
                    std::cout << "[HTTP] Registration confirmed by orchestrator\n";
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

    auto body = adapter_utils::buildNodeEvent(node_event_type::offline, config_.node_id, runner_, management_);

    try {
        httplib::Client client(scheme + "://" + host_port);
        client.set_connection_timeout(5);
        client.set_read_timeout(5);
        if(!config_.api_key.empty())
            client.set_default_headers({{"X-API-Key", config_.api_key}});

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