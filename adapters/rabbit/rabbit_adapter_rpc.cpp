// RPC control-message handlers - split out of rabbit_adapter.cpp to keep
// the AMQP transport / event-loop wiring readable on its own. Defines the
// dispatch table that on_control_message() looks up, plus the small
// require_management() guard used by management-API handlers.
#include <rabbit_adapter.h>
#include <adapter_status.h>
#include <adapter_utils.h>
#include <api_types.h>
#include <iostream>

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
