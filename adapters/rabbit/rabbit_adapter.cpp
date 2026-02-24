#include <adapter_utils.h>
#include <chrono>
#include <filesystem>
#include <future>
#include <iostream>
#include <rabbit_adapter.h>
#include <register_adapter.h>
#include <time_utils.h>
#include <uv_amqp_handler.h>

// ============================================================================
// Construction / Destruction
// ============================================================================

RabbitAdapter::RabbitAdapter(
	TestRunnerService& runner,
	const ManagementAPI* management,
	const nlohmann::json& config
)
	: runner_(runner), management_(management),
	  alive_(std::make_shared<std::atomic<bool>>(true)) {
	setupControlHandlers();

	if(!config.contains("host") || config["host"].get<std::string>().empty()) {
		throw std::runtime_error("[RabbitMQ] Missing required 'host' in config");
	}

	config_.host = config.value("host", "localhost");
	config_.port = config.value("port", 5672);
	config_.user = config.value("user", "guest");
	config_.password = config.value("password", "guest");
	config_.vhost = config.value("vhost", "/");
	config_.node_id = config.value("nodeId", "");
	config_.connection_timeout_sec = config.value("connectionTimeoutSec", 10);

	if(config_.port < 1 || config_.port > 65535) {
		throw std::runtime_error("[RabbitMQ] Invalid port: " + std::to_string(config_.port));
	}

	if(config_.node_id.empty()) {
		config_.node_id = generateNodeId();
	}
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

	// Broadcast online event (non-blocking, runs on event loop)
	postToEventLoop([this]() { publishNodeEvent("online"); });
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
					publishNodeEvent("offline");
				} catch(const std::exception& e) {
					std::cerr << "[RabbitMQ] Failed to publish offline event: " << e.what() << "\n";
				} catch(...) {}

				// Close AMQP channels and connection
				correctness_channel_.reset();
				performance_channel_.reset();
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
	correctness_channel_ = std::make_unique<AMQP::Channel>(connection_.get());
	performance_channel_ = std::make_unique<AMQP::Channel>(connection_.get());
	status_channel_ = std::make_unique<AMQP::Channel>(connection_.get());

	// Set up error handlers with channel names for diagnostics
	publish_channel_->onError([](const char* msg) {
		std::cerr << "[RabbitMQ] Publish channel error: " << msg << "\n";
	});
	correctness_channel_->onError([](const char* msg) {
		std::cerr << "[RabbitMQ] Correctness channel error: " << msg << "\n";
	});
	performance_channel_->onError([](const char* msg) {
		std::cerr << "[RabbitMQ] Performance channel error: " << msg << "\n";
	});
	status_channel_->onError([](const char* msg) {
		std::cerr << "[RabbitMQ] Status channel error: " << msg << "\n";
	});

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

	// Queue: test.tasks.correctness
	ch.declareQueue("test.tasks.correctness", AMQP::durable);
	ch.bindQueue("test.direct", "test.tasks.correctness", "correctness");

	// Queue: test.tasks.performance
	ch.declareQueue("test.tasks.performance", AMQP::durable);
	ch.bindQueue("test.direct", "test.tasks.performance", "performance");

	// Queue: test.results
	ch.declareQueue("test.results", AMQP::durable);
	ch.bindQueue("test.direct", "test.results", "result");

	// Queue: node.events
	ch.declareQueue("node.events", AMQP::durable);
	ch.bindQueue("node.fanout", "node.events", "");
}

void RabbitAdapter::startConsumers() {
	std::cout << "[RabbitMQ] Starting consumers...\n";
	// Correctness consumer (concurrency managed by JobQueue, not AMQP QoS)
	correctness_channel_->consume("test.tasks.correctness")
	                    .onReceived(
		                    [this](const AMQP::Message& msg, uint64_t tag, bool) {
			                    onTaskReceived(msg, tag, correctness_channel_.get());
		                    }
	                    )
	                    .onError(
		                    [](const char* msg) {
			                    std::cerr << "[RabbitMQ] Correctness consume error: " << msg << "\n";
		                    }
	                    );

	// Performance consumer: QoS = 1 (exclusive execution)
	performance_channel_->setQos(1);
	performance_channel_->consume("test.tasks.performance")
	                    .onReceived(
		                    [this](const AMQP::Message& msg, uint64_t tag, bool) {
			                    onTaskReceived(msg, tag, performance_channel_.get());
		                    }
	                    )
	                    .onError(
		                    [](const char* msg) {
			                    std::cerr << "[RabbitMQ] Performance consume error: " << msg << "\n";
		                    }
	                    );

	// Control listener: exclusive auto-delete queue bound to node.fanout
	status_channel_->declareQueue(AMQP::exclusive + AMQP::autodelete)
	               .onSuccess(
		               [this](const std::string& name, uint32_t, uint32_t) {
			               status_channel_->bindQueue("node.fanout", name, "");
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

	// Embed AMQP reply metadata for the executor
	if(msg.hasReplyTo()) task["_reply_to"] = msg.replyTo();
	if(msg.hasCorrelationID()) task["_correlation_id"] = msg.correlationID();

	std::string job_id = task.value("jobId", "");
	if(job_id.empty()) {
		job_id = generateJobId();
		task["jobId"] = job_id;
	}

	// Ack immediately (task accepted into queue)
	channel->ack(tag);

	std::string mode = task.value("mode", "correctness");
	std::string solution_dir = task.value("solutionDir", "");
	std::cout << "[RabbitMQ] " << mode << " task: " << job_id << "\n";

	// Build completion callback for AMQP result publishing
	std::string reply_to = task.value("_reply_to", "");
	std::string correlation_id = task.value("_correlation_id", "");
	auto alive = alive_;
	auto node_id = config_.node_id;
	auto start_time = std::chrono::steady_clock::now();

	auto on_complete = [this, alive, reply_to, correlation_id, job_id, node_id, start_time](
		const nlohmann::json& result
	) {
		if(!alive->load()) return;
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - start_time
		).count();

		auto msg = adapter_utils::buildCompletionResult(result, job_id, node_id, duration);

		postToEventLoop(
			[this, msg = std::move(msg), reply_to, correlation_id]() {
				publish("test.direct", "result", msg);
				if(!reply_to.empty()) {
					publishReply(reply_to, msg, correlation_id);
				}
			}
		);
	};

	// Submit to shared job queue (lane determined by "mode" field)
	runner_.submit(std::move(task), std::move(on_complete));

	// Send immediate RPC acceptance AFTER submit (job now exists in queue)
	if(msg.hasReplyTo()) {
		auto info = runner_.getJobInfo(job_id);
		std::string solution_name = solution_dir.empty()
			? "" : std::filesystem::path(solution_dir).filename().string();
		publishReply(
			msg.replyTo(),
			{
				{"jobId", job_id},
				{"status", "queued"},
				{"nodeId", config_.node_id},
				{"position", info.queue_position},
				{"mode", mode},
				{"solution", solution_name},
				{"timestamp", nowISO8601()}
			},
			msg.hasCorrelationID() ? msg.correlationID() : ""
		);
	}
}

void RabbitAdapter::setupControlHandlers() {
	control_handlers_["queueStatus"] = [this](const nlohmann::json&, const ReplyFn& reply) {
		reply("queueStatusResponse", runner_.getQueueStatus());
		std::cout << "[RabbitMQ] Responded to queueStatus\n";
	};

	control_handlers_["statusRequest"] = [this](const nlohmann::json&, const ReplyFn& reply) {
		auto status = adapter_utils::buildNodeStatus(runner_, config_.node_id);
		reply("statusResponse", status);
		std::cout << "[RabbitMQ] Responded to statusRequest\n";
	};

	control_handlers_["listAdapters"] = [this](const nlohmann::json&, const ReplyFn& reply) {
		if(!management_) {
			std::cerr << "[RabbitMQ] Management API not available for listAdapters\n";
			return;
		}
		const char* json_str = management_->list_adapters(management_->context);
		if(!json_str) {
			reply("listAdaptersResponse", {{"adapters", nlohmann::json::array()}, {"error", "Failed to list adapters"}});
			return;
		}
		auto adapters = nlohmann::json::parse(json_str);
		management_->free_string(management_->context, json_str);
		reply("listAdaptersResponse", {{"adapters", adapters}});
		std::cout << "[RabbitMQ] Responded to listAdapters\n";
	};

	control_handlers_["loadAdapter"] = [this](const nlohmann::json& parsed, const ReplyFn& reply) {
		if(!management_) {
			std::cerr << "[RabbitMQ] Management API not available for loadAdapter\n";
			return;
		}
		std::string adapter_name = parsed.value("adapter", "");
		nlohmann::json config = parsed.value("config", nlohmann::json::object());
		bool ok = management_->load_adapter(management_->context, adapter_name.c_str(), config);
		reply("loadAdapterResponse", {
			{"adapter", adapter_name},
			{"status", ok ? "started" : "failed"}
		});
		std::cout << "[RabbitMQ] loadAdapter '" << adapter_name << "': "
			<< (ok ? "ok" : "failed") << "\n";
	};

	control_handlers_["listAvailableAdapters"] = [this](const nlohmann::json&, const ReplyFn& reply) {
		if(!management_) {
			std::cerr << "[RabbitMQ] Management API not available for listAvailableAdapters\n";
			return;
		}
		auto available = adapter_utils::filterAvailableAdapters(management_);
		reply("listAvailableAdaptersResponse", {{"adapters", available}});
		std::cout << "[RabbitMQ] Responded to listAvailableAdapters\n";
	};

	control_handlers_["unloadAdapter"] = [this](const nlohmann::json& parsed, const ReplyFn& reply) {
		if(!management_) {
			std::cerr << "[RabbitMQ] Management API not available for unloadAdapter\n";
			return;
		}
		std::string adapter_name = parsed.value("adapter", "");
		bool ok = management_->unload_adapter(management_->context, adapter_name.c_str());
		reply("unloadAdapterResponse", {
			{"adapter", adapter_name},
			{"status", ok ? "stopped" : "failed"}
		});
		std::cout << "[RabbitMQ] unloadAdapter '" << adapter_name << "': "
			<< (ok ? "ok" : "failed") << "\n";
	};

	control_handlers_["setMaxCorrectnessWorkers"] = [this](const nlohmann::json& parsed, const ReplyFn& reply) {
		int n = parsed.value("maxCorrectnessWorkers", 0);
		if(n < 1) {
			reply("setMaxCorrectnessWorkersResponse", {{"status", "error"}, {"error", "maxCorrectnessWorkers must be >= 1"}});
			return;
		}
		runner_.setMaxCorrectnessWorkers(n);
		auto status = runner_.getQueueStatus();
		status["maxCorrectnessWorkers"] = n;
		reply("setMaxCorrectnessWorkersResponse", status);
		std::cout << "[RabbitMQ] setMaxCorrectnessWorkers -> " << n << "\n";
	};

	control_handlers_["setJobRetentionSeconds"] = [this](const nlohmann::json& parsed, const ReplyFn& reply) {
		int sec = parsed.value("jobRetentionSeconds", 0);
		if(sec < 1) {
			reply("setJobRetentionSecondsResponse", {{"status", "error"}, {"error", "jobRetentionSeconds must be >= 1"}});
			return;
		}
		runner_.setJobRetentionSeconds(sec);
		reply("setJobRetentionSecondsResponse", {{"status", "ok"}, {"jobRetentionSeconds", sec}});
		std::cout << "[RabbitMQ] setJobRetentionSeconds -> " << sec << "\n";
	};

	control_handlers_["cancelJob"] = [this](const nlohmann::json& parsed, const ReplyFn& reply) {
		std::string job_id = parsed.value("jobId", "");
		if(job_id.empty()) {
			reply("cancelJobResponse", {{"status", "error"}, {"error", "Missing jobId"}});
			return;
		}
		bool ok = runner_.cancel(job_id);
		reply("cancelJobResponse", {
			{"jobId", job_id},
			{"status", ok ? "cancelled" : "failed"},
			{"error", ok ? "" : "Cannot cancel job (not queued or not found)"}
		});
		std::cout << "[RabbitMQ] cancelJob '" << job_id << "': "
			<< (ok ? "cancelled" : "failed") << "\n";
	};

	control_handlers_["getJobInfo"] = [this](const nlohmann::json& parsed, const ReplyFn& reply) {
		std::string job_id = parsed.value("jobId", "");
		if(job_id.empty()) {
			reply("getJobInfoResponse", {{"status", "error"}, {"error", "Missing jobId"}});
			return;
		}
		try {
			auto info = runner_.getJobInfo(job_id);
			reply("getJobInfoResponse", adapter_utils::buildJobInfoJson(info));
		} catch(const std::exception& e) {
			reply("getJobInfoResponse", {
				{"jobId", job_id}, {"status", "error"}, {"error", e.what()}
			});
		}
		std::cout << "[RabbitMQ] getJobInfo '" << job_id << "'\n";
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

		auto it = control_handlers_.find(type);
		if(it != control_handlers_.end()) {
			it->second(parsed, reply_control);
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

void RabbitAdapter::publishNodeEvent(const std::string& type) {
	nlohmann::json event;
	event["type"] = type;
	event["nodeId"] = config_.node_id;
	event["timestamp"] = nowISO8601();

	if(type != "offline") {
		auto status = adapter_utils::buildNodeStatus(runner_, config_.node_id);
		event["capabilities"] = status["capabilities"];
		event["currentLoad"] = status["currentLoad"];
	}

	publish("node.fanout", "", event);
}

// ============================================================================
// DLL Factory (generated by macro)
// ============================================================================

REGISTER_ADAPTER(RabbitAdapter, "rabbit")
