#pragma once

/**
 * @file transport_adapter.h
 * @brief Base interface for transport adapters (HTTP, RabbitMQ, etc.).
 *
 * Each adapter is compiled as a separate shared library (.dll/.so) and
 * loaded at runtime by the server. Adapters receive test tasks from an
 * external source, execute them via TestRunnerService, and deliver
 * results back through their transport.
 */

#include <string>

class TransportAdapter {
    public:
        virtual ~TransportAdapter() = default;

        /// Human-readable name for logging (e.g. "HTTP", "RabbitMQ").
        virtual std::string name() const = 0;

        /// Start the adapter (non-blocking — launches internal threads).
        virtual void start() = 0;

        /// Stop the adapter (blocks until fully shut down).
        virtual void stop() = 0;
};