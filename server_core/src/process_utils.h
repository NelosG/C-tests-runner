#pragma once

/**
 * @file process_utils.h
 * @brief Cross-platform command execution utilities (header-only).
 */

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <string>

struct CommandResult {
    std::string output;
    int exit_code = -1;
    bool failed() const { return exit_code != 0; }
};

/// Escape and quote a string for safe interpolation into a shell command.
/// Prevents command injection via crafted paths or arguments.
inline std::string shell_quote(const std::string& arg) {
    #ifdef _WIN32
    // Windows cmd: wrap in double quotes, escape internal double quotes and backslashes
    std::string result = "\"";
    for(char c : arg) {
        if(c == '"') result += "\\\"";
        else if(c == '\\') result += "\\\\";
        else result += c;
    }
    result += "\"";
    return result;
    #else
    // POSIX: wrap in single quotes, escape embedded single quotes
    std::string result = "'";
    for(char c : arg) {
        if(c == '\'') result += "'\\''";
        else result += c;
    }
    result += "'";
    return result;
    #endif
}

inline CommandResult run_command(const std::string& cmd) {
    CommandResult result;
    std::array<char, 4096> buf{};

    // Cap captured output at 8 MiB so a runaway command (e.g. infinite warning
    // loop from a broken CMake invocation) doesn't OOM the server. We still
    // read the pipe to EOF so the child can finish - only the tail of output
    // is dropped.
    constexpr size_t kOutputCapBytes = 8 * 1024 * 1024;

    struct PipeGuard {
        FILE* fp = nullptr;

        ~PipeGuard() {
            if(fp) {
                #ifdef _WIN32
                _pclose(fp);
                #else
                pclose(fp);
                #endif
            }
        }
    } guard;

    #ifdef _WIN32
    // cmd /c requires outer quotes when command contains inner quotes
    std::string wrapped = "\"" + cmd + "\"";
    guard.fp = _popen(wrapped.c_str(), "r");
    #else
    guard.fp = popen(cmd.c_str(), "r");
    #endif

    if(!guard.fp) {
        result.output = "Failed to run command: " + cmd;
        return result;
    }

    bool truncated = false;
    while(fgets(buf.data(), static_cast<int>(buf.size()), guard.fp) != nullptr) {
        if(result.output.size() < kOutputCapBytes) {
            size_t room = kOutputCapBytes - result.output.size();
            size_t chunk = std::min(std::strlen(buf.data()), room);
            result.output.append(buf.data(), chunk);
            if(result.output.size() >= kOutputCapBytes && !truncated) {
                result.output += "\n[...command output truncated at 8 MiB...]\n";
                truncated = true;
            }
        }
        // Keep draining: stopping fgets early would close the pipe and SIGPIPE
        // the child on POSIX, sometimes hiding the real exit code.
    }

    // Close pipe and capture exit code
    FILE* fp = guard.fp;
    guard.fp = nullptr;  // prevent double-close in destructor
    #ifdef _WIN32
    result.exit_code = _pclose(fp);
    #else
    int status = pclose(fp);
    result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    #endif

    return result;
}
