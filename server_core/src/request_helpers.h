#pragma once

/**
 * @file request_helpers.h
 * @brief Small parsing helpers shared between pipeline and adapters.
 */

#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>


namespace request_helpers {

    /// Best-effort: derive a human-readable solution name from a TaskSubmission
    /// request. For local solutions it is the basename of `path`; for git URLs
    /// it is the last path segment of `url` with a trailing ".git" stripped.
    /// Returns an empty string if the request lacks a usable solutionSource.
    inline std::string extract_solution_name(const nlohmann::json& request) {
        if(!request.contains("solutionSource")) return "";
        const auto& src = request["solutionSource"];
        if(src.contains("path")) {
            return std::filesystem::path(src.value("path", "")).filename().string();
        }
        if(src.contains("url")) {
            std::string url = src.value("url", "");
            auto pos = url.rfind('/');
            std::string name = (pos != std::string::npos) ? url.substr(pos + 1) : url;
            if(name.size() > 4 && name.substr(name.size() - 4) == ".git")
                name.resize(name.size() - 4);
            return name;
        }
        return "";
    }

} // namespace request_helpers
