#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <string_view>

#include <mcp/client/types.hpp>

namespace mcp::client
{

    /**
     * @brief Retry policy for reconnectable transports.
     */
    struct RetryPolicy
    {
        std::size_t max_attempts = 0;
        std::chrono::milliseconds initial_delay{500};
        std::chrono::milliseconds max_delay{5000};
        double backoff_multiplier = 2.0;
    };

    /**
     * @brief Bundle of user-provided callbacks for observing client activity.
     */
    struct ClientCallbacks
    {
        std::function<void(LogLevel, std::string_view)> log;
        std::function<void(ConnectionState)> state_changed;
        std::function<void(const Notification &)> notification_received;
        std::function<void(const std::string &)> transport_error;
    };

    /**
     * @brief Top-level configuration object for a client instance.
     */
    struct ClientOptions
    {
        InitializeParams initialize;
        RequestOptions default_request;
        RetryPolicy retry;
        ClientCallbacks callbacks;
        bool auto_initialize = true;
        bool auto_reconnect = false;
    };

} // namespace mcp::client
