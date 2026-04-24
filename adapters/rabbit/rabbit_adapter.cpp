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
    setup_control_handlers();

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

void RabbitAdapter::post_to_event_loop(std::function<void()> fn) {
    {
        std::lock_guard lock(pending_mutex_);
        pending_callbacks_.push(std::move(fn));
    }
    uv_async_send(&async_handle_);
}

void RabbitAdapter::on_async_callback(uv_async_t* handle) {
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

void RabbitAdapter::event_loop_main() {
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
    uv_async_init(&loop_, &async_handle_, on_async_callback);
    async_handle_.data = this;

    // Create handler and initiate TCP connection
    handler_ = std::make_unique<UvAmqpHandler>(&loop_);

    // Use promise to synchronize: wait until AMQP connection is ready or fails
    auto ready_promise = std::make_shared<std::promise<bool>>();
    auto ready_future = ready_promise->get_future();

    handler_->setReadyCallback(
        [this, ready_promise]() {
            try {
                setup_channels();
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
    event_loop_thread_ = std::thread([this]() { event_loop_main(); });

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

void RabbitAdapter::notify_online() {
    // Stateless multi-instance parallel-server uses on-demand discovery via
    // statusRequest broadcast on node.fanout - no lifecycle event published.
}

void RabbitAdapter::stop() {
    if(stop_.exchange(true)) return;

    std::cout << "[RabbitMQ] Shutting down...\n";

    // Mark as not alive so in-flight completion callbacks become no-ops
    alive_->store(false);

    bool clean_shutdown = false;

    // Unified path: as long as the event loop thread is alive, ask it to do
    // the AMQP/libuv cleanup itself (libuv handles can only be safely closed
    // from the loop's own thread). This works both for the fully-started case
    // and for start() failing mid-init (e.g. broker refused TCP connection)
    // - in that case channels/connection may be null, reset() is a no-op.
    if(event_loop_thread_.joinable()) {
        auto done_promise = std::make_shared<std::promise<void>>();
        auto done_future = done_promise->get_future();

        post_to_event_loop(
            [this, done_promise]() {
                // Stop confirm-timeout timer first so it can't fire during
                // shutdown and touch a half-torn-down channel.
                if(confirm_timer_initialized_) {
                    uv_timer_stop(&confirm_timeout_timer_);
                    uv_close(reinterpret_cast<uv_handle_t*>(&confirm_timeout_timer_), nullptr);
                    confirm_timer_initialized_ = false;
                }

                // Drop tracking maps. In-flight unacked tags will be redelivered
                // by the broker after consumer_timeout; we do NOT ack them here
                // since their results may not have been delivered to test.results.
                pending_confirms_.clear();
                job_to_tag_.clear();

                // Close AMQP channels and connection - all reset() calls are
                // safe on null unique_ptrs (start-failure path).
                task_channel_.reset();
                status_channel_.reset();
                publish_channel_.reset();
                result_channel_.reset();

                if(connection_) {
                    connection_->close();
                }

                // Close handler and async handle - closing async_handle_ is
                // what lets uv_run() exit cleanly in event_loop_main().
                if(handler_) handler_->shutdown();
                if(!uv_is_closing(reinterpret_cast<uv_handle_t*>(&async_handle_))) {
                    uv_close(reinterpret_cast<uv_handle_t*>(&async_handle_), nullptr);
                }

                done_promise->set_value();
            }
        );

        clean_shutdown = (done_future.wait_for(std::chrono::seconds(2)) != std::future_status::timeout);
        if(!clean_shutdown) {
            // Event loop not responding - wake it up and force stop.
            if(loop_initialized_) {
                uv_stop(&loop_);
                uv_async_send(&async_handle_);
            }
            // Give the event loop one more short window so we can safely
            // close the loop afterwards. libuv is strictly single-threaded.
            auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
            while(std::chrono::steady_clock::now() < deadline) {
                if(done_future.wait_for(std::chrono::milliseconds(20)) != std::future_status::timeout) {
                    clean_shutdown = true;
                    break;
                }
            }
        }
    } else {
        // No thread to coordinate with (start() failed before event_loop_thread_
        // was created, or stop() called twice). Safe to clean up from main.
        clean_shutdown = true;
    }

    // ALWAYS resolve the event loop thread before touching the loop from main.
    // Otherwise destruction of a joinable std::thread calls std::terminate.
    if(event_loop_thread_.joinable()) {
        if(clean_shutdown) {
            event_loop_thread_.join();
        } else {
            // Thread still alive after our deadline. Detach so the process
            // can exit; skip uv_loop_close to avoid racing the live thread.
            event_loop_thread_.detach();
            loop_initialized_ = false;
        }
    }

    if(loop_initialized_) {
        // Safe to touch loop from main thread now: event_loop_thread_ has
        // been joined (or never started). Run remaining close callbacks.
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

void RabbitAdapter::setup_channels() {
    publish_channel_ = std::make_unique<AMQP::Channel>(connection_.get());
    result_channel_  = std::make_unique<AMQP::Channel>(connection_.get());
    task_channel_    = std::make_unique<AMQP::Channel>(connection_.get());
    status_channel_  = std::make_unique<AMQP::Channel>(connection_.get());

    // Set up error handlers with channel names for diagnostics
    publish_channel_->onError(
        [](const char* msg) {
            std::cerr << "[RabbitMQ] Publish channel error: " << msg << "\n";
        }
    );
    result_channel_->onError(
        [](const char* msg) {
            std::cerr << "[RabbitMQ] Result channel error: " << msg << "\n";
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

    // Enable publisher confirms on result_channel_ ONLY. The previous bug:
    // confirmSelect was on publish_channel_ which also carried progress events
    // and RPC replies. Each non-result publish bumped the broker's delivery_tag
    // counter, but next_publish_seq_ tracked only result publishes - the two
    // counters desynced and every confirm-ack arrived with a delivery_tag that
    // didn't match any pending_confirms_ key. The fix: dedicate this channel
    // to confirm-tracked publishes only. next_publish_seq_ (local) and broker's
    // delivery_tag now both start at 1 and increment together.
    // NB: chain ack/nack BEFORE success/error - `.onError()` returns the base
    // Deferred& (no .onAck/.onNack overloads), so the confirm-specific callbacks
    // must be installed first.
    result_channel_->confirmSelect()
                   .onAck([this](uint64_t delivery_tag, bool multiple) {
                       on_publish_ack(delivery_tag, multiple);
                   })
                   .onNack([this](uint64_t delivery_tag, bool multiple, bool /*requeue*/) {
                       on_publish_nack(delivery_tag, multiple);
                   })
                   .onSuccess([]() {
                       std::cout << "[RabbitMQ] Publisher confirms enabled on result channel\n";
                   })
                   .onError([](const char* msg) {
                       std::cerr << "[RabbitMQ] confirmSelect failed: " << msg
                           << " - results will not be confirmed; broker will redeliver on consumer timeout\n";
                   });

    // Periodic timer for confirm-timeout sweeping (event loop thread).
    if(!confirm_timer_initialized_) {
        uv_timer_init(&loop_, &confirm_timeout_timer_);
        confirm_timeout_timer_.data = this;
        uv_timer_start(&confirm_timeout_timer_, [](uv_timer_t* t) {
            static_cast<RabbitAdapter*>(t->data)->scan_confirm_timeouts();
        }, 5000, 5000); // first tick after 5s, then every 5s
        confirm_timer_initialized_ = true;
    }

    declare_topology();
    start_consumers();
}

void RabbitAdapter::declare_topology() {
    std::cout << "[RabbitMQ] Declaring topology...\n";
    auto& ch = *publish_channel_;

    // Exchange: test.direct (direct) - tasks & results & progress
    ch.declareExchange("test.direct", AMQP::direct, AMQP::durable);

    // Exchange: node.fanout (fanout) - broadcast control (statusRequest)
    ch.declareExchange("node.fanout", AMQP::fanout, AMQP::durable);

    // Exchange: node.control.direct (direct) - targeted control per nodeId
    ch.declareExchange("node.control.direct", AMQP::direct, AMQP::durable);

    // Queue: test.tasks (single queue for all task types)
    ch.declareQueue("test.tasks", AMQP::durable);
    ch.bindQueue("test.direct", "test.tasks", "correctness");
    ch.bindQueue("test.direct", "test.tasks", "performance");
    ch.bindQueue("test.direct", "test.tasks", "all");

    // Queue: test.results - declared by parallel-server consumer side. Runner
    // only publishes here; declaration kept for backward compat / standalone tests.
    ch.declareQueue("test.results", AMQP::durable);
    ch.bindQueue("test.direct", "test.results", "results");
    // NOTE: test.progress queue is declared by parallel-server, not the runner.
    // Runner publishes with routing key "progress" - broker drops if no binding.
}

void RabbitAdapter::start_consumers() {
    std::cout << "[RabbitMQ] Starting consumers...\n";

    // Prefetch matches the JobQueue's correctness worker pool size, so AMQP
    // keeps the runner fully utilised while still letting the broker redeliver
    // unacked tasks if the runner crashes.
    int prefetch = runner_.correctness_workers();
    if(prefetch < 1) prefetch = 1;
    task_channel_->setQos(static_cast<uint16_t>(prefetch));
    std::cout << "[RabbitMQ] task.tasks prefetch=" << prefetch
        << " (manual-ack, publisher-confirms on results)\n";

    // Task consumer (concurrency managed by JobQueue, manual ack after publish-confirm).
    task_channel_->consume("test.tasks")
                 .onReceived(
                     [this](const AMQP::Message& msg, uint64_t tag, bool) {
                         on_task_received(msg, tag, task_channel_.get());
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
                                                  on_control_message(msg, tag, redelivered);
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

void RabbitAdapter::on_task_received(
    const AMQP::Message& msg,
    uint64_t tag,
    AMQP::Channel* channel
) {
    if(stop_) {
        // Don't even parse - leave the message unacked, broker will redeliver
        // to a healthier consumer (or to us after restart).
        channel->reject(tag, true);
        return;
    }

    std::string body(msg.body(), msg.bodySize());
    nlohmann::json task;
    try {
        task = nlohmann::json::parse(body);
    } catch(const std::exception& e) {
        // Malformed input - bad data, not a transport issue. Ack-and-drop.
        std::cerr << "[RabbitMQ] Invalid task JSON: " << e.what() << "\n";
        channel->ack(tag);
        return;
    }

    // Shared validation (sources, testId, jobId, threads, memoryLimitMb, wallTimeSec, cpuTimeSec, maxProcesses)
    auto [valid, validation_error] = adapter_utils::validate_run_request(task);
    if(!valid) {
        // Bad input - ack-and-drop, don't requeue.
        std::cerr << "[RabbitMQ] Task rejected: " << validation_error << "\n";
        if(msg.hasReplyTo()) {
            publish_reply(
                msg.replyTo(),
                {{"error", validation_error}, {"status", to_string(response_status::rejected)}},
                msg.hasCorrelationID() ? msg.correlationID() : ""
            );
        }
        channel->ack(tag);
        return;
    }

    // Embed AMQP reply metadata for the executor
    if(msg.hasReplyTo()) task["_reply_to"] = msg.replyTo();
    if(msg.hasCorrelationID()) task["_correlation_id"] = msg.correlationID();

    std::string job_id = task.value("jobId", "");
    if(job_id.empty()) {
        job_id = generate_job_id();
        task["jobId"] = job_id;
    }
    // jobId format already validated by shared validate_run_request() above

    // Track delivery_tag so cancelJob can ack-and-discard a queued job
    // (avoids 30-min consumer_timeout redelivery for cancelled tasks).
    job_to_tag_[job_id] = tag;

    // Inject node_id so pipeline tags progress events with this runner.
    task["_node_id"] = config_.node_id;

    // Emit "received" progress event (best-effort, before the job even queues).
    {
        nlohmann::json event = {
            {"jobId",     job_id},
            {"nodeId",    config_.node_id},
            {"phase",     "received"},
            {"timestamp", now_iso8601()}
        };
        publish("test.direct", "progress", event);
    }

    // Build progress callback bound to this adapter's event loop.
    auto alive_for_progress = alive_;
    auto on_progress = [this, alive_for_progress](const nlohmann::json& event) {
        if(!alive_for_progress->load()) return;
        post_to_event_loop([this, event]() {
            publish("test.direct", "progress", event);
        });
    };

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
    std::cout << "[RabbitMQ] task: " << job_id << "\n";

    // Build completion callback. The lambda runs on a JobQueue worker thread;
    // it dispatches the actual publish + ack to the event loop. Ack is deferred
    // until the broker confirms the publish (see publish_result_with_confirm).
    auto alive = alive_;
    auto node_id = config_.node_id;
    auto start_time = std::chrono::steady_clock::now();

    auto on_complete = [this, alive, job_id, node_id, start_time, tag](
        const nlohmann::json& result
    ) {
        if(!alive->load()) return;
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time
        ).count();

        auto envelope_msg = adapter_utils::build_completion_result(result, job_id, node_id, duration);

        post_to_event_loop(
            [this, job_id, tag, envelope_msg = std::move(envelope_msg)]() {
                // If the job was cancelled, job_to_tag_ no longer holds an entry
                // and the consume tag has already been acked by cancel_job.
                // In that case the JobQueue worker should not have invoked us at
                // all (callback erased on cancel), but be defensive:
                if(job_to_tag_.find(job_id) == job_to_tag_.end()) return;
                publish_result_with_confirm(job_id, tag, envelope_msg);
            }
        );
    };

    // Submit to shared job queue
    runner_.submit(std::move(task), std::move(on_complete), std::move(on_progress));

    // Send immediate RPC acceptance AFTER submit (job now exists in queue)
    if(msg.hasReplyTo()) {
        auto info = runner_.get_job_info(job_id);
        publish_reply(
            msg.replyTo(),
            {
                {"jobId", job_id},
                {"status", to_string(job_status::queued)},
                {"nodeId", config_.node_id},
                {"position", info.queue_position},
                {"solution", solution_name},
                {"memoryLimitMb", memory_limit_mb},
                {"timestamp", now_iso8601()}
            },
            msg.hasCorrelationID() ? msg.correlationID() : ""
        );
    }
}

bool RabbitAdapter::require_management(const ReplyFn& reply, control_type ct) {
    if(management_) return true;
    reply(response_type(ct), {{"status", "error"}, {"error", "Management API not available"}});
    return false;
}

void RabbitAdapter::setup_control_handlers() {
    control_handlers_[control_type::queue_status] = [this](const nlohmann::json&, const ReplyFn& reply) {
        reply(response_type(control_type::queue_status), runner_.get_queue_status());
        std::cout << "[RabbitMQ] Responded to queueStatus\n";
    };

    control_handlers_[control_type::status_request] = [this](const nlohmann::json&, const ReplyFn& reply) {
        auto status = adapter_utils::build_node_event(node_event_type::info, config_.node_id, runner_, management_);
        reply("statusResponse", status);
        std::cout << "[RabbitMQ] Responded to statusRequest\n";
    };

    control_handlers_[control_type::list_adapters] = [this](const nlohmann::json&, const ReplyFn& reply) {
        if(!require_management(reply, control_type::list_adapters)) return;
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
        if(!require_management(reply, control_type::load_adapter)) return;
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
        if(!require_management(reply, control_type::list_available_adapters)) return;
        auto available = adapter_utils::filter_available_adapters(management_);
        reply(response_type(control_type::list_available_adapters), {{"adapters", available}});
        std::cout << "[RabbitMQ] Responded to listAvailableAdapters\n";
    };

    control_handlers_[control_type::unload_adapter] = [this](const nlohmann::json& parsed, const ReplyFn& reply) {
        if(!require_management(reply, control_type::unload_adapter)) return;
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
        // Canonical shape (same as HTTP PUT /api/config):
        //   { "type":"updateConfig", "config": { <ConfigUpdateRequest> } }
        auto cfg = parsed.value("config", nlohmann::json::object());
        if(!cfg.is_object() || cfg.empty()) {
            reply(
                response_type(control_type::update_config),
                {{"status", to_string(response_status::error)}, {"error", "Missing or empty 'config' object"}}
            );
            return;
        }
        auto [ok, err] = adapter_utils::apply_config(runner_, cfg);
        if(!ok) {
            reply(
                response_type(control_type::update_config),
                {{"status", to_string(response_status::error)}, {"error", err}}
            );
            return;
        }
        std::cout << "[RabbitMQ] updateConfig: " << cfg.dump() << "\n";
        reply(response_type(control_type::update_config), runner_.get_queue_status());
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
        if(ok) {
            // Cancel succeeded - JobQueue dropped the queued job and erased
            // its completion callback, so on_complete will never fire and
            // never publish-and-ack. Ack the consume tag here so the broker
            // doesn't redeliver a cancelled task after consumer_timeout.
            auto it = job_to_tag_.find(job_id);
            if(it != job_to_tag_.end()) {
                if(task_channel_) task_channel_->ack(it->second);
                job_to_tag_.erase(it);
            }
        }
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
            auto info = runner_.get_job_info(job_id);
            reply(response_type(control_type::get_job_info), adapter_utils::build_job_info_json(info));
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
        if(!require_management(reply, control_type::list_resource_providers)) return;
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
            if(!require_management(reply, control_type::list_available_resource_providers)) return;
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
            if(!require_management(reply, control_type::load_resource_provider)) return;
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
            if(!require_management(reply, control_type::unload_resource_provider)) return;
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

void RabbitAdapter::on_control_message(const AMQP::Message& msg, uint64_t, bool) {
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
            extra["timestamp"] = now_iso8601();
            publish_reply(msg.replyTo(), extra, corr_id);
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

void RabbitAdapter::publish_reply(
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
// Durability: result publish with confirms + ack
// ============================================================================

void RabbitAdapter::publish_result_with_confirm(
    const std::string& job_id,
    uint64_t consume_tag,
    const nlohmann::json& message
) {
    if(!result_channel_ || !task_channel_) return;

    // Reserve the publish slot BEFORE the publish call, so a same-tick confirm
    // callback can find the entry. On a confirm-select-enabled channel that is
    // used EXCLUSIVELY for confirm-tracked publishes, broker's delivery_tag is
    // monotonic from 1 and matches our next_publish_seq_ exactly.
    uint64_t my_seq = next_publish_seq_++;
    pending_confirms_[my_seq] = PendingConfirm{
        consume_tag, job_id, std::chrono::steady_clock::now()
    };

    std::string body = message.dump();
    AMQP::Envelope envelope(body.data(), body.size());
    envelope.setContentType("application/json");
    envelope.setDeliveryMode(2);

    bool ok = result_channel_->publish("test.direct", "results", envelope);
    if(!ok) {
        // Local publish failure (channel/connection broken). Drop tracking;
        // the consume tag stays unacked and broker will redeliver after
        // consumer_timeout. Don't try to nack - channel may already be dead.
        pending_confirms_.erase(my_seq);
        std::cerr << "[RabbitMQ] result publish failed locally for " << job_id
            << " (channel error?), broker will redeliver task\n";
    }
}

void RabbitAdapter::resolve_pending(uint64_t publish_seq, bool ack_consume_tag) {
    auto it = pending_confirms_.find(publish_seq);
    if(it == pending_confirms_.end()) return;

    if(task_channel_) {
        if(ack_consume_tag) {
            task_channel_->ack(it->second.consume_tag);
        } else {
            task_channel_->reject(it->second.consume_tag, true); // requeue
        }
    }
    job_to_tag_.erase(it->second.job_id);
    pending_confirms_.erase(it);
}

void RabbitAdapter::on_publish_ack(uint64_t delivery_tag, bool multiple) {
    if(multiple) {
        // Resolve everything up to and including delivery_tag.
        // We pre-advance `it` because resolve_pending invalidates it via erase.
        // CRITICAL: check `it != end()` BEFORE dereferencing - after the last
        // erase `it` may already be `end()` and `it->first` would be UB.
        auto it = pending_confirms_.begin();
        while(it != pending_confirms_.end() && it->first <= delivery_tag) {
            uint64_t seq = it->first;
            ++it;
            resolve_pending(seq, true);
        }
    } else {
        resolve_pending(delivery_tag, true);
    }
}

void RabbitAdapter::on_publish_nack(uint64_t delivery_tag, bool multiple) {
    std::cerr << "[RabbitMQ] publisher NACK for delivery_tag=" << delivery_tag
        << " multiple=" << multiple << " - requeueing input tasks\n";
    if(multiple) {
        // See on_publish_ack for why the loop guard checks end() first.
        auto it = pending_confirms_.begin();
        while(it != pending_confirms_.end() && it->first <= delivery_tag) {
            uint64_t seq = it->first;
            ++it;
            resolve_pending(seq, false);
        }
    } else {
        resolve_pending(delivery_tag, false);
    }
}

void RabbitAdapter::scan_confirm_timeouts() {
    auto now = std::chrono::steady_clock::now();
    auto threshold = std::chrono::seconds(kConfirmTimeoutSec);
    int requeued = 0;
    for(auto it = pending_confirms_.begin(); it != pending_confirms_.end();) {
        if(now - it->second.published_at < threshold) { ++it; continue; }
        uint64_t seq = it->first;
        std::string job_id = it->second.job_id;
        ++it;
        std::cerr << "[RabbitMQ] confirm timeout for job " << job_id
            << " (seq=" << seq << ") - requeueing\n";
        resolve_pending(seq, false);
        ++requeued;
    }
    if(requeued > 0) {
        std::cerr << "[RabbitMQ] confirm-timeout sweep requeued " << requeued
            << " task(s)\n";
    }
}

// ============================================================================
// DLL Factory (generated by macro)
// ============================================================================

REGISTER_ADAPTER(RabbitAdapter, "rabbit")