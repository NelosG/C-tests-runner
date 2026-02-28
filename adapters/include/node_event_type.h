#pragma once

/**
 * @file node_event_type.h
 * @brief Node lifecycle / status event types shared by all adapters.
 */

#include <string>

/// Node event types used in buildNodeEvent() and adapter messages.
enum class node_event_type {
    online,   ///< Node came up (full payload: capabilities, transports, resourceProviders)
    offline,  ///< Node going down (minimal: nodeId, timestamp)
    info      ///< Node status query response (full payload + currentLoad)
};

inline std::string to_string(node_event_type t) {
    switch(t) {
        case node_event_type::online:  return "online";
        case node_event_type::offline: return "offline";
        case node_event_type::info:    return "info";
    }
    return "online";
}
