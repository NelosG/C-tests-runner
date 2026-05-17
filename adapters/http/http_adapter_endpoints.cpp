// HTTP route handlers - split out of http_adapter.cpp to keep the lifecycle /
// registration / progress-publisher logic readable on its own. Defines two
// HttpAdapter private methods that are called from the constructor.
#include <http_adapter.h>
#include <adapter_status.h>
#include <adapter_utils.h>
#include <api_types.h>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <time_utils.h>

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

                // node_id is held by TestRunnerService (set at startup) and
                // threaded into Pipeline::execute() - no need to inject here.

                // Pre-extract jobId for the "received" progress event (fall through
                // to runner-generated id if absent - same logic as JobQueue).
                std::string client_job_id = json.value("jobId", "");
                if(!client_job_id.empty()) {
                    enqueue_progress(
                        {
                            {"jobId", client_job_id},
                            {"nodeId", config_.node_id},
                            {"phase", "received"},
                            {"timestamp", now_iso8601()}
                        }
                    );
                }

                // alive_ guards against use-after-free: if HttpAdapter::stop()
                // has set alive_ = false, the JobQueue worker calling us must
                // not touch this->progress_queue_ / progress_mutex_ - those
                // are members and may already be in destruction. on_complete
                // uses the same pattern.
                auto alive_for_progress = alive_;
                auto on_progress = [this, alive_for_progress](const nlohmann::json& event) {
                    if(!alive_for_progress->load()) return;
                    enqueue_progress(event);
                };

                auto job_id = runner_.submit(std::move(json), std::move(on_complete), std::move(on_progress));
                // If the client did not supply jobId, the runner generated one - emit "received" now.
                if(client_job_id.empty()) {
                    enqueue_progress(
                        {
                            {"jobId", job_id},
                            {"nodeId", config_.node_id},
                            {"phase", "received"},
                            {"timestamp", now_iso8601()}
                        }
                    );
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

    // /api/health and /api/health/ - both registered so the route also exists
    // when a probe appends a trailing slash. The auth middleware is already
    // forgiving about either; the route table needs the same.
    auto health_handler = [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({"status":"ok"})", "application/json");
    };
    svr_.Get("/api/health", health_handler);
    svr_.Get("/api/health/", health_handler);

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

            // Sub-path collision guard: cpp-httplib resolves GET vs POST
            // independently, so `POST /api/adapters/available` lands here
            // with adapter="available". Reject so we don't try to dlopen
            // a plugin named "available".
            if(adapter == "available") {
                res.status = 400;
                res.set_content(
                    R"({"error":"'available' is reserved; use GET /api/adapters/available"})",
                    "application/json"
                );
                return;
            }

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

            // See the adapters analogue: reject "available" so the GET-only
            // sub-resource doesn't get reinterpreted as a load request.
            if(provider == "available") {
                res.status = 400;
                res.set_content(
                    R"({"error":"'available' is reserved; use GET /api/resource-providers/available"})",
                    "application/json"
                );
                return;
            }

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
