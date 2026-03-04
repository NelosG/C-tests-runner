#pragma once

/**
 * @file test_execution_adapter.h
 * @brief Adapter base class with shared ID generation utilities.
 *
 * Provides static helper methods for generating unique job and node
 * identifiers used by all transport adapters that handle test execution.
 */

#include <random>
#include <sstream>
#include <string>
#include <transport_adapter.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

class TestExecutionAdapter : public TransportAdapter {
    public:
        /// Generate a unique random job ID (e.g. "j-a3f7b2c1").
        static std::string generateJobId() {
            std::uniform_int_distribution<uint32_t> dist;
            std::ostringstream ss;
            ss << "j-" << std::hex << dist(rng());
            return ss.str();
        }

        /// Generate a bearer auth token: "tok-<32 hex digits>" (128-bit entropy).
        static std::string generateAuthToken() {
            std::uniform_int_distribution<uint32_t> dist;
            std::ostringstream ss;
            ss << "tok-" << std::hex
                << dist(rng()) << dist(rng())
                << dist(rng()) << dist(rng());
            return ss.str();
        }

        /// Generate a unique node ID: "{hostname}-{random_hex}".
        static std::string generateNodeId() {
            #ifdef _WIN32
            char hostname[256] = {};
            DWORD size = sizeof(hostname);
            GetComputerNameA(hostname, &size);
            #else
            char hostname[256] = {};
            gethostname(hostname, sizeof(hostname));
            #endif
            std::uniform_int_distribution<uint32_t> dist;
            std::ostringstream ss;
            ss << hostname << "-" << std::hex << dist(rng());
            return ss.str();
        }

    private:
        static std::mt19937& rng() {
            thread_local std::mt19937 gen(std::random_device{}());
            return gen;
        }
};