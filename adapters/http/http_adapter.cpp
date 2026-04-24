// Include http_adapter.h first so httplib.h pulls in winsock2.h before windows.h
#include <http_adapter.h>
#include <adapter_status.h>
#include <adapter_utils.h>
#include <algorithm>
#include <api_types.h>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <register_adapter.h>
#include <time_utils.h>

/// Forward declaration: definition lives further down (used by start() and
/// progress_worker_loop() which appear before it in source order).
static void parse_url(const std::string& raw, std::string& scheme,
                      std::string& host_port, std::string& path);

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
    auth_token_ = generate_auth_token();

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

    // Bearer token auth middleware - reject unauthorized requests (except GET /api/health)
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

    setup_test_routes();
    setup_management_routes();
}

HttpAdapter::~HttpAdapter() {
    try {
        HttpAdapter::stop();
    } catch(const std::exception& e) {
        std::cerr << "[HTTP] Error during shutdown: " << e.what() << "\n";
    } catch(...) {}
}

void HttpAdapter::setup_test_routes() {
    svr_.Post(
        "/api/run",
        [this](const httplib::Request& req, httplib::Response& res) {
            try {
                auto json = nlohmann::json::parse(req.body);

                // Shared validation (sources, testId, jobId, threads, memoryLimitMb, wallTimeSec, cpuTimeSec, maxProcesses)
                auto [valid, err] = adapter_utils::validate_run_request(json);
                if(!valid) {
                    res.status = 400;
                    res.set_content(nlohmann::json{{"error", err}}.dump(), "application/json");
                    return;
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
                    auto msg = adapter_utils::build_completion_result(result, job_id, node_id, duration);

                    if(callback_url.empty()) return;
                    try {
                        const size_t scheme_end = callback_url.find("://");
                        if(scheme_end == std::string::npos)
                            throw std::runtime_error("Invalid callback URL: missing scheme");
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
                        // Use job_id (non-sensitive) for correlation instead of the
                        // path, which can contain webhook-secret-like tokens.
                        if(!r)
                            std::cerr << "[HTTP] Callback PUT failed for job " << job_id
                                << ": " << httplib::to_string(r.error()) << "\n";
                        else if(r->status >= 400)
                            std::cerr << "[HTTP] Callback PUT for job " << job_id
                                << " returned HTTP " << r->status << "\n";
                    } catch(const std::exception& e) {
                        std::cerr << "[HTTP] Callback failed for job " << job_id
                            << ": " << e.what() << "\n";
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

                // Inject node_id so pipeline tags progress events with this runner.
                json["_node_id"] = config_.node_id;

                // Pre-extract jobId for the "received" progress event (fall through
                // to runner-generated id if absent - same logic as JobQueue).
                std::string client_job_id = json.value("jobId", "");
                if(!client_job_id.empty()) {
                    enqueue_progress({
                        {"jobId",     client_job_id},
                        {"nodeId",    config_.node_id},
                        {"phase",     "received"},
                        {"timestamp", now_iso8601()}
                    });
                }

                auto on_progress = [this](const nlohmann::json& event) {
                    enqueue_progress(event);
                };

                auto job_id = runner_.submit(std::move(json), std::move(on_complete), std::move(on_progress));
                // If the client did not supply jobId, the runner generated one - emit "received" now.
                if(client_job_id.empty()) {
                    enqueue_progress({
                        {"jobId",     job_id},
                        {"nodeId",    config_.node_id},
                        {"phase",     "received"},
                        {"timestamp", now_iso8601()}
                    });
                }
                auto info = runner_.get_job_info(job_id);

                std::cout << "[HTTP] POST /api/run -> job " << job_id
                    << " (pos=" << info.queue_position << ")\n";
                res.status = 202;
                res.set_content(
                    nlohmann::json{
                        {"jobId", job_id},
                        {"status", to_string(job_status::queued)},
                        {"nodeId", config_.node_id},
                        {"position", info.queue_position},
                        {"solution", solution_name},
                        {"memoryLimitMb", memory_limit_mb},
                        {"timestamp", now_iso8601()}
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
            res.set_content(runner_.get_queue_status().dump(), "application/json");
        }
    );

    svr_.Get(
        R"(/api/jobs/([a-zA-Z0-9_-]+))",
        [this](const httplib::Request& req, httplib::Response& res) {
            try {
                auto info = runner_.get_job_info(req.matches[1]);
                res.set_content(adapter_utils::build_job_info_json(info).dump(), "application/json");
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

    // GET /api/node/status - detailed node status for orchestrator polling
    svr_.Get(
        "/api/node/status",
        [this](const httplib::Request&, httplib::Response& res) {
            auto status = adapter_utils::build_node_event(node_event_type::info, config_.node_id, runner_, management_);
            res.set_content(status.dump(), "application/json");
        }
    );

    // PUT /api/config - dynamically update engine configuration.
    // Canonical request shape: { "config": { <ConfigUpdateRequest> } }
    svr_.Put(
        "/api/config",
        [this](const httplib::Request& req, httplib::Response& res) {
            try {
                auto json = nlohmann::json::parse(req.body);
                auto cfg = json.value("config", nlohmann::json::object());
                if(!cfg.is_object() || cfg.empty()) {
                    res.status = 400;
                    res.set_content(
                        nlohmann::json{
                            {"status", to_string(response_status::error)},
                            {"error", "Missing or empty 'config' object"}
                        }.dump(),
                        "application/json"
                    );
                    return;
                }
                auto [ok, err] = adapter_utils::apply_config(runner_, cfg);
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
                std::cout << "[HTTP] PUT /api/config: " << cfg.dump() << "\n";
                res.set_content(runner_.get_queue_status().dump(), "application/json");
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

void HttpAdapter::setup_management_routes() {
    if(!management_) {
        std::cout << "[HTTP] Management API not provided, skipping management routes\n";
        return;
    }

    // GET /api/adapters - list available and running adapters
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

    // GET /api/adapters/available - list only adapters that can be loaded
    svr_.Get(
        "/api/adapters/available",
        [this](const httplib::Request&, httplib::Response& res) {
            auto available = adapter_utils::filter_available_adapters(management_);
            res.set_content(
                nlohmann::json{{"adapters", available}}.dump(),
                "application/json"
            );
        }
    );

    // POST /api/adapters/:name - load and start an adapter (config in body)
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

    // DELETE /api/adapters/:name - stop and unload an adapter
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

    // GET /api/resource-providers - list available and running resource providers
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

    // GET /api/resource-providers/available - list only providers that can be loaded
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

    // POST /api/resource-providers/:name - load and start a resource provider
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

    // DELETE /api/resource-providers/:name - stop and unload a resource provider
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

    // Derive progress endpoint from register_url (scheme://host:port + /api/callback/progress).
    // If register_url is empty (orchestrator integration disabled), progress thread stays idle.
    if(!config_.register_url.empty()) {
        std::string scheme, host_port, _path;
        parse_url(config_.register_url, scheme, host_port, _path);
        progress_url_ = scheme + "://" + host_port + "/api/callback/progress";
        progress_thread_ = std::thread([this]() { progress_worker_loop(); });
        // Do not log progress_url_ - it inherits user-info from register_url.
        std::cout << "[HTTP] Progress publisher started\n";
    }

    server_thread_ = std::thread(
        [this]() {
            // Do not log any part of auth_token_ - even a tail can help an
            // attacker who got partial visibility (and the token is generated
            // from a 32-bit-seeded PRNG, so any leaked bytes accelerate state
            // recovery). Operators can read the token from the management
            // config snapshot if they need to verify deployment.
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
        throw std::runtime_error("[HTTP] Failed to listen - " + reason);
    }
    std::cout << "[HTTP] Listening on 0.0.0.0:" << config_.port << "\n";

}

void HttpAdapter::notify_online() {
    if(config_.register_url.empty()) return;

    if(config_.api_key.empty()) {
        stop();
        throw std::runtime_error("[HTTP] apiKey is required for orchestrator registration (set in http.json)");
    }
    if(!do_register()) {
        stop();
        throw std::runtime_error("[HTTP] Registration with orchestrator failed");
    }
}

void HttpAdapter::stop() {
    if(stop_.exchange(true)) return;
    alive_->store(false);
    do_deregister();
    svr_.stop();
    if(server_thread_.joinable()) {
        server_thread_.join();
    }

    // Stop progress publisher. The worker is bounded by HTTP timeouts in
    // make_client() (5s connect + 5s read = 10s max for the in-flight POST),
    // after which it sees progress_stop_ and exits. A naive joinable()-spin
    // loop here would never see joinable() flip to false on its own - only
    // join()/detach() change it - so this needs an actual timed-join via
    // promise/future from the worker side.
    if(progress_thread_.joinable()) {
        progress_stop_ = true;
        progress_cv_.notify_all();
        progress_thread_.join();
    }
}

// ============================================================================
// Progress publisher (separate thread, bounded queue, drop-oldest on overflow)
// ============================================================================

void HttpAdapter::enqueue_progress(nlohmann::json event) {
    // Skip if progress endpoint isn't wired (no register_url).
    if(progress_url_.empty()) return;
    {
        std::lock_guard lock(progress_mutex_);
        if(progress_queue_.size() >= kProgressQueueMax) {
            progress_queue_.pop_front();  // drop oldest
            progress_dropped_queue_.fetch_add(1, std::memory_order_relaxed);
            std::cerr << "[HTTP] progress queue full - dropped oldest event\n";
        }
        progress_queue_.push_back(std::move(event));
    }
    progress_cv_.notify_one();
}

void HttpAdapter::progress_worker_loop() {
    if(progress_url_.empty()) return;

    // Parse the progress URL once.
    std::string scheme, host_port, path;
    parse_url(progress_url_, scheme, host_port, path);

    // Factory: build a fresh keep-alive client. Used both initially and to
    // recover from a stale connection (orchestrator-side keep-alive timeout
    // closes the socket while we're idle between bursts; the next write goes
    // to a dead socket and Post() returns Error::Read).
    auto make_client = [&]() {
        auto c = std::make_unique<httplib::Client>(scheme + "://" + host_port);
        c->set_connection_timeout(5);
        c->set_read_timeout(5);
        if(!config_.api_key.empty())
            c->set_default_headers({{"X-API-Key", config_.api_key}});
        return c;
    };

    auto client = make_client();
    auto period_start = std::chrono::steady_clock::now();
    uint64_t period_published = 0;
    uint64_t period_dropped   = 0;

    auto log_period_stats = [&](bool force) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - period_start).count();
        // Throttle: every 60s with activity, or on shutdown ("force").
        bool active = period_published > 0 || period_dropped > 0;
        if(!force && !(active && elapsed >= 60)) return;
        if(active) {
            uint64_t total = period_published + period_dropped;
            double drop_pct = total ? (100.0 * period_dropped / total) : 0.0;
            std::cout << "[HTTP] progress stats (last " << elapsed << "s): "
                << period_published << " ok, " << period_dropped << " dropped on send ("
                << drop_pct << "%)\n";
        }
        period_published = 0;
        period_dropped   = 0;
        period_start     = now;
    };

    while(true) {
        nlohmann::json event;
        {
            std::unique_lock lock(progress_mutex_);
            progress_cv_.wait_for(lock, std::chrono::seconds(5), [this]() {
                return progress_stop_.load() || !progress_queue_.empty();
            });
            if(progress_stop_.load() && progress_queue_.empty()) {
                log_period_stats(true);
                uint64_t pub = progress_published_.load(std::memory_order_relaxed);
                uint64_t snd = progress_dropped_send_.load(std::memory_order_relaxed);
                uint64_t que = progress_dropped_queue_.load(std::memory_order_relaxed);
                std::cout << "[HTTP] progress publisher exiting. Totals: "
                    << pub << " published, " << snd << " dropped on send, "
                    << que << " dropped by queue overflow\n";
                return;
            }
            if(progress_queue_.empty()) {
                // Spurious wakeup or 5s tick with no events - flush stats if due.
                log_period_stats(false);
                continue;
            }
            event = std::move(progress_queue_.front());
            progress_queue_.pop_front();
        }

        bool delivered = false;
        try {
            auto body = event.dump();
            auto res = client->Post(path, body, "application/json");
            // Read-error usually means stale keep-alive: write succeeded into
            // the kernel buffer, but the orchestrator had already closed the
            // socket so the read of the response got EOF/RST. Recreate the
            // client and retry once. Other errors (connection refused, etc.)
            // are non-transient at this layer and we drop.
            if(!res && res.error() == httplib::Error::Read) {
                client = make_client();
                res = client->Post(path, body, "application/json");
            }
            if(!res) {
                std::cerr << "[HTTP] progress POST failed: "
                    << httplib::to_string(res.error()) << " (event dropped)\n";
            } else if(res->status >= 400) {
                std::cerr << "[HTTP] progress POST returned HTTP " << res->status
                    << " (event dropped)\n";
            } else {
                delivered = true;
            }
        } catch(const std::exception& e) {
            std::cerr << "[HTTP] progress POST threw: " << e.what() << "\n";
        }

        if(delivered) {
            progress_published_.fetch_add(1, std::memory_order_relaxed);
            ++period_published;
        } else {
            progress_dropped_send_.fetch_add(1, std::memory_order_relaxed);
            ++period_dropped;
        }
        log_period_stats(false);
    }
}

// ============================================================================
// Registration / deregistration via HTTP POST
// ============================================================================

/// Parse "scheme://host:port/path" into components. Defined here, but used
/// earlier in start()/progress_worker_loop() - forward-declared at top of file.
static void parse_url(
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

bool HttpAdapter::do_register() {
    if(config_.register_url.empty()) return true;

    // Do not log register_url - it can carry user-info if the operator put
    // credentials in the URL. The operator can read it from http.json directly.
    std::cout << "[HTTP] Attempting registration via POST"
        << " (timeout: " << config_.registration_timeout_sec << "s)...\n";

    std::string scheme, host_port, path;
    parse_url(config_.register_url, scheme, host_port, path);

    auto body = adapter_utils::build_node_event(node_event_type::online, config_.node_id, runner_, management_);

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

void HttpAdapter::do_deregister() {
    if(config_.register_url.empty() || !registered_) return;

    std::cout << "[HTTP] Sending deregister to orchestrator...\n";

    std::string scheme, host_port, path;
    parse_url(config_.register_url, scheme, host_port, path);

    auto body = adapter_utils::build_node_event(node_event_type::offline, config_.node_id, runner_, management_);

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