#pragma once

/**
 * @file process_utils.h
 * @brief Cross-platform command execution utilities (header-only).
 */

#include <array>
#include <cstdio>
#include <string>

struct CommandResult {
    std::string output;
    int exit_code = -1;
    bool failed() const { return exit_code != 0; }
};

inline CommandResult runCommand(const std::string& cmd) {
    CommandResult result;
    std::array < char, 4096 > buf{};

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

    while(fgets(buf.data(), static_cast<int>(buf.size()), guard.fp) != nullptr) {
        result.output += buf.data();
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