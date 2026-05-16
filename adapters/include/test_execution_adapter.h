#pragma once

/**
 * @file test_execution_adapter.h
 * @brief Adapter base class with shared ID generation utilities.
 *
 * Provides static helper methods for generating unique job and node
 * identifiers used by all transport adapters that handle test execution.
 */

#include <iomanip>
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
        static std::string generate_job_id() {
            std::uniform_int_distribution<uint32_t> dist;
            std::ostringstream ss;
            ss << "j-" << std::hex << dist(rng());
            return ss.str();
        }

        /// Generate a bearer auth token: "tok-<64 hex digits>" (256-bit entropy).
        /// Reads directly from std::random_device - backed by /dev/urandom on
        /// Linux and BCryptGenRandom on Windows - so the token is not
        /// recoverable from any other engine output. Do NOT route this through
        /// mt19937: a single leaked sample fully determines the next 19937 bits.
        static std::string generate_auth_token() {
            std::random_device rd;
            std::ostringstream ss;
            ss << "tok-" << std::hex << std::setfill('0');
            for(int i = 0; i < 8; ++i) {
                ss << std::setw(8) << rd();  // 8 * 32 = 256 bits
            }
            return ss.str();
        }

        /// Generate a unique node ID: "{hostname}-{random_hex}".
        static std::string generate_node_id() {
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
