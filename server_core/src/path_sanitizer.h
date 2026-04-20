#pragma once

/**
 * @file path_sanitizer.h
 * @brief Strip absolute paths from text fields exposed to clients.
 *
 * The engine writes various paths (temp dirs, server install dir) into build
 * output and stderr. Leaking those into orchestrator/student-facing JSON
 * exposes server topology. sanitize_paths() collapses each absolute path to
 * `<...>/filename` so the basename remains for diagnostics.
 */

#include <cctype>
#include <string>


namespace path_sanitizer {

    inline std::string sanitize(const std::string& input) {
        static const auto is_boundary = [](char c) {
            return c == ' ' || c == '"' || c == '\'' || c == '=' || c == '\n' || c == '(' || c == '-';
        };
        static const auto is_path_end = [](char c) {
            return c == ' ' || c == '"' || c == '\'' || c == '\n' || c == ')';
        };

        std::string result;
        result.reserve(input.size());

        for(size_t i = 0; i < input.size();) {
            bool is_abs_unix = (input[i] == '/' && i + 1 < input.size()
                && input[i + 1] != ' ' && input[i + 1] != '\n');
            bool is_abs_win = (i + 2 < input.size()
                && std::isalpha(static_cast<unsigned char>(input[i]))
                && input[i + 1] == ':' && (input[i + 2] == '\\' || input[i + 2] == '/'));

            if((is_abs_unix || is_abs_win) && (i == 0 || is_boundary(input[i - 1]))) {
                size_t start = i;
                while(i < input.size() && !is_path_end(input[i])) ++i;
                std::string path = input.substr(start, i - start);
                while(!path.empty() && (path.back() == '/' || path.back() == '\\'))
                    path.pop_back();
                auto sep = path.find_last_of("/\\");
                result += (sep != std::string::npos && sep + 1 < path.size())
                    ? "<...>/" + path.substr(sep + 1)
                    : "<path>";
            } else {
                result += input[i++];
            }
        }
        return result;
    }

} // namespace path_sanitizer
