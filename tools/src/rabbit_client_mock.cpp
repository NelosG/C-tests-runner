/**
 * @file rabbit_client_mock.cpp
 * @brief Mock orchestrator — monitors node events and manages test jobs via RabbitMQ.
 *
 * Uses AMQP-CPP + libuv (async, single-threaded event loop).
 *
 * Usage:
 *   rabbit_client_mock [host] [port] [user] [password] [vhost]
 *
 * Defaults: localhost 5672 guest guest /
 *
 * Commands (type in console):
 *   s                     — broadcast status_request to all nodes
 *   a                     — broadcast list_adapters to all nodes
 *   av                    — broadcast list_available_adapters to all nodes
 *   load <name> [json]    — broadcast load_adapter to all nodes
 *   unload <name>         — broadcast unload_adapter to all nodes
 *   rp                    — broadcast list_resource_providers to all nodes
 *   rpav                  — broadcast list_available_resource_providers to all nodes
 *   rp-load <name> [json] — broadcast load_resource_provider to all nodes
 *   rp-unload <name>      — broadcast unload_resource_provider to all nodes
 *   run <args>            — submit test jobs via RabbitMQ (see below)
 *   qs                    — broadcast queue_status request to all nodes
 *   cancel <job_id>       — cancel a queued job on all nodes
 *   job <job_id>          — query job status/result on all nodes
 *   config <key> <val>    — set config on all nodes, e.g. config maxCorrectnessWorkers 4
 *   q                     — quit
 *
 * Run command:
 *   run --test-id <id> --test-dir <dir> --solution <dir> [--solution <dir2>]
 *       [--mode correctness|performance|all] [--threads N] [--memory MB]
 *
 * Logs node online/offline events from node.events queue.
 */

#include <amqpcpp.h>
#include <iostream>
#include <memory>
#include <new>
#include <random>
#include <sstream>
#include <string>
#include <time_utils.h>
#include <uv_amqp_handler.h>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "mock_utils.h"

namespace fs = std::filesystem;

#ifdef _WIN32
#include <windows.h>
#endif

static std::string generateId() {
    thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<uint32_t> dist;
    std::ostringstream ss;
    ss << "rpc-" << std::hex << dist(gen);
    return ss.str();
}

/// Encapsulates all mutable state for the mock orchestrator.
struct MockState {
    uv_loop_t* loop = nullptr;
    UvAmqpHandler* handler = nullptr;
    AMQP::Channel* channel = nullptr;
    AMQP::Channel* rpc_channel = nullptr;
    int status_count = 0;
};

static MockState g_state;

// ============================================================================
// Run command — submit test jobs via RabbitMQ
// ============================================================================

static void submitRunRabbit(const std::string& args_str) {
    RunArgs args;
    if(!parseRunArgs(args_str, args)) return;

    if(!g_state.channel || !g_state.rpc_channel) {
        std::cerr << "[Mock] No AMQP channel available\n";
        return;
    }

    int total_jobs = static_cast<int>(args.solutions.size());

    std::cout << "\n[Mock] ========== Submitting test run via RabbitMQ ==========\n"
        << "  test_id:    " << args.test_id << "\n"
        << "  test_dir:   " << args.test_dir << "\n"
        << "  mode:       " << args.mode << " (routing_key: " << args.mode << ")\n"
        << "  threads:    " << args.threads << "\n"
        << "  memory:     " << (args.memory_limit_mb >= 0 ? std::to_string(args.memory_limit_mb) + " MB" : "default") << "\n"
        << "  solutions:  " << args.solutions.size() << "\n";
    for(size_t i = 0; i < args.solutions.size(); ++i) {
        std::cout << "    [" << (i + 1) << "] " << fs::path(args.solutions[i]).filename().string()
            << " (" << args.solutions[i] << ")\n";
    }

    // Routing key = mode (single queue, engine routes internally by mode field)
    std::string routing_key = args.mode;

    // Declare exclusive reply queue to collect results
    g_state.rpc_channel->declareQueue(AMQP::exclusive + AMQP::autodelete)
           .onSuccess(
               [args, total_jobs, routing_key](const std::string& reply_queue, uint32_t, uint32_t) {
                   auto run_results = std::make_shared<int>(0);

                   g_state.rpc_channel->consume(reply_queue, AMQP::noack)
                          .onReceived(
                              [total_jobs, run_results](const AMQP::Message& msg, uint64_t, bool) {
                                  std::string body(msg.body(), msg.bodySize());
                                  try {
                                      auto resp = nlohmann::json::parse(body);
                                      std::string status = resp.value("status", "unknown");
                                      std::string job_id = resp.value("jobId", "?");
                                      std::string solution = resp.value("solution", "?");
                                      std::string mode = resp.value("mode", "?");

                                      // Skip intermediate acceptance messages, count only final results
                                      if(status == "queued" || status == "accepted") {
                                          std::cout << "[Mock] Job " << job_id
                                              << " accepted (solution=" << solution
                                              << ", mode=" << mode << ")\n";
                                          return;
                                      }

                                      ++(*run_results);
                                      std::cout << "\n=== Result " << *run_results << "/" << total_jobs
                                          << ": " << solution << " [" << mode << "] "
                                          << status << " (job=" << job_id << ") ===\n";
                                      if(resp.contains("correctness"))
                                          std::cout << "  Correctness: " << summarizeTests(resp["correctness"]) << "\n";
                                      if(resp.contains("performance"))
                                          std::cout << "  Performance: " << summarizeTests(resp["performance"]) << "\n";
                                      if(resp.value("performanceSkipped", false))
                                          std::cout << "  Performance: SKIPPED ("
                                              << resp.value("performanceSkipReason", "") << ")\n";
                                      if(resp.contains("error"))
                                          std::cerr << "  Error: " << resp["error"].get<std::string>() << "\n";
                                      std::cout << resp.dump(2) << "\n";

                                      if(*run_results >= total_jobs) {
                                          std::cout << "[Mock] All " << total_jobs << " result(s) received\n";
                                      }
                                  } catch(...) {
                                      ++(*run_results);
                                      std::cout << "\n=== Result (raw) ===\n" << body << "\n";
                                  }
                              }
                          );

                   // Publish one task per solution
                   for(const auto& sol_dir : args.solutions) {
                       nlohmann::json task = {
                           {"testId", args.test_id},
                           {"testSourceType", "local"},
                           {"testSource", {{"path", args.test_dir}}},
                           {"solutionSourceType", "local"},
                           {"solutionSource", {{"path", sol_dir}}},
                           {"mode", args.mode},
                           {"threads", args.threads}
                       };
                       if(args.memory_limit_mb >= 0) {
                           task["memoryLimitMb"] = args.memory_limit_mb;
                       }

                       std::string corr_id = generateId();
                       std::string body = task.dump();
                       AMQP::Envelope envelope(body.data(), body.size());
                       envelope.setContentType("application/json");
                       envelope.setDeliveryMode(1);
                       envelope.setReplyTo(reply_queue);
                       envelope.setCorrelationID(corr_id);

                       std::string sol_name = fs::path(sol_dir).filename().string();
                       std::cout << "[Mock] Publishing: " << sol_name
                           << " | mode=" << args.mode
                           << " | routing_key=" << routing_key
                           << " | corr_id=" << corr_id << "\n";

                       g_state.channel->publish("test.direct", routing_key, envelope);
                   }

                   std::cout << "[Mock] " << total_jobs << " job(s) submitted, waiting for results...\n";

                   // Timeout timer (5 minutes)
                   auto* timer = new uv_timer_t;
                   uv_timer_init(g_state.loop, timer);
                   uv_timer_start(
                       timer,
                       [](uv_timer_t* t) {
                           std::cout << "[Mock] Run timeout (5 min)\n";
                           uv_timer_stop(t);
                           uv_close(
                               reinterpret_cast<uv_handle_t*>(t),
                               [](uv_handle_t* h) {
                                   delete reinterpret_cast<uv_timer_t*>(h);
                               }
                           );
                       },
                       300000,
                       0
                   );
               }
           );
}

// ============================================================================
// Control RPC
// ============================================================================

static void sendControlRpc(const nlohmann::json& request, int timeout_ms = 5000) {
    if(!g_state.rpc_channel) {
        std::cerr << "[Mock] No RPC channel\n";
        return;
    }

    std::string corr_id = generateId();
    std::string req_type = request.value("type", "?");

    g_state.rpc_channel->declareQueue(AMQP::exclusive + AMQP::autodelete)
           .onSuccess(
               [corr_id, request, req_type, timeout_ms](const std::string& reply_queue, uint32_t, uint32_t) {
                   g_state.status_count = 0;

                   g_state.rpc_channel->consume(reply_queue, AMQP::noack)
                          .onReceived(
                              [corr_id](const AMQP::Message& msg, uint64_t, bool) {
                                  std::string body(msg.body(), msg.bodySize());
                                  std::string msg_corr = msg.hasCorrelationID() ? msg.correlationID() : "";
                                  if(!msg_corr.empty() && msg_corr != corr_id) return;

                                  try {
                                      auto resp = nlohmann::json::parse(body);
                                      ++g_state.status_count;
                                      std::string node_id_resp = resp.value("nodeId", "");
                                      std::string type_resp = resp.value("type", "");
                                      std::cout << "\n[Mock] Response #" << g_state.status_count;
                                      if(!node_id_resp.empty()) std::cout << " from " << node_id_resp;
                                      if(!type_resp.empty()) std::cout << " (" << type_resp << ")";
                                      std::cout << ":\n" << resp.dump(2) << "\n";
                                  } catch(...) {
                                      ++g_state.status_count;
                                      std::cout << "\n[Mock] Response #" << g_state.status_count
                                          << " (raw):\n" << body << "\n";
                                  }
                              }
                          );

                   std::string body = request.dump();
                   AMQP::Envelope envelope(body.data(), body.size());
                   envelope.setContentType("application/json");
                   envelope.setDeliveryMode(1);
                   envelope.setReplyTo(reply_queue);
                   envelope.setCorrelationID(corr_id);

                   g_state.channel->publish("node.fanout", "", envelope);

                   std::cout << "[Mock] Sent '" << req_type
                       << "', collecting responses for " << (timeout_ms / 1000) << "s...\n";

                   auto* timer = new uv_timer_t;
                   uv_timer_init(g_state.loop, timer);
                   uv_timer_start(
                       timer,
                       [](uv_timer_t* t) {
                           std::cout << "[Mock] Received " << g_state.status_count << " response(s)\n";
                           uv_timer_stop(t);
                           uv_close(
                               reinterpret_cast<uv_handle_t*>(t),
                               [](uv_handle_t* h) {
                                   delete reinterpret_cast<uv_timer_t*>(h);
                               }
                           );
                       },
                       timeout_ms,
                       0
                   );
               }
           );
}

int main(int argc, char** argv) {
    std::string host = argc > 1 ? argv[1] : "localhost";
    int port = 5672;
    if(argc > 2) {
        try { port = std::stoi(argv[2]); } catch(const std::exception&) {
            std::cerr << "[Mock] Invalid port: " << argv[2] << "\n";
            return 1;
        }
    }
    std::string user = argc > 3 ? argv[3] : "guest";
    std::string password = argc > 4 ? argv[4] : "guest";
    std::string vhost = argc > 5 ? argv[5] : "/";

    std::cout << "[Mock] Connecting to " << host << ":" << port << "...\n";

    uv_loop_t loop;
    uv_loop_init(&loop);
    g_state.loop = &loop;

    // Setup AMQP connection via UvAmqpHandler
    UvAmqpHandler handler(&loop);
    g_state.handler = &handler;

    handler.setErrorCallback(
        [](const char* msg) {
            std::cerr << "[Mock] Connection error: " << msg << "\n";
        }
    );

    handler.connect(host, port);

    AMQP::Login login(user, password);
    AMQP::Connection connection(&handler, login, vhost);
    handler.setConnection(&connection);

    AMQP::Channel channel(&connection);
    AMQP::Channel rpc_channel(&connection);
    g_state.channel = &channel;
    g_state.rpc_channel = &rpc_channel;

    handler.setReadyCallback(
        [&]() {
            std::cout << "[Mock] Connected!\n";

            // Declare topology
            g_state.channel->declareExchange("node.fanout", AMQP::fanout, AMQP::durable);
            g_state.channel->declareExchange("node.control.direct", AMQP::direct, AMQP::durable);
            g_state.channel->declareQueue("node.events", AMQP::durable);
            g_state.channel->bindQueue("node.fanout", "node.events", "");

            // Consume from node.events
            g_state.channel->consume("node.events", AMQP::noack)
                   .onReceived(
                       [](const AMQP::Message& msg, uint64_t, bool) {
                           std::string body(msg.body(), msg.bodySize());

                           try {
                               auto parsed = nlohmann::json::parse(body);
                               std::string type = parsed.value("type", "unknown");
                               std::string node_id = parsed.value("nodeId", "unknown");

                               if(type == "online") {
                                   std::cout << "[Mock] ONLINE: node_id=" << node_id << "\n";
                                   if(parsed.contains("capabilities")) {
                                       std::cout << "  capabilities: " << parsed["capabilities"].dump() << "\n";
                                   }
                               } else if(type == "offline") {
                                   std::cout << "[Mock] OFFLINE: node_id=" << node_id << "\n";
                               } else {
                                   std::cout << "[Mock] Event: type=" << type << " node_id=" << node_id << "\n";
                               }
                           } catch(const std::exception& e) {
                               std::cerr << "[Mock] Parse error: " << e.what() << "\n";
                           }
                       }
                   )
                   .onSuccess(
                       [](const std::string& tag) {
                           std::cout <<
                               "[Mock] Ready. Commands: s, qs, a, av, a-load/a-unload <name>, rp, rpav, rp-load/rp-unload <name>, cancel/job <id>, config <key> <val>, run <args>, q\n";
                       }
                   );
        }
    );

    // Setup stdin reading via libuv
    uv_tty_t tty;
    uv_tty_init(&loop, &tty, 0, 1); // fd=0 (stdin), readable=1

    static std::string line_buf;

    uv_read_start(
        reinterpret_cast<uv_stream_t*>(&tty),
        [](uv_handle_t*, size_t suggested, uv_buf_t* buf) {
            buf->base = new(std::nothrow) char[suggested];
            buf->len = buf->base ? static_cast<unsigned int>(suggested) : 0;
        },
        [](uv_stream_t*, ssize_t nread, const uv_buf_t* buf) {
            if(nread > 0) {
                for(ssize_t i = 0; i < nread; ++i) {
                    char c = buf->base[i];
                    if(c == '\n' || c == '\r') {
                        if(!line_buf.empty()) {
                            if(line_buf == "q" || line_buf == "quit") {
                                std::cout << "[Mock] Shutting down...\n";
                                g_state.handler->shutdown();
                                uv_stop(g_state.loop);
                            } else if(line_buf == "qs" || line_buf == "queue") {
                                sendControlRpc({{"type", "queueStatus"}});
                            } else if(line_buf == "s" || line_buf == "status") {
                                sendControlRpc({{"type", "statusRequest"}});
                            } else if(line_buf == "a" || line_buf == "adapters") {
                                sendControlRpc({{"type", "listAdapters"}});
                            } else if(line_buf == "av" || line_buf == "available") {
                                sendControlRpc({{"type", "listAvailableAdapters"}});
                            } else if(startsWith(line_buf, "config ")) {
                                std::string rest = line_buf.substr(7);
                                while(!rest.empty() && rest[0] == ' ') rest.erase(rest.begin());
                                auto sp = rest.find(' ');
                                if(sp == std::string::npos) {
                                    std::cout <<
                                        "[Mock] Usage: config <key> <value>  (e.g. config maxCorrectnessWorkers 4)\n";
                                } else {
                                    std::string key = rest.substr(0, sp);
                                    std::string val_str = rest.substr(sp + 1);
                                    while(!val_str.empty() && val_str[0] == ' ') val_str.erase(val_str.begin());
                                    try {
                                        int val = std::stoi(val_str);
                                        sendControlRpc(
                                            {
                                                {"type", "updateConfig"},
                                                {"config", {{key, val}}}
                                            }
                                        );
                                    } catch(const std::exception&) {
                                        std::cout << "[Mock] Invalid value (expected integer): " << val_str << "\n";
                                    }
                                }
                            } else if(startsWith(line_buf, "a-load ")) {
                                std::string rest = line_buf.substr(7);
                                while(!rest.empty() && rest[0] == ' ') rest.erase(rest.begin());
                                auto sp = rest.find(' ');
                                std::string name = (sp == std::string::npos) ? rest : rest.substr(0, sp);
                                nlohmann::json config = nlohmann::json::object();
                                if(sp != std::string::npos) {
                                    std::string json_str = rest.substr(sp + 1);
                                    while(!json_str.empty() && json_str[0] == ' ') json_str.erase(json_str.begin());
                                    try { config = nlohmann::json::parse(json_str); } catch(...) {
                                        std::cerr << "[Mock] Invalid config JSON\n";
                                    }
                                }
                                sendControlRpc(
                                    {
                                        {"type", "loadAdapter"},
                                        {"adapter", name},
                                        {"config", config}
                                    },
                                    15000
                                );
                            } else if(startsWith(line_buf, "a-unload ")) {
                                std::string rest = line_buf.substr(9);
                                while(!rest.empty() && rest[0] == ' ') rest.erase(rest.begin());
                                if(rest.empty()) {
                                    std::cout << "[Mock] Usage: a-unload <adapter_name>\n";
                                } else {
                                    sendControlRpc(
                                        {
                                            {"type", "unloadAdapter"},
                                            {"adapter", rest}
                                        }
                                    );
                                }
                            } else if(startsWith(line_buf, "cancel ") || startsWith(line_buf, "c ")) {
                                std::string rest = startsWith(line_buf, "c ")
                                    ? line_buf.substr(2)
                                    : line_buf.substr(7);
                                while(!rest.empty() && rest[0] == ' ') rest.erase(rest.begin());
                                if(rest.empty()) {
                                    std::cout << "[Mock] Usage: cancel <job_id>\n";
                                } else {
                                    sendControlRpc(
                                        {
                                            {"type", "cancelJob"},
                                            {"jobId", rest}
                                        }
                                    );
                                }
                            } else if(startsWith(line_buf, "job ") || startsWith(line_buf, "j ")) {
                                std::string rest = startsWith(line_buf, "j ")
                                    ? line_buf.substr(2)
                                    : line_buf.substr(4);
                                while(!rest.empty() && rest[0] == ' ') rest.erase(rest.begin());
                                if(rest.empty()) {
                                    std::cout << "[Mock] Usage: job <job_id>\n";
                                } else {
                                    sendControlRpc(
                                        {
                                            {"type", "getJobInfo"},
                                            {"jobId", rest}
                                        }
                                    );
                                }
                            } else if(startsWith(line_buf, "run ") || line_buf == "run") {
                                if(line_buf.size() <= 4) {
                                    std::cerr << "[Mock] Usage: run --test-id <id> --test-dir <dir> --solution <dir>"
                                        " [--solution <dir2>] [--mode all] [--threads 4] [--memory 1024]\n";
                                } else {
                                    submitRunRabbit(line_buf.substr(4));
                                }
                            } else if(line_buf == "rp" || line_buf == "providers") {
                                sendControlRpc({{"type", "listResourceProviders"}});
                            } else if(line_buf == "rpav") {
                                sendControlRpc({{"type", "listAvailableResourceProviders"}});
                            } else if(startsWith(line_buf, "rp-load ")) {
                                std::string rest = line_buf.substr(8);
                                while(!rest.empty() && rest[0] == ' ') rest.erase(rest.begin());
                                auto sp = rest.find(' ');
                                std::string name = (sp == std::string::npos) ? rest : rest.substr(0, sp);
                                nlohmann::json config = nlohmann::json::object();
                                if(sp != std::string::npos) {
                                    std::string json_str = rest.substr(sp + 1);
                                    while(!json_str.empty() && json_str[0] == ' ') json_str.erase(json_str.begin());
                                    try { config = nlohmann::json::parse(json_str); } catch(...) {
                                        std::cerr << "[Mock] Invalid config JSON\n";
                                    }
                                }
                                sendControlRpc(
                                    {
                                        {"type", "loadResourceProvider"},
                                        {"provider", name},
                                        {"config", config}
                                    },
                                    15000
                                );
                            } else if(startsWith(line_buf, "rp-unload ")) {
                                std::string rest = line_buf.substr(10);
                                while(!rest.empty() && rest[0] == ' ') rest.erase(rest.begin());
                                if(rest.empty()) {
                                    std::cout << "[Mock] Usage: rp-unload <provider_name>\n";
                                } else {
                                    sendControlRpc(
                                        {
                                            {"type", "unloadResourceProvider"},
                                            {"provider", rest}
                                        }
                                    );
                                }
                            } else {
                                std::cout << "[Mock] Unknown: '" << line_buf
                                    <<
                                    "'. Commands: s, qs, a, av, a-load/a-unload <name>, rp, rpav, rp-load/rp-unload <name>, cancel/job <id>, config <key> <val>, run <args>, q\n";
                            }
                            line_buf.clear();
                        }
                    } else {
                        line_buf += c;
                    }
                }
            } else if(nread < 0) {
                // EOF on stdin
                g_state.handler->shutdown();
                uv_stop(g_state.loop);
            }
            delete[] buf->base;
        }
    );

    // Run event loop
    uv_run(&loop, UV_RUN_DEFAULT);

    // Cleanup
    uv_close(reinterpret_cast<uv_handle_t*>(&tty), nullptr);
    uv_run(&loop, UV_RUN_NOWAIT);
    uv_loop_close(&loop);

    std::cout << "[Mock] Done.\n";
    return 0;
}
