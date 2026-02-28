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
        /// Must NOT publish online events — use notifyOnline() for that.
        virtual void start() = 0;

        /// Stop the adapter (blocks until fully shut down).
        virtual void stop() = 0;

        /// Publish online event / register with orchestrator.
        /// Called by AdapterManager AFTER start() succeeds and the adapter
        /// is fully registered in the loaded adapters map.  At this point
        /// buildTransportsList will include this adapter AND all previously
        /// loaded adapters with their complete configs.
        /// Default: no-op (adapters that don't publish online events).
        virtual void notifyOnline() {}
};