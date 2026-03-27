#pragma once

#include <functional>
#include <memory>
#include <string>

#include <mcp/client/types.hpp>

namespace mcp::client
{

    /**
     * @brief Abstract transport used by the MCP client runtime.
     *
     * A transport implementation is responsible only for moving serialized protocol
     * messages. It should not contain MCP business logic such as request routing or
     * capability negotiation.
     */
    class IClientTransport
    {
    public:
        using MessageHandler = std::function<void(const JsonString &)>;
        using ErrorHandler = std::function<void(const std::string &)>;
        using CloseHandler = std::function<void()>;

        virtual ~IClientTransport() = default;

        /**
         * @brief Open the underlying connection.
         */
        virtual void connect() = 0;

        /**
         * @brief Close the underlying connection and release owned resources.
         */
        virtual void close() = 0;

        /**
         * @brief Send one fully serialized MCP message.
         */
        virtual void send(const JsonString &message) = 0;

        /**
         * @brief Register the callback invoked for inbound messages.
         */
        virtual void set_message_handler(MessageHandler handler) = 0;

        /**
         * @brief Register the callback invoked when transport-level failures occur.
         */
        virtual void set_error_handler(ErrorHandler handler) = 0;

        /**
         * @brief Register the callback invoked after the transport closes.
         */
        virtual void set_close_handler(CloseHandler handler) = 0;
    };

    using ClientTransportPtr = std::shared_ptr<IClientTransport>;

} // namespace mcp::client
