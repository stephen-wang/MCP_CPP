#pragma once

#include <iosfwd>

#include <mcp/client/transport.hpp>
#include <mcp/detail/stdio_transport_base.hpp>

namespace mcp::client
{

    /**
     * @brief Basic line-delimited stdio transport for MCP clients.
     *
     * Outbound messages are written to the configured output stream followed by a
     * newline. Inbound messages are consumed one line at a time from the input
     * stream on a background reader thread.
     *
     * When this transport is bound to `stdin` / `stdout`, application logging
     * should go to `mcp::log_output` or another sink so protocol frames on
     * `stdout` are not corrupted.
     */
    class StdioTransport final : public mcp::detail::StdioTransportBase<IClientTransport>
    {
    public:
        using mcp::detail::StdioTransportBase<IClientTransport>::StdioTransportBase;

        ~StdioTransport() override = default;
        StdioTransport(StdioTransport &&) noexcept = default;
        StdioTransport &operator=(StdioTransport &&) noexcept = default;

        void connect() override;
    };

} // namespace mcp::client
