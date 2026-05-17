// Include http_adapter.h first so httplib.h pulls in winsock2.h before windows.h
#include <http_adapter.h>
#include <adapter_utils.h>
#include <chrono>
#include <iostream>
#include <register_adapter.h>

// Route handlers live in http_adapter_endpoints.cpp.

/// Forward declaration: definition lives further down (used by start() and
/// progress_worker_loop() which appear before it in source order).
static void parse_url(
    const std::string& raw,
    std::string& scheme,
    std::string& host_port,
    std::string& path
);

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
            // Accept both /api/health and /api/health/ - cpp-httplib does not
            // normalize trailing slashes, and probes from various monitoring
            // tools (Docker HEALTHCHECK, k8s probe, curl) may add one.
            if(req.method == "GET"
                && (req.path == "/api/health" || req.path == "/api/health/")) {
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
            // Do not log any part of auth_token_ - even a partial tail is a
            // gift to an attacker correlating with other side channels.
            // Operators can read the token from the management config
            // snapshot if they need to verify deployment.
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

    // Start the periodic re-registration heartbeat. Only meaningful after the
    // initial register succeeded; on failure we never get here.
    heartbeat_thread_ = std::thread([this]() { heartbeat_worker_loop(); });
}

void HttpAdapter::stop() {
    if(stop_.exchange(true)) return;
    alive_->store(false);

    // Stop the heartbeat thread BEFORE deregister: otherwise a heartbeat POST
    // could land between deregister and the orchestrator marking us offline,
    // re-creating us as online for ~5 min until we miss the next beat.
    if(heartbeat_thread_.joinable()) {
        heartbeat_stop_ = true;
        heartbeat_cv_.notify_all();
        heartbeat_thread_.join();
    }

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
// Heartbeat: periodic re-POST to register_url so a restarted orchestrator
// repopulates its node table without engine restart. Idempotent on the
// orchestrator side (repeat POST = "still online", not "new registration").
// ============================================================================

void HttpAdapter::heartbeat_worker_loop() {
    if(config_.register_url.empty()) return;
    while(true) {
        std::unique_lock lock(heartbeat_mutex_);
        // Wake on stop OR after the interval, whichever comes first.
        heartbeat_cv_.wait_for(
            lock,
            std::chrono::seconds(kHeartbeatIntervalSec),
            [this] { return heartbeat_stop_.load(); }
        );
        if(heartbeat_stop_.load()) return;
        lock.unlock();

        // do_register() is best-effort: returns false on transport / 4xx / 5xx
        // and logs. We don't propagate the failure here - the next heartbeat
        // tick will try again. registered_ stays true regardless so
        // do_deregister() still fires on shutdown.
        do_register();
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
    uint64_t period_dropped = 0;

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
        period_dropped = 0;
        period_start = now;
    };

    while(true) {
        nlohmann::json event;
        {
            std::unique_lock lock(progress_mutex_);
            progress_cv_.wait_for(
                lock,
                std::chrono::seconds(5),
                [this]() {
                    return progress_stop_.load() || !progress_queue_.empty();
                }
            );
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
