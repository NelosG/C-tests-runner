#include <http_client.h>
#include <stdexcept>
#include <curl/curl.h>

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    (void)userp;
    return size * nmemb;
}

void HttpClient::sendJson(const nlohmann::json& jsonPayload) const {
    sendJson(jsonPayload.dump());
}

void HttpClient::sendJson(const std::string& jsonPayload) const {
    curl_global_init(CURL_GLOBAL_ALL);
    CURL* curl = curl_easy_init();
    if(!curl) {
        throw std::runtime_error("Failed to initialize cURL");
    }

    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonPayload.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);

    if(const CURLcode res = curl_easy_perform(curl); res != CURLE_OK) {
        const std::string err = "cURL request failed: " + std::string(curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        throw std::runtime_error(err);
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    curl_global_cleanup();
}
