#pragma once

/**
 * @file control_type.h
 * @brief Type-safe enum for RabbitMQ control message types.
 *
 * Wire protocol constants — enum prevents typos in handler
 * registration and response type derivation.
 */

#include <stdexcept>
#include <string>

/// RabbitMQ control message types (node.fanout RPC).
enum class control_type {
    queue_status,
    status_request,
    get_job_info,
    cancel_job,
    list_adapters,
    list_available_adapters,
    load_adapter,
    unload_adapter,
    update_config,
    list_resource_providers,
    list_available_resource_providers,
    load_resource_provider,
    unload_resource_provider
};

inline std::string to_string(control_type c) {
    switch(c) {
        case control_type::queue_status: return "queueStatus";
        case control_type::status_request: return "statusRequest";
        case control_type::get_job_info: return "getJobInfo";
        case control_type::cancel_job: return "cancelJob";
        case control_type::list_adapters: return "listAdapters";
        case control_type::list_available_adapters: return "listAvailableAdapters";
        case control_type::load_adapter: return "loadAdapter";
        case control_type::unload_adapter: return "unloadAdapter";
        case control_type::update_config: return "updateConfig";
        case control_type::list_resource_providers: return "listResourceProviders";
        case control_type::list_available_resource_providers: return "listAvailableResourceProviders";
        case control_type::load_resource_provider: return "loadResourceProvider";
        case control_type::unload_resource_provider: return "unloadResourceProvider";
    }
    return "queueStatus";
}

inline control_type control_type_from_string(const std::string& s) {
    if(s == "queueStatus") return control_type::queue_status;
    if(s == "statusRequest") return control_type::status_request;
    if(s == "getJobInfo") return control_type::get_job_info;
    if(s == "cancelJob") return control_type::cancel_job;
    if(s == "listAdapters") return control_type::list_adapters;
    if(s == "listAvailableAdapters") return control_type::list_available_adapters;
    if(s == "loadAdapter") return control_type::load_adapter;
    if(s == "unloadAdapter") return control_type::unload_adapter;
    if(s == "updateConfig") return control_type::update_config;
    if(s == "listResourceProviders") return control_type::list_resource_providers;
    if(s == "listAvailableResourceProviders") return control_type::list_available_resource_providers;
    if(s == "loadResourceProvider") return control_type::load_resource_provider;
    if(s == "unloadResourceProvider") return control_type::unload_resource_provider;
    throw std::invalid_argument("Unknown control type: '" + s + "'");
}

inline bool is_valid_control_type(const std::string& s) {
    return s == "queueStatus" || s == "statusRequest"
        || s == "getJobInfo" || s == "cancelJob"
        || s == "listAdapters" || s == "listAvailableAdapters"
        || s == "loadAdapter" || s == "unloadAdapter"
        || s == "updateConfig"
        || s == "listResourceProviders" || s == "listAvailableResourceProviders"
        || s == "loadResourceProvider" || s == "unloadResourceProvider";
}

/// Derive response type string from control type: to_string(c) + "Response".
inline std::string response_type(control_type c) {
    return to_string(c) + "Response";
}