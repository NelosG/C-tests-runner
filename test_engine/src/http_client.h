#pragma once
#include <string>
#include <nlohmann/json.hpp>

class HttpClient {
    public:
        explicit HttpClient(const std::string& url): url(url) {}


        void sendJson(const std::string& jsonPayload) const;

        void sendJson(const nlohmann::json& jsonPayload) const;

        std::string url;
};
