#pragma once

#include <memory>

#include <mcp/transport.hpp>
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
    class IClientTransport : public mcp::ITransport
    {
    public:
        using MessageHandler = mcp::ITransport::MessageHandler;
        using ErrorHandler = mcp::ITransport::ErrorHandler;
        using CloseHandler = mcp::ITransport::CloseHandler;

        ~IClientTransport() override = default;

        /**
         * @brief Open the underlying connection.
         */
        virtual void connect() = 0;
    };

    using ClientTransportPtr = std::shared_ptr<IClientTransport>;

} // namespace mcp::client
