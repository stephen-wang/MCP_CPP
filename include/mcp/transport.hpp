#pragma once

#include <functional>
#include <string>

namespace mcp
{

    /**
     * @brief Common transport contract shared by MCP client and server runtimes.
     *
     * This interface owns the message plumbing that is identical across both
     * roles. Client and server specific transport interfaces can extend it with
     * lifecycle methods such as `connect()` or `listen()`.
     */
    class ITransport
    {
    public:
        using MessageHandler = std::function<void(const std::string &)>;
        using ErrorHandler = std::function<void(const std::string &)>;
        using CloseHandler = std::function<void()>;

        virtual ~ITransport() = default;

        /**
         * @brief Close the underlying transport and release owned resources.
         */
        virtual void close() = 0;

        /**
         * @brief Send one fully serialized MCP message.
         */
        virtual void send(const std::string &message) = 0;

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

} // namespace mcp