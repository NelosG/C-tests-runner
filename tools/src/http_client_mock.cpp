/**
 * @file http_client_mock.cpp
 * @brief Mock orchestrator — manages test jobs and monitors nodes via HTTP.
 *
 * Listens on a configurable port for node registration and result callbacks.
 * Tracks registered nodes and can poll their status on demand.
 *
 * Usage:
 *   http_client_mock [port]
 *
 * Defaults: port 9000
 *
 * Commands (type in console):
 *   s                  — poll GET /api/node/status on all registered nodes
 *   l                  — list registered nodes
 *   a                  — list all adapters on all nodes (GET /api/adapters)
 *   av                 — list available (not loaded) adapters (GET /api/adapters/available)
 *   load <name> [json] — load adapter on all nodes (POST /api/adapters/:name)
 *   unload <name>      — unload adapter on all nodes (DELETE /api/adapters/:name)
 *   run <args>         — submit test jobs (see below)
 *   qs                 — poll queue status on all nodes (GET /api/status)
 *   cancel <job_id>    — cancel a queued job on all nodes (DELETE /api/jobs/:id)
 *   job <job_id>       — query job status/result on all nodes (GET /api/jobs/:id)
 *   config <key> <val> — set config on all nodes (PUT /api/config), e.g. config maxCorrectnessWorkers 4
 *   q                  — quit
 *
 * Run command:
 *   run --test-id <id> --test-dir <dir> --solution <dir> [--solution <dir2>]
 *       [--mode correctness|performance|all] [--threads N]
 *
 * Automatically confirms registration requests with {"status":"registered"}.
 * Results are received via PUT /api/results callback (no polling).
 */

#include <httplib.h>
#include <algorithm>
#include <atomic>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <condition_variable>
#include <time_utils.h>
#include <nlohmann/json.hpp>
#include "mock_utils.h"

namespace fs = std::filesystem;

struct RegisteredNode {
	std::string node_id;
	std::string transport;
	int port = 0;
	std::string auth_token;
	std::string orchestrator_token;
	nlohmann::json capabilities;
	std::string registered_at;
};

/// Encapsulates all mutable state for the HTTP mock orchestrator.
struct MockState {
	std::mutex nodes_mutex;
	std::vector<RegisteredNode> nodes;
	std::atomic<bool> running{true};
	int listen_port = 9000;
	size_t node_idx = 0;  ///< Round-robin index for load balancing.

	// Callback result collection
	std::mutex results_mutex;
	std::condition_variable results_cv;
	std::unordered_map<std::string, nlohmann::json> callback_results;
};

static MockState g_state;

static std::string generateToken() {
	thread_local std::mt19937 gen(std::random_device{}());
	std::uniform_int_distribution<uint32_t> dist;
	std::ostringstream ss;
	ss << "tok-" << std::hex << dist(gen) << dist(gen);
	return ss.str();
}

static void removeNodes(const std::vector<std::string>& dead_ids) {
	if(dead_ids.empty()) return;
	std::lock_guard lock(g_state.nodes_mutex);
	for(auto& id : dead_ids) {
		g_state.nodes.erase(
			std::remove_if(g_state.nodes.begin(), g_state.nodes.end(),
				[&](const RegisteredNode& n) { return n.node_id == id; }),
			g_state.nodes.end()
		);
		std::cout << "[Mock] Removed unreachable node: " << id << "\n";
	}
}

static void listNodes() {
	std::lock_guard lock(g_state.nodes_mutex);
	if(g_state.nodes.empty()) {
		std::cout << "[Mock] No nodes registered\n";
		return;
	}
	std::cout << "[Mock] Registered nodes (" << g_state.nodes.size() << "):\n";
	for(size_t i = 0; i < g_state.nodes.size(); ++i) {
		auto& n = g_state.nodes[i];
		std::cout << "  " << (i + 1) << ". " << n.node_id
			<< " (transport=" << n.transport << ", port=" << n.port << ")\n";
	}
}

/// Execute a callback for each registered node, handling errors and dead node cleanup.
/// The callback receives a configured httplib::Client and the node reference.
/// Return true from callback on success (for counting).
using NodeCallback = std::function<bool(httplib::Client&, const RegisteredNode&)>;

static void forEachNode(
	const std::string& action_desc,
	NodeCallback callback,
	int connect_timeout = 5,
	int read_timeout = 5
) {
	std::vector<RegisteredNode> nodes_copy;
	{
		std::lock_guard lock(g_state.nodes_mutex);
		nodes_copy = g_state.nodes;
	}

	if(nodes_copy.empty()) {
		std::cout << "[Mock] No nodes registered\n";
		return;
	}

	std::cout << "[Mock] " << action_desc << " on " << nodes_copy.size() << " node(s)...\n";

	std::vector<std::string> dead;
	for(auto& node : nodes_copy) {
		std::string base_url = "http://localhost:" + std::to_string(node.port);
		try {
			httplib::Client client(base_url);
			client.set_connection_timeout(connect_timeout);
			client.set_read_timeout(read_timeout);
			if(!node.auth_token.empty())
				client.set_bearer_token_auth(node.auth_token);

			if(!callback(client, node)) {
				dead.push_back(node.node_id);
			}
		} catch(const std::exception& e) {
			std::cerr << "[Mock] " << node.node_id << " error: " << e.what() << "\n";
			dead.push_back(node.node_id);
		}
	}
	removeNodes(dead);
}

/// Handle common HTTP response patterns: 401 = dead, error = dead, success = parse JSON.
static bool handleResponse(
	const httplib::Result& res,
	const std::string& node_id,
	const std::function<void(const nlohmann::json&)>& on_success
) {
	if(res && (res->status == 200 || res->status == 201)) {
		auto resp = nlohmann::json::parse(res->body);
		on_success(resp);
		return true;
	}
	if(res && res->status == 401) {
		std::cerr << "[Mock] " << node_id << " returned HTTP 401 (stale token)\n";
		return false;
	}
	if(res) {
		std::cerr << "[Mock] " << node_id << " returned HTTP " << res->status;
		if(!res->body.empty()) std::cerr << ": " << res->body;
		std::cerr << "\n";
		return true; // not dead, just an error
	}
	std::cerr << "[Mock] " << node_id << " unreachable: "
		<< httplib::to_string(res.error()) << "\n";
	return false;
}

static void pollStatus() {
	forEachNode("Polling status", [](httplib::Client& client, const RegisteredNode& node) {
		auto res = client.Get("/api/node/status");
		return handleResponse(res, node.node_id, [&](const nlohmann::json& resp) {
			std::cout << "[Mock] Status from " << node.node_id << ":\n"
				<< resp.dump(2) << "\n";
		});
	});
}

static void pollQueueStatus() {
	forEachNode("Polling queue status", [](httplib::Client& client, const RegisteredNode& node) {
		auto res = client.Get("/api/status");
		return handleResponse(res, node.node_id, [&](const nlohmann::json& resp) {
			std::cout << "[Mock] Queue status from " << node.node_id << ":\n"
				<< resp.dump(2) << "\n";
		});
	});
}

static void listAdapters() {
	forEachNode("Listing adapters", [](httplib::Client& client, const RegisteredNode& node) {
		auto res = client.Get("/api/adapters");
		return handleResponse(res, node.node_id, [&](const nlohmann::json& resp) {
			std::cout << "[Mock] Adapters on " << node.node_id << ":\n"
				<< resp.dump(2) << "\n";
		});
	});
}

static void listAvailableAdapters() {
	forEachNode("Listing available adapters", [](httplib::Client& client, const RegisteredNode& node) {
		auto res = client.Get("/api/adapters/available");
		return handleResponse(res, node.node_id, [&](const nlohmann::json& resp) {
			std::cout << "[Mock] Available adapters on " << node.node_id << ":\n"
				<< resp.dump(2) << "\n";
		});
	});
}

static void loadAdapter(const std::string& adapter_name, const std::string& config_json) {
	std::string body = config_json.empty() ? "{}" : config_json;
	std::string path = "/api/adapters/" + adapter_name;
	forEachNode(
		"Loading adapter '" + adapter_name + "'",
		[&](httplib::Client& client, const RegisteredNode& node) {
			auto res = client.Post(path, body, "application/json");
			return handleResponse(res, node.node_id, [&](const nlohmann::json& resp) {
				std::cout << "[Mock] " << node.node_id << ": " << resp.dump() << "\n";
			});
		},
		10, 15
	);
}

static void unloadAdapter(const std::string& adapter_name) {
	std::string path = "/api/adapters/" + adapter_name;
	forEachNode(
		"Unloading adapter '" + adapter_name + "'",
		[&](httplib::Client& client, const RegisteredNode& node) {
			auto res = client.Delete(path);
			return handleResponse(res, node.node_id, [&](const nlohmann::json& resp) {
				std::cout << "[Mock] " << node.node_id << ": " << resp.dump() << "\n";
			});
		}
	);
}

static void cancelJob(const std::string& job_id) {
	std::string path = "/api/jobs/" + job_id;
	forEachNode(
		"Cancelling job '" + job_id + "'",
		[&](httplib::Client& client, const RegisteredNode& node) {
			auto res = client.Delete(path);
			return handleResponse(res, node.node_id, [&](const nlohmann::json& resp) {
				std::cout << "[Mock] " << node.node_id << ": " << resp.dump() << "\n";
			});
		}
	);
}

static void getJobInfo(const std::string& job_id) {
	std::string path = "/api/jobs/" + job_id;
	forEachNode(
		"Querying job '" + job_id + "'",
		[&](httplib::Client& client, const RegisteredNode& node) {
			auto res = client.Get(path);
			return handleResponse(res, node.node_id, [&](const nlohmann::json& resp) {
				std::cout << "[Mock] " << node.node_id << ":\n" << resp.dump(2) << "\n";
			});
		}
	);
}

static void setConfig(const std::string& key, const std::string& value) {
	nlohmann::json body;
	try {
		int n = std::stoi(value);
		body[key] = n;
	} catch(const std::exception&) {
		std::cout << "[Mock] Invalid value: " << value << "\n";
		return;
	}
	forEachNode(
		"Setting " + key + "=" + value,
		[&](httplib::Client& client, const RegisteredNode& node) {
			auto res = client.Put("/api/config", body.dump(), "application/json");
			return handleResponse(res, node.node_id, [&](const nlohmann::json& resp) {
				std::cout << "[Mock] " << node.node_id << ": " << resp.dump() << "\n";
			});
		}
	);
}

// ============================================================================
// Run command — submit test jobs to registered nodes
// ============================================================================

static void submitRun(const std::string& args_str) {
	RunArgs args;
	if(!parseRunArgs(args_str, args)) return;

	std::vector<RegisteredNode> nodes_copy;
	{
		std::lock_guard lock(g_state.nodes_mutex);
		nodes_copy = g_state.nodes;
	}

	if(nodes_copy.empty()) {
		std::cout << "[Mock] No nodes registered\n";
		return;
	}

	std::string lane = (args.mode == "performance" || args.mode == "all")
		? "performance" : "correctness";
	std::cout << "\n[Mock] ========== Submitting test run ==========\n"
		<< "  test_id:    " << args.test_id << "\n"
		<< "  test_dir:   " << args.test_dir << "\n"
		<< "  mode:       " << args.mode << " (lane: " << lane << ")\n"
		<< "  threads:    " << args.threads << "\n"
		<< "  solutions:  " << args.solutions.size() << "\n";
	for(size_t i = 0; i < args.solutions.size(); ++i) {
		std::cout << "    [" << (i + 1) << "] " << fs::path(args.solutions[i]).filename().string()
			<< " (" << args.solutions[i] << ")\n";
	}
	std::cout << "  nodes:      " << nodes_copy.size() << "\n"
		<< "  callback:   http://localhost:" << g_state.listen_port << "/api/results\n";

	struct SubmittedJob {
		std::string job_id;
		std::string solution;
		std::string mode;
	};
	std::vector<SubmittedJob> jobs;
	std::vector<std::string> dead;

	// Clear previous callback results
	{
		std::lock_guard lock(g_state.results_mutex);
		g_state.callback_results.clear();
	}

	// Submit one job per solution (round-robin load balancing)
	for(auto& sol_dir : args.solutions) {
		auto& node = nodes_copy[g_state.node_idx % nodes_copy.size()];
		g_state.node_idx++;

		std::string base_url = "http://localhost:" + std::to_string(node.port);
		try {
			httplib::Client client(base_url);
			client.set_connection_timeout(10);
			client.set_read_timeout(30);
			if(!node.auth_token.empty())
				client.set_bearer_token_auth(node.auth_token);

			nlohmann::json request = {
				{"testId", args.test_id},
				{"testDir", args.test_dir},
				{"solutionDir", sol_dir},
				{"mode", args.mode},
				{"threads", args.threads},
				{"callbackUrl", "http://localhost:" + std::to_string(g_state.listen_port) + "/api/results"}
			};

			std::string sol_name = fs::path(sol_dir).filename().string();
			std::cout << "[Mock] Submitting: " << sol_name
				<< " | mode=" << args.mode << " | node=" << node.node_id
				<< " | port=" << node.port << "\n";

			auto res = client.Post("/api/run", request.dump(), "application/json");
			if(res && (res->status >= 200 && res->status < 300)) {
				auto resp = nlohmann::json::parse(res->body);
				std::string job_id = resp.value("jobId", "");
				std::cout << "[Mock] -> accepted: job_id=" << job_id << "\n";

				jobs.push_back({job_id, sol_name, args.mode});
			} else if(res && res->status == 401) {
				std::cerr << "[Mock] " << node.node_id << " HTTP 401 (stale token)\n";
				dead.push_back(node.node_id);
			} else if(res) {
				std::cerr << "[Mock] " << node.node_id << " HTTP " << res->status;
				if(!res->body.empty()) std::cerr << ": " << res->body;
				std::cerr << "\n";
			} else {
				std::cerr << "[Mock] " << node.node_id << " unreachable\n";
				dead.push_back(node.node_id);
			}
		} catch(const std::exception& e) {
			std::cerr << "[Mock] " << node.node_id << " error: " << e.what() << "\n";
		}
	}
	removeNodes(dead);

	if(jobs.empty()) {
		std::cerr << "[Mock] No jobs submitted\n";
		return;
	}

	std::cout << "[Mock] " << jobs.size() << " job(s) submitted, waiting for callbacks...\n";

	// Wait for all callbacks
	int completed = 0, failed = 0;
	size_t remaining = jobs.size();

	std::unique_lock lock(g_state.results_mutex);
	while(remaining > 0) {
		g_state.results_cv.wait_for(lock, std::chrono::seconds(5), [&]() {
			for(auto& job : jobs) {
				if(g_state.callback_results.count(job.job_id)) return true;
			}
			return false;
		});

		for(auto it = jobs.begin(); it != jobs.end(); ) {
			auto found = g_state.callback_results.find(it->job_id);
			if(found != g_state.callback_results.end()) {
				auto& result = found->second;
				std::string status = result.value("status", "");
				std::string job_id = result.value("jobId", it->job_id);

				if(status == "failed") {
					failed++;
					std::cerr << "\n=== " << it->solution << " [" << it->mode
						<< "] FAILED (job=" << job_id << ") ===\n";
					if(result.contains("error"))
						std::cerr << "  Error: " << result["error"].get<std::string>() << "\n";
					if(result.contains("correctness")) {
						std::cout << "  Correctness: " << summarizeTests(result["correctness"]) << "\n";
					}
					if(result.value("performanceSkipped", false)) {
						std::cout << "  Performance: SKIPPED ("
							<< result.value("performanceSkipReason", "") << ")\n";
					}
					std::cout << result.dump(2) << "\n";
				} else {
					completed++;
					std::cout << "\n=== " << it->solution << " [" << it->mode
						<< "] COMPLETED (job=" << job_id << ") ===\n";
					if(result.contains("correctness"))
						std::cout << "  Correctness: " << summarizeTests(result["correctness"]) << "\n";
					if(result.contains("performance"))
						std::cout << "  Performance: " << summarizeTests(result["performance"]) << "\n";
					std::cout << result.dump(2) << "\n";
				}

				g_state.callback_results.erase(found);
				it = jobs.erase(it);
				remaining--;
			} else {
				++it;
			}
		}

		if(remaining > 0) {
			std::cout << "[Mock] Waiting... " << remaining << " job(s) remaining\n";
		}
	}

	std::cout << "\n[Mock] Done: " << completed << " completed, " << failed << " failed\n";
}

int main(int argc, char** argv) {
	if(argc > 1) {
		try { g_state.listen_port = std::stoi(argv[1]); }
		catch(const std::exception&) {
			std::cerr << "[Mock] Invalid port: " << argv[1] << "\n";
			return 1;
		}
	}

	httplib::Server svr;

	// Bearer token auth middleware — verify orchestrator_token on incoming requests
	svr.set_pre_routing_handler(
		[](const httplib::Request& req, httplib::Response& res) -> httplib::Server::HandlerResponse {
			// Registration is the only unauthenticated endpoint
			if(req.method == "POST" && req.path == "/api/register") {
				return httplib::Server::HandlerResponse::Unhandled;
			}

			// Extract Bearer token from header
			auto it = req.headers.find("Authorization");
			if(it == req.headers.end() || it->second.substr(0, 7) != "Bearer ") {
				res.status = 401;
				res.set_content(
					R"({"error":"Unauthorized: missing Bearer token"})",
					"application/json"
				);
				return httplib::Server::HandlerResponse::Handled;
			}
			std::string token = it->second.substr(7);

			// Check token against any registered node's orchestrator_token
			{
				std::lock_guard lock(g_state.nodes_mutex);
				for(const auto& node : g_state.nodes) {
					if(node.orchestrator_token == token) {
						return httplib::Server::HandlerResponse::Unhandled;
					}
				}
			}

			res.status = 401;
			res.set_content(
				R"({"error":"Unauthorized: invalid Bearer token"})",
				"application/json"
			);
			return httplib::Server::HandlerResponse::Handled;
		}
	);

	// POST /api/register — accept node registration or deregistration
	svr.Post(
		"/api/register",
		[](const httplib::Request& req, httplib::Response& res) {
			try {
				auto body = nlohmann::json::parse(req.body);
				std::string type = body.value("type", "register");
				std::string node_id = body.value("nodeId", "unknown");

				if(type == "deregister") {
					std::lock_guard lock(g_state.nodes_mutex);
					auto it = std::remove_if(g_state.nodes.begin(), g_state.nodes.end(),
						[&](const RegisteredNode& n) { return n.node_id == node_id; });
					if(it != g_state.nodes.end()) {
						g_state.nodes.erase(it, g_state.nodes.end());
						std::cout << "[Mock] DEREGISTER: node_id=" << node_id << " -> REMOVED\n";
					} else {
						std::cout << "[Mock] DEREGISTER: node_id=" << node_id << " -> NOT FOUND\n";
					}
					res.set_content(
						nlohmann::json{
							{"status", "deregistered"},
							{"nodeId", node_id}
						}.dump(),
						"application/json"
					);
					return;
				}

				std::string transport = body.value("transport", "unknown");
				int port = body.value("port", 0);
				std::string auth_token = body.value("authToken", "");
				std::string orchestrator_token = generateToken();

				RegisteredNode node;
				node.node_id = node_id;
				node.transport = transport;
				node.port = port;
				node.auth_token = auth_token;
				node.orchestrator_token = orchestrator_token;
				node.capabilities = body.value("capabilities", nlohmann::json::object());
				node.registered_at = nowISO8601();

				{
					std::lock_guard lock(g_state.nodes_mutex);
					bool found = false;
					for(auto& existing : g_state.nodes) {
						if(existing.node_id == node_id) {
							existing = node;
							found = true;
							break;
						}
					}
					if(!found) {
						g_state.nodes.push_back(node);
					}
				}

				std::cout << "[Mock] REGISTER: node_id=" << node_id
					<< " transport=" << transport << " port=" << port
					<< " auth_token=" << auth_token
					<< " orchestrator_token=" << orchestrator_token << " -> CONFIRMED\n";

				nlohmann::json response = {
					{"status", "registered"},
					{"nodeId", node_id},
					{"orchestratorAuthToken", orchestrator_token},
					{"timestamp", nowISO8601()}
				};
				res.set_content(response.dump(), "application/json");
			} catch(const std::exception& e) {
				std::cerr << "[Mock] Registration parse error: " << e.what() << "\n";
				res.status = 400;
				res.set_content(
					nlohmann::json{{"error", e.what()}}.dump(),
					"application/json"
				);
			}
		}
	);

	// PUT /api/results — receive callback results from nodes
	svr.Put(
		"/api/results",
		[](const httplib::Request& req, httplib::Response& res) {
			try {
				auto body = nlohmann::json::parse(req.body);
				std::string job_id = body.value("jobId", "");

				if(job_id.empty()) {
					res.status = 400;
					res.set_content(R"({"error":"Missing job_id"})", "application/json");
					return;
				}

				std::string cb_status = body.value("status", "unknown");
			std::string cb_mode = body.value("mode", "?");
			std::string cb_solution = body.value("solution", "?");
			std::cout << "[Mock] Callback received: job=" << job_id
				<< " status=" << cb_status
				<< " mode=" << cb_mode
				<< " solution=" << cb_solution << "\n";

				{
					std::lock_guard lock(g_state.results_mutex);
					g_state.callback_results[job_id] = std::move(body);
				}
				g_state.results_cv.notify_all();

				res.set_content(R"({"status":"ok"})", "application/json");
			} catch(const std::exception& e) {
				std::cerr << "[Mock] Callback parse error: " << e.what() << "\n";
				res.status = 400;
				res.set_content(
					nlohmann::json{{"error", e.what()}}.dump(),
					"application/json"
				);
			}
		}
	);

	// GET /api/health — simple health check
	svr.Get(
		"/api/health",
		[](const httplib::Request&, httplib::Response& res) {
			res.set_content(R"({"status":"ok"})", "application/json");
		}
	);

	// Start HTTP server in background thread
	std::thread server_thread(
		[&svr]() {
			std::cout << "[Mock] Listening on 0.0.0.0:" << g_state.listen_port << "\n";
			svr.listen("0.0.0.0", g_state.listen_port);
			std::cout << "[Mock] Server stopped\n";
		}
	);

	std::cout << "[Mock] HTTP registration mock started.\n";
	std::cout << "[Mock] Nodes should POST to http://localhost:" << g_state.listen_port << "/api/register\n";
	std::cout << "[Mock] Commands: s=status, l=list, qs=queue status, a=adapters, av=available,\n"
		<< "                load/unload <name>, cancel/job <id>, config <key> <val>, run <args>, q=quit\n";

	// Read commands from stdin
	std::string line;
	auto trim_right = [](std::string& s) {
		while(!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' '))
			s.pop_back();
	};

	while(g_state.running && std::getline(std::cin, line)) {
		trim_right(line);
		if(line.empty()) continue;

		if(line == "q" || line == "quit") {
			std::cout << "[Mock] Shutting down...\n";
			g_state.running = false;
			svr.stop();
			break;
		} else if(line == "s" || line == "status") {
			pollStatus();
		} else if(line == "l" || line == "list") {
			listNodes();
		} else if(line == "a" || line == "adapters") {
			listAdapters();
		} else if(line == "av" || line == "available") {
			listAvailableAdapters();
		} else if(startsWith(line, "load ")) {
			std::string rest = line.substr(5);
			while(!rest.empty() && rest[0] == ' ') rest.erase(rest.begin());
			auto space_pos = rest.find(' ');
			if(space_pos == std::string::npos) {
				loadAdapter(rest, "{}");
			} else {
				std::string name = rest.substr(0, space_pos);
				std::string json = rest.substr(space_pos + 1);
				while(!json.empty() && json[0] == ' ') json.erase(json.begin());
				loadAdapter(name, json);
			}
		} else if(startsWith(line, "run ") || line == "run") {
			if(line.size() <= 4) {
				std::cerr << "[Mock] Usage: run --test-id <id> --test-dir <dir> --solution <dir>"
					" [--solution <dir2>] [--mode all] [--threads 4]\n";
			} else {
				submitRun(line.substr(4));
			}
		} else if(startsWith(line, "unload ") || startsWith(line, "u ")) {
			std::string rest = startsWith(line, "u ")
				? line.substr(2) : line.substr(7);
			while(!rest.empty() && rest[0] == ' ') rest.erase(rest.begin());
			if(rest.empty()) {
				std::cout << "[Mock] Usage: unload <adapter_name>\n";
			} else {
				unloadAdapter(rest);
			}
		} else if(startsWith(line, "cancel ") || startsWith(line, "c ")) {
			std::string rest = startsWith(line, "c ")
				? line.substr(2) : line.substr(7);
			while(!rest.empty() && rest[0] == ' ') rest.erase(rest.begin());
			if(rest.empty()) {
				std::cout << "[Mock] Usage: cancel <job_id>\n";
			} else {
				cancelJob(rest);
			}
		} else if(line == "qs" || line == "queue") {
			pollQueueStatus();
		} else if(startsWith(line, "config ")) {
			std::string rest = line.substr(7);
			while(!rest.empty() && rest[0] == ' ') rest.erase(rest.begin());
			auto space_pos = rest.find(' ');
			if(space_pos == std::string::npos) {
				// Legacy: "config <N>" sets maxCorrectnessWorkers
				try {
					std::stoi(rest);
					setConfig("maxCorrectnessWorkers", rest);
				} catch(const std::exception&) {
					std::cout << "[Mock] Usage: config <key> <value>  (e.g. config maxCorrectnessWorkers 4, config jobRetentionSeconds 600)\n";
				}
			} else {
				std::string key = rest.substr(0, space_pos);
				std::string val = rest.substr(space_pos + 1);
				while(!val.empty() && val[0] == ' ') val.erase(val.begin());
				setConfig(key, val);
			}
		} else if(startsWith(line, "job ") || startsWith(line, "j ")) {
			std::string rest = startsWith(line, "j ")
				? line.substr(2) : line.substr(4);
			while(!rest.empty() && rest[0] == ' ') rest.erase(rest.begin());
			if(rest.empty()) {
				std::cout << "[Mock] Usage: job <job_id>\n";
			} else {
				getJobInfo(rest);
			}
		} else {
			std::cout << "[Mock] Unknown command: '" << line
				<< "'. Commands: s, l, qs, a, av, load/unload <name>, cancel/job <id>, config <key> <val>, run <args>, q\n";
		}
	}

	if(server_thread.joinable()) {
		server_thread.join();
	}

	std::cout << "[Mock] Done.\n";
	return 0;
}
