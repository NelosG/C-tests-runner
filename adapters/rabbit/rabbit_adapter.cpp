// Include rabbit_adapter.h first so amqpcpp pulls in winsock2.h before windows.h
#include <rabbit_adapter.h>
#include <adapter_status.h>
#include <adapter_utils.h>
#include <algorithm>
#include <api_types.h>
#include <chrono>
#include <control_type.h>
#include <filesystem>
#include <future>
#include <iostream>
#include <thread>
#include <register_adapter.h>
#include <time_utils.h>
#include <uv_amqp_handler.h>

// ============================================================================
// Construction / Destruction
// ============================================================================

RabbitAdapter::RabbitAdapter(
    TestRunnerService& runner,
    const ManagementAPI* management,
    const AdapterContext& ctx
)
    : runner_(runner), management_(management),
      alive_(std::make_shared<std::atomic<bool>>(true)) {
    const auto& config = ctx.config;
    setupControlHandlers();

    if(ctx.node_id.empty()) {
        throw std::runtime_error("[RabbitMQ] node_id is required in AdapterContext");
    }

    if(!config.contains("host") || config["host"].get<std::string>().empty()) {
        throw std::runtime_error("[RabbitMQ] Missing required 'host' in config");
    }

    config_.host = config.value("host", "localhost");
    config_.port = config.value("port", 5672);
    config_.user = config.value("user", "guest");
    config_.password = config.value("password", "guest");
    config_.vhost = config.value("vhost", "/");
    config_.connection_timeout_sec = config.value("connectionTimeoutSec", 10);

    if(config_.port < 1 || config_.port > 65535) {
        throw std::runtime_error("[RabbitMQ] Invalid port: " + std::to_string(config_.port));
    }

    config_.node_id = ctx.node_id;
}

RabbitAdapter::~RabbitAdapter() {
    try {
        RabbitAdapter::stop();
    } catch(const std::exception& e) {
        std::cerr << "[RabbitMQ] Error during shutdown: " << e.what() << "\n";
    } catch(...) {}
}

// ============================================================================
// Cross-thread Communication
// ============================================================================

void RabbitAdapter::postToEventLoop(std::function<void()> fn) {
    {
        std::lock_guard lock(pending_mutex_);
        pending_callbacks_.push(std::move(fn));
    }
    uv_async_send(&async_handle_);
}

void RabbitAdapter::onAsyncCallback(uv_async_t* handle) {
    auto* self = static_cast<RabbitAdapter*>(handle->data);

    std::queue<std::function<void()>> callbacks;
    {
        std::lock_guard lock(self->pending_mutex_);
        std::swap(callbacks, self->pending_callbacks_);
    }
    while(!callbacks.empty()) {
        try {
            callbacks.front()();
        } catch(const std::exception& e) {
            std::cerr << "[RabbitMQ] Async callback error: " << e.what() << "\n";
        }
        callbacks.pop();
    }
}

// ============================================================================
// Event Loop
// ============================================================================

void RabbitAdapter::eventLoopMain() {
    uv_run(&loop_, UV_RUN_DEFAULT);
}

// ============================================================================
// Start / Stop
// ============================================================================

void RabbitAdapter::start() {
    std::cout << "[RabbitMQ] Starting async adapter for node " << config_.node_id
        << " (" << config_.host << ":" << config_.port << ")\n";

    // Initialize event loop
    uv_loop_init(&loop_);
    loop_initialized_ = true;

    // Initialize async handle for cross-thread callbacks
    uv_async_init(&loop_, &async_handle_, onAsyncCallback);
    async_handle_.data = this;

    // Create handler and initiate TCP connection
    handler_ = std::make_unique<UvAmqpHandler>(&loop_);

    // Use promise to synchronize: wait until AMQP connection is ready or fails
    auto ready_promise = std::make_shared<std::promise<bool>>();
    auto ready_future = ready_promise->get_future();

    handler_->setReadyCallback(
        [this, ready_promise]() {
            try {
                setupChannels();
                ready_promise->set_value(true);
            } catch(const std::exception& e) {
                std::cerr << "[RabbitMQ] Setup failed: " << e.what() << "\n";
                ready_promise->set_value(false);
            }
        }
    );

    handler_->setErrorCallback(
        [ready_promise](const char* msg) {
            std::cerr << "[RabbitMQ] Connection error: " << msg << "\n";
            try {
                ready_promise->set_value(false);
            } catch(...) {
                // Promise already satisfied (error after ready)
            }
        }
    );

    // Initiate TCP connection (async)
    handler_->connect(config_.host, config_.port);

    // Create AMQP connection (protocol handshake happens when TCP connects)
    AMQP::Login login(config_.user, config_.password);
    connection_ = std::make_unique<AMQP::Connection>(handler_.get(), login, config_.vhost);
    handler_->setConnection(connection_.get());

    // Start event loop thread
    event_loop_thread_ = std::thread([this]() { eventLoopMain(); });

    // Wait for connection ready (with timeout)
    if(ready_future.wait_for(std::chrono::seconds(config_.connection_timeout_sec)) ==
        std::future_status::timeout) {
        throw std::runtime_error("[RabbitMQ] Connection timeout");
    }

    if(!ready_future.get()) {
        throw std::runtime_error("[RabbitMQ] Connection failed");
    }

    started_ = true;
    std::cout << "[RabbitMQ] Started: event loop + shared JobQueue\n";
}

void RabbitAdapter::notifyOnline() {
    postToEventLoop([this]() { publishNodeEvent(node_event_type::online); });
}

void RabbitAdapter::stop() {
    if(stop_.exchange(true)) return;

    std::cout << "[RabbitMQ] Shutting down...\n";

    // Mark as not alive so in-flight completion callbacks become no-ops
    alive_->store(false);

    if(started_) {
        // Broadcast offline event and stop event loop
        auto done_promise = std::make_shared<std::promise<void>>();
        auto done_future = done_promise->get_future();

        postToEventLoop(
            [this, done_promise]() {
                try {
                    publishNodeEvent(node_event_type::offline);
                } catch(const std::exception& e) {
                    std::cerr << "[RabbitMQ] Failed to publish offline event: " << e.what() << "\n";
                } catch(...) {}

                // Close AMQP channels and connection
                task_channel_.reset();
                status_channel_.reset();
                publish_channel_.reset();

                if(connection_) {
                    connection_->close();
                }

                // Close handler and async handle
                if(handler_) handler_->shutdown();
                uv_close(reinterpret_cast<uv_handle_t*>(&async_handle_), nullptr);

                done_promise->set_value();
            }
        );

        if(done_future.wait_for(std::chrono::seconds(5)) == std::future_status::timeout) {
            std::cerr << "[RabbitMQ] Shutdown timed out after 5 seconds\n";
        }
    } else if(loop_initialized_) {
        // start() failed but loop was initialized
        if(handler_) handler_->shutdown();
        if(!uv_is_closing(reinterpret_cast<uv_handle_t*>(&async_handle_))) {
            uv_close(reinterpret_cast<uv_handle_t*>(&async_handle_), nullptr);
        }
    }

    if(event_loop_thread_.joinable()) {
        event_loop_thread_.join();
    }

    if(loop_initialized_) {
        // Run remaining close callbacks
        uv_run(&loop_, UV_RUN_NOWAIT);
        uv_loop_close(&loop_);
        loop_initialized_ = false;
    }

    connection_.reset();
    handler_.reset();

    std::cout << "[RabbitMQ] Shut down complete\n";
}

// ============================================================================
// Channel Setup (event loop thread)
// ============================================================================

void RabbitAdapter::setupChannels() {
    publish_channel_ = std::make_unique<AMQP::Channel>(connection_.get());
    task_channel_ = std::make_unique<AMQP::Channel>(connection_.get());
    status_channel_ = std::make_unique<AMQP::Channel>(connection_.get());

    // Set up error handlers with channel names for diagnostics
    publish_channel_->onError(
        [](const char* msg) {
            std::cerr << "[RabbitMQ] Publish channel error: " << msg << "\n";
        }
    );
    task_channel_->onError(
        [](const char* msg) {
            std::cerr << "[RabbitMQ] Task channel error: " << msg << "\n";
        }
    );
    status_channel_->onError(
        [](const char* msg) {
            std::cerr << "[RabbitMQ] Status channel error: " << msg << "\n";
        }
    );

    declareTopology();
    startConsumers();
}

void RabbitAdapter::declareTopology() {
    std::cout << "[RabbitMQ] Declaring topology...\n";
    auto& ch = *publish_channel_;

    // Exchange: test.direct (direct) — tasks & results
    ch.declareExchange("test.direct", AMQP::direct, AMQP::durable);

    // Exchange: node.fanout (fanout) — node events & control
    ch.declareExchange("node.fanout", AMQP::fanout, AMQP::durable);

    // Exchange: node.control.direct (direct) — targeted control messages per nodeId
    ch.declareExchange("node.control.direct", AMQP::direct, AMQP::durable);

    // Queue: test.tasks (single queue, mode is in message body)
    ch.declareQueue("test.tasks", AMQP::durable);
    ch.bindQueue("test.direct", "test.tasks", "correctness");
    ch.bindQueue("test.direct", "test.tasks", "performance");
    ch.bindQueue("test.direct", "test.tasks", "all");

    // Queue: test.results
    ch.declareQueue("test.results", AMQP::durable);
    ch.bindQueue("test.direct", "test.results", "results");

    // Queue: node.events
    ch.declareQueue("node.events", AMQP::durable);
    ch.bindQueue("node.fanout", "node.events", "");
}

void RabbitAdapter::startConsumers() {
    std::cout << "[RabbitMQ] Starting consumers...\n";
    // Task consumer (concurrency managed by JobQueue, lane determined by mode in message body)
    task_channel_->consume("test.tasks")
                 .onReceived(
                     [this](const AMQP::Message& msg, uint64_t tag, bool) {
                         onTaskReceived(msg, tag, task_channel_.get());
                     }
                 )
                 .onError(
                     [](const char* msg) {
                         std::cerr << "[RabbitMQ] Task consume error: " << msg << "\n";
                     }
                 );

    // Control listener: exclusive auto-delete queue bound to node.fanout
    status_channel_->declareQueue(AMQP::exclusive + AMQP::autodelete)
                   .onSuccess(
                       [this](const std::string& name, uint32_t, uint32_t) {
                           status_channel_->bindQueue("node.fanout", name, "");
                           status_channel_->bindQueue("node.control.direct", name, config_.node_id);
                           status_channel_->consume(name, AMQP::noack)
                                          .onReceived(
                                              [this](const AMQP::Message& msg, uint64_t tag, bool redelivered) {
                                                  onControlMessage(msg, tag, redelivered);
                                              }
                                          )
                                          .onError(
                                              [](const char* msg) {
                                                  std::cerr << "[RabbitMQ] Control consume error: " << msg << "\n";
                                              }
                                          );
                           std::cout << "[RabbitMQ] Control listener started on " << name << "\n";
                       }
                   );
}

// ============================================================================
// Message Handlers (event loop thread)
// ============================================================================

void RabbitAdapter::onTaskReceived(
    const AMQP::Message& msg,
    uint64_t tag,
    AMQP::Channel* channel
) {
    if(stop_) {
        channel->reject(tag);
        return;
    }

    std::string body(msg.body(), msg.bodySize());
    nlohmann::json task;
    try {
        task = nlohmann::json::parse(body);
    } catch(const std::exception& e) {
        std::cerr << "[RabbitMQ] Invalid task JSON: " << e.what() << "\n";
        channel->ack(tag);
        return;
    }

    // Validate required fields
    const bool has_solution = task.contains("solutionSource") && task.contains("solutionSourceType");
    const bool has_tests = task.contains("testSource") && task.contains("testSourceType");
    if(!has_solution || !has_tests) {
        std::cerr << "[RabbitMQ] Task rejected: missing solution or test source\n";
        if(msg.hasReplyTo()) {
            publishReply(
                msg.replyTo(),
                {
                    {"error", "Missing required: solutionSourceType + solutionSource and testSourceType + testSource"},
                    {"status", to_string(response_status::rejected)}
                },
                msg.hasCorrelationID() ? msg.correlationID() : ""
            );
        }
        channel->ack(tag);
        return;
    }

    if(!task.contains("testId") || task["testId"].get<std::string>().empty()) {
        std::cerr << "[RabbitMQ] Task rejected: missing testId\n";
        if(msg.hasReplyTo()) {
            publishReply(
                msg.replyTo(),
                {
                    {"error", "Missing required field: testId"},
                    {"status", to_string(response_status::rejected)}
                },
                msg.hasCorrelationID() ? msg.correlationID() : ""
            );
        }
        channel->ack(tag);
        return;
    }

    std::string mode = task.value("mode", to_string(test_mode::correctness));
    if(!is_valid_test_mode(mode)) {
        std::cerr << "[RabbitMQ] Task rejected: invalid mode '" << mode << "'\n";
        if(msg.hasReplyTo()) {
            publishReply(
                msg.replyTo(),
                {
                    {"error", "Invalid mode: '" + mode + "'. Must be 'correctness', 'performance', or 'all'"},
                    {"status", to_string(response_status::rejected)}
                },
                msg.hasCorrelationID() ? msg.correlationID() : ""
            );
        }
        channel->ack(tag);
        return;
    }

    // Validate threads (if provided)
    if(task.contains("threads")) {
        int threads = task["threads"].get<int>();
        int max_hw = static_cast<int>(std::thread::hardware_concurrency()) * 2;
        if(threads < 1 || threads > max_hw) {
            std::cerr << "[RabbitMQ] Task rejected: invalid threads " << threads << "\n";
            if(msg.hasReplyTo()) {
                publishReply(
                    msg.replyTo(),
                    {
                        {
                            "error",
                            "Invalid threads: " + std::to_string(threads) + ". Must be 1.." + std::to_string(max_hw)
                        },
                        {"status", to_string(response_status::rejected)}
                    },
                    msg.hasCorrelationID() ? msg.correlationID() : ""
                );
            }
            channel->ack(tag);
            return;
        }
    }

    // Validate numaNode (if provided)
    if(task.contains("numaNode")) {
        int numa = task["numaNode"].get<int>();
        if(numa < -1) {
            std::cerr << "[RabbitMQ] Task rejected: invalid numaNode " << numa << "\n";
            if(msg.hasReplyTo()) {
                publishReply(
                    msg.replyTo(),
                    {
                        {"error", "Invalid numaNode: must be >= -1"},
                        {"status", to_string(response_status::rejected)}
                    },
                    msg.hasCorrelationID() ? msg.correlationID() : ""
                );
            }
            channel->ack(tag);
            return;
        }
    }

    // Validate memoryLimitMb (if provided)
    if(task.contains("memoryLimitMb")) {
        auto& v = task["memoryLimitMb"];
        if(!v.is_number_integer() || v.get<long long>() < 0) {
            std::cerr << "[RabbitMQ] Task rejected: invalid memoryLimitMb\n";
            if(msg.hasReplyTo()) {
                publishReply(
                    msg.replyTo(),
                    {
                        {"error", "memoryLimitMb must be non-negative integer"},
                        {"status", to_string(response_status::rejected)}
                    },
                    msg.hasCorrelationID() ? msg.correlationID() : ""
                );
            }
            channel->ack(tag);
            return;
        }
    }

    // Embed AMQP reply metadata for the executor
    if(msg.hasReplyTo()) task["_reply_to"] = msg.replyTo();
    if(msg.hasCorrelationID()) task["_correlation_id"] = msg.correlationID();

    std::string job_id = task.value("jobId", "");
    if(job_id.empty()) {
        job_id = generateJobId();
        task["jobId"] = job_id;
    } else {
        // Validate jobId format: only alphanumeric, hyphens, underscores
        // Prevents path traversal via crafted jobId (used in temp dir names)
        static const auto is_safe_id = [](const std::string& s) {
            return std::all_of(
                s.begin(),
                s.end(),
                [](char c) {
                    return std::isalnum(c) || c == '-' || c == '_';
                }
            );
        };
        if(!is_safe_id(job_id)) {
            std::cerr << "[RabbitMQ] Task rejected: invalid jobId format\n";
            if(msg.hasReplyTo()) {
                publishReply(
                    msg.replyTo(),
                    {
                        {"error", "Invalid jobId: must contain only alphanumeric, hyphen, underscore"},
                        {"status", to_string(response_status::rejected)}
                    },
                    msg.hasCorrelationID() ? msg.correlationID() : ""
                );
            }
            channel->ack(tag);
            return;
        }
    }

    // Ack immediately (task accepted into queue)
    channel->ack(tag);

    // Extract solution name for acceptance reply
    std::string solution_name;
    if(task.contains("solutionSource")) {
        const auto& src = task["solutionSource"];
        if(src.contains("path"))
            solution_name = std::filesystem::path(src.value("path", "")).filename().string();
        else if(src.contains("url"))
            solution_name = src.value("url", "");
    }
    long long memory_limit_mb = task.value("memoryLimitMb", runner_.default_memory_limit_mb());
    std::cout << "[RabbitMQ] " << mode << " task: " << job_id << "\n";

    // Build completion callback for AMQP result publishing
    auto alive = alive_;
    auto node_id = config_.node_id;
    auto start_time = std::chrono::steady_clock::now();

    auto on_complete = [this, alive, job_id, node_id, start_time](
        const nlohmann::json& result
    ) {
        if(!alive->load()) return;
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time
        ).count();

        auto msg = adapter_utils::buildCompletionResult(result, job_id, node_id, duration);

        postToEventLoop(
            [this, msg = std::move(msg)]() {
                publish("test.direct", "results", msg);
            }
        );
    };

    // Submit to shared job queue (lane determined by "mode" field)
    runner_.submit(std::move(task), std::move(on_complete));

    // Send immediate RPC acceptance AFTER submit (job now exists in queue)
    if(msg.hasReplyTo()) {
        auto info = runner_.getJobInfo(job_id);
        publishReply(
            msg.replyTo(),
            {
                {"jobId", job_id},
                {"status", to_string(job_status::queued)},
                {"nodeId", config_.node_id},
                {"position", info.queue_position},
                {"mode", mode},
                {"solution", solution_name},
                {"memoryLimitMb", memory_limit_mb},
                {"timestamp", nowISO8601()}
            },
            msg.hasCorrelationID() ? msg.correlationID() : ""
        );
    }
}

void RabbitAdapter::setupControlHandlers() {
    control_handlers_[control_type::queue_status] = [this](const nlohmann::json&, const ReplyFn& reply) {
        reply(response_type(control_type::queue_status), runner_.getQueueStatus());
        std::cout << "[RabbitMQ] Responded to queueStatus\n";
    };

    control_handlers_[control_type::status_request] = [this](const nlohmann::json&, const ReplyFn& reply) {
        auto status = adapter_utils::buildNodeEvent(node_event_type::info, config_.node_id, runner_, management_);
        reply("statusResponse", status);
        std::cout << "[RabbitMQ] Responded to statusRequest\n";
    };

    control_handlers_[control_type::list_adapters] = [this](const nlohmann::json&, const ReplyFn& reply) {
        if(!management_) {
            std::cerr << "[RabbitMQ] Management API not available for listAdapters\n";
            return;
        }
        const char* json_str = management_->list_adapters(management_->context);
        if(!json_str) {
            reply(
                response_type(control_type::list_adapters),
                {{"adapters", nlohmann::json::array()}, {"error", "Failed to list adapters"}}
            );
            return;
        }
        auto adapters = nlohmann::json::parse(json_str);
        management_->free_string(management_->context, json_str);
        reply(response_type(control_type::list_adapters), {{"adapters", adapters}});
        std::cout << "[RabbitMQ] Responded to listAdapters\n";
    };

    control_handlers_[control_type::load_adapter] = [this](const nlohmann::json& parsed, const ReplyFn& reply) {
        if(!management_) {
            std::cerr << "[RabbitMQ] Management API not available for loadAdapter\n";
            return;
        }
        std::string adapter_name = parsed.value("adapter", "");
        nlohmann::json config = parsed.value("config", nlohmann::json::object());
        bool ok = management_->load_adapter(management_->context, adapter_name.c_str(), config);
        nlohmann::json resp = {
            {"adapter", adapter_name},
            {"status", to_string(ok ? adapter_status::started : adapter_status::failed)}
        };
        if(!ok) resp["error"] = "Failed to load adapter '" + adapter_name + "'. Check server logs.";
        reply(response_type(control_type::load_adapter), resp);
        std::cout << "[RabbitMQ] loadAdapter '" << adapter_name << "': "
            << (ok ? "ok" : "failed") << "\n";
    };

    control_handlers_[control_type::list_available_adapters] = [this](const nlohmann::json&, const ReplyFn& reply) {
        if(!management_) {
            std::cerr << "[RabbitMQ] Management API not available for listAvailableAdapters\n";
            return;
        }
        auto available = adapter_utils::filterAvailableAdapters(management_);
        reply(response_type(control_type::list_available_adapters), {{"adapters", available}});
        std::cout << "[RabbitMQ] Responded to listAvailableAdapters\n";
    };

    control_handlers_[control_type::unload_adapter] = [this](const nlohmann::json& parsed, const ReplyFn& reply) {
        if(!management_) {
            std::cerr << "[RabbitMQ] Management API not available for unloadAdapter\n";
            return;
        }
        std::string adapter_name = parsed.value("adapter", "");
        bool ok = management_->unload_adapter(management_->context, adapter_name.c_str());
        nlohmann::json resp = {
            {"adapter", adapter_name},
            {"status", to_string(ok ? adapter_status::stopped : adapter_status::failed)}
        };
        if(!ok) resp["error"] = "Adapter '" + adapter_name + "' not found or not running";
        reply(response_type(control_type::unload_adapter), resp);
        std::cout << "[RabbitMQ] unloadAdapter '" << adapter_name << "': "
            << (ok ? "ok" : "failed") << "\n";
    };

    control_handlers_[control_type::update_config] = [this](
        const nlohmann::json& parsed,
        const ReplyFn& reply
    ) {
        auto cfg = parsed.value("config", nlohmann::json::object());
        if(!cfg.is_object() || cfg.empty()) {
            reply(
                response_type(control_type::update_config),
                {{"status", to_string(response_status::error)}, {"error", "Missing or empty 'config' object"}}
            );
            return;
        }
        auto [ok, err] = adapter_utils::applyConfig(runner_, cfg);
        if(!ok) {
            reply(
                response_type(control_type::update_config),
                {{"status", to_string(response_status::error)}, {"error", err}}
            );
            return;
        }
        std::cout << "[RabbitMQ] updateConfig: " << cfg.dump() << "\n";
        reply(response_type(control_type::update_config), runner_.getQueueStatus());
    };

    control_handlers_[control_type::cancel_job] = [this](const nlohmann::json& parsed, const ReplyFn& reply) {
        std::string job_id = parsed.value("jobId", "");
        if(job_id.empty()) {
            reply(
                response_type(control_type::cancel_job),
                {{"status", to_string(response_status::error)}, {"error", "Missing jobId"}}
            );
            return;
        }
        bool ok = runner_.cancel(job_id);
        nlohmann::json resp = {
            {"jobId", job_id},
            {"status", ok ? to_string(job_status::cancelled) : to_string(response_status::error)}
        };
        if(!ok) resp["error"] = "Cannot cancel job (not queued or not found)";
        reply(response_type(control_type::cancel_job), resp);
        std::cout << "[RabbitMQ] cancelJob '" << job_id << "': "
            << (ok ? "cancelled" : "failed") << "\n";
    };

    control_handlers_[control_type::get_job_info] = [this](const nlohmann::json& parsed, const ReplyFn& reply) {
        std::string job_id = parsed.value("jobId", "");
        if(job_id.empty()) {
            reply(
                response_type(control_type::get_job_info),
                {{"status", to_string(response_status::error)}, {"error", "Missing jobId"}}
            );
            return;
        }
        try {
            auto info = runner_.getJobInfo(job_id);
            reply(response_type(control_type::get_job_info), adapter_utils::buildJobInfoJson(info));
        } catch(const std::exception& e) {
            reply(
                response_type(control_type::get_job_info),
                {
                    {"jobId", job_id},
                    {"status", to_string(response_status::error)},
                    {"error", e.what()}
                }
            );
        }
        std::cout << "[RabbitMQ] getJobInfo '" << job_id << "'\n";
    };

    control_handlers_[control_type::list_resource_providers] = [this](const nlohmann::json&, const ReplyFn& reply) {
        if(!management_) {
            std::cerr << "[RabbitMQ] Management API not available for listResourceProviders\n";
            return;
        }
        const char* json_str = management_->list_resource_providers(management_->context);
        if(!json_str) {
            reply(
                response_type(control_type::list_resource_providers),
                {{"providers", nlohmann::json::array()}, {"error", "Failed to list resource providers"}}
            );
            return;
        }
        auto providers = nlohmann::json::parse(json_str);
        management_->free_string(management_->context, json_str);
        reply(response_type(control_type::list_resource_providers), {{"providers", providers}});
        std::cout << "[RabbitMQ] Responded to listResourceProviders\n";
    };

    control_handlers_[control_type::list_available_resource_providers] = [this](
        const nlohmann::json&,
        const ReplyFn& reply
    ) {
            if(!management_) {
                std::cerr << "[RabbitMQ] Management API not available for listAvailableResourceProviders\n";
                return;
            }
            const char* json_str = management_->list_available_resource_providers(management_->context);
            if(!json_str) {
                reply(
                    response_type(control_type::list_available_resource_providers),
                    {{"providers", nlohmann::json::array()}, {"error", "Failed to list available resource providers"}}
                );
                return;
            }
            auto providers = nlohmann::json::parse(json_str);
            management_->free_string(management_->context, json_str);
            reply(response_type(control_type::list_available_resource_providers), {{"providers", providers}});
            std::cout << "[RabbitMQ] Responded to listAvailableResourceProviders\n";
        };

    control_handlers_[control_type::load_resource_provider] = [this
        ](const nlohmann::json& parsed, const ReplyFn& reply) {
            if(!management_) {
                std::cerr << "[RabbitMQ] Management API not available for loadResourceProvider\n";
                return;
            }
            std::string provider_name = parsed.value("provider", "");
            nlohmann::json config = parsed.value("config", nlohmann::json::object());
            bool ok = management_->load_resource_provider(management_->context, provider_name.c_str(), config);
            nlohmann::json resp = {
                {"provider", provider_name},
                {"status", to_string(ok ? adapter_status::started : adapter_status::failed)}
            };
            if(!ok) resp["error"] = "Failed to load resource provider '" + provider_name + "'. Check server logs.";
            reply(response_type(control_type::load_resource_provider), resp);
            std::cout << "[RabbitMQ] loadResourceProvider '" << provider_name << "': "
                << (ok ? "ok" : "failed") << "\n";
        };

    control_handlers_[control_type::unload_resource_provider] = [this](
        const nlohmann::json& parsed,
        const ReplyFn& reply
    ) {
            if(!management_) {
                std::cerr << "[RabbitMQ] Management API not available for unloadResourceProvider\n";
                return;
            }
            std::string provider_name = parsed.value("provider", "");
            bool ok = management_->unload_resource_provider(management_->context, provider_name.c_str());
            nlohmann::json resp = {
                {"provider", provider_name},
                {"status", to_string(ok ? adapter_status::stopped : adapter_status::failed)}
            };
            if(!ok) resp["error"] = "Resource provider '" + provider_name + "' not found or not running";
            reply(response_type(control_type::unload_resource_provider), resp);
            std::cout << "[RabbitMQ] unloadResourceProvider '" << provider_name << "': "
                << (ok ? "ok" : "failed") << "\n";
        };
}

void RabbitAdapter::onControlMessage(const AMQP::Message& msg, uint64_t, bool) {
    std::string body(msg.body(), msg.bodySize());
    try {
        auto parsed = nlohmann::json::parse(body);
        std::string type = parsed.value("type", "");

        std::string target = parsed.value("nodeId", "");
        if(!target.empty() && target != config_.node_id) return;
        if(!msg.hasReplyTo()) return;

        std::string corr_id = msg.hasCorrelationID() ? msg.correlationID() : "";

        auto reply_control = [&](const std::string& resp_type, nlohmann::json extra) {
            extra["type"] = resp_type;
            extra["nodeId"] = config_.node_id;
            extra["timestamp"] = nowISO8601();
            publishReply(msg.replyTo(), extra, corr_id);
        };

        if(is_valid_control_type(type)) {
            auto it = control_handlers_.find(control_type_from_string(type));
            if(it != control_handlers_.end()) {
                it->second(parsed, reply_control);
            }
        } else if(!type.empty()) {
            std::cout << "[RabbitMQ] Unknown control command: " << type << "\n";
        }
    } catch(const std::exception& e) {
        std::cerr << "[RabbitMQ] Control message parse error: " << e.what() << "\n";
    }
}

// ============================================================================
// Publishing (event loop thread only)
// ============================================================================

void RabbitAdapter::publishReply(
    const std::string& reply_to,
    const nlohmann::json& message,
    const std::string& correlation_id
) {
    if(reply_to.empty()) return;
    publish("", reply_to, message, correlation_id);
}

void RabbitAdapter::publish(
    const std::string& exchange,
    const std::string& routing_key,
    const nlohmann::json& message,
    const std::string& correlation_id
) {
    if(!publish_channel_) return;

    std::string body = message.dump();
    AMQP::Envelope envelope(body.data(), body.size());
    envelope.setContentType("application/json");
    envelope.setDeliveryMode(2);
    if(!correlation_id.empty()) {
        envelope.setCorrelationID(correlation_id);
    }

    publish_channel_->publish(exchange, routing_key, envelope);
}

// ============================================================================
// Node Lifecycle
// ============================================================================

void RabbitAdapter::publishNodeEvent(node_event_type type) {
    auto event = adapter_utils::buildNodeEvent(type, config_.node_id, runner_, management_);
    publish("node.fanout", "", event);
}

// ============================================================================
// DLL Factory (generated by macro)
// ============================================================================

REGISTER_ADAPTER(RabbitAdapter, "rabbit")