#pragma once

#include <functional>
#include <string_view>

#include <mcp/server/types.hpp>

namespace mcp::server
{

    /**
     * @brief Bundle of user-provided callbacks for observing server activity.
     */
    struct ServerCallbacks
    {
        std::function<void(LogLevel, std::string_view)> log;
        std::function<void(ServerState)> state_changed;
        std::function<void(const Notification &)> notification_received;
        std::function<void(const std::string &)> transport_error;
    };

    /**
     * @brief Top-level configuration object for a server instance.
     */
    struct ServerOptions
    {
        std::string protocol_version = "2025-03-26";
        ServerInfo server_info;
        ServerCapabilities capabilities;
        ServerCallbacks callbacks;
        bool auto_handle_initialize = true;
    };

} // namespace mcp::server
