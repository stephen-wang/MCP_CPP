#pragma once

#include <memory>

#include <mcp/transport.hpp>
#include <mcp/server/types.hpp>

namespace mcp::server
{

    /**
     * @brief Abstract transport used by the MCP server runtime.
     *
     * A transport implementation is responsible only for moving serialized protocol
     * messages. It should not contain MCP business logic such as routing or
     * validation.
     */
    class IServerTransport : public mcp::ITransport
    {
    public:
        using MessageHandler = mcp::ITransport::MessageHandler;
        using ErrorHandler = mcp::ITransport::ErrorHandler;
        using CloseHandler = mcp::ITransport::CloseHandler;

        ~IServerTransport() override = default;

        /**
         * @brief Begin accepting inbound traffic.
         */
        virtual void listen() = 0;
    };

    using ServerTransportPtr = std::shared_ptr<IServerTransport>;

} // namespace mcp::server
