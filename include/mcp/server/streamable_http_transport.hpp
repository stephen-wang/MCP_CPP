#pragma once

#include <memory>
#include <string>

#include <mcp/server/transport.hpp>

namespace mcp::server
{

    /**
     * @brief Configuration for the streamable HTTP server transport.
     */
    struct StreamableHttpTransportOptions
    {
        std::string bind_address = "127.0.0.1";
        unsigned short port = 0;
        std::string path = "/mcp";
        int backlog = 8;
    };

    /**
     * @brief Streamable HTTP transport for MCP servers.
     *
     * Plain JSON POST requests receive one JSON HTTP response.
     * Requests that explicitly prefer `text/event-stream` receive SSE-framed MCP
     * messages and can continue receiving later notifications through the same
     * stream.
     */
    class StreamableHttpTransport final : public IServerTransport
    {
    public:
        explicit StreamableHttpTransport(StreamableHttpTransportOptions options = {});
        ~StreamableHttpTransport() override;

        StreamableHttpTransport(const StreamableHttpTransport &) = delete;
        StreamableHttpTransport &operator=(const StreamableHttpTransport &) = delete;
        StreamableHttpTransport(StreamableHttpTransport &&) noexcept;
        StreamableHttpTransport &operator=(StreamableHttpTransport &&) noexcept;

        [[nodiscard]] const StreamableHttpTransportOptions &options() const noexcept;
        [[nodiscard]] unsigned short port() const noexcept;

        void listen() override;
        void close() override;
        void send(const JsonString &message) override;
        void set_message_handler(MessageHandler handler) override;
        void set_error_handler(ErrorHandler handler) override;
        void set_close_handler(CloseHandler handler) override;

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace mcp::server
