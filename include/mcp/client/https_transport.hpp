#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <string>

#include <mcp/client/transport.hpp>

namespace mcp::client
{

    /**
     * @brief Configuration for the HTTP(S) client transport.
     */
    struct HttpsTransportOptions
    {
        std::string url;
        std::map<std::string, std::string> headers;
        std::chrono::milliseconds connect_timeout{5000};
        std::chrono::milliseconds request_timeout{30000};
        std::string user_agent = "mcp_cpp/0.1.0";
        bool verify_peer = true;
    };

    /**
     * @brief Synchronous HTTP(S) transport backed by libcurl.
     *
     * Each outbound MCP message is sent as one HTTP POST request to the
     * configured endpoint. When the endpoint returns a response body, JSON
     * responses are forwarded directly to the registered message handler and
     * `text/event-stream` responses are decoded into individual MCP messages.
     *
     * The transport accepts both `https://` and `http://` URLs so local tests can
     * run without TLS infrastructure, while production use can target HTTPS.
     */
    class HttpsTransport final : public IClientTransport
    {
    public:
        explicit HttpsTransport(HttpsTransportOptions options);
        ~HttpsTransport() override;

        HttpsTransport(const HttpsTransport &) = delete;
        HttpsTransport &operator=(const HttpsTransport &) = delete;
        HttpsTransport(HttpsTransport &&) noexcept;
        HttpsTransport &operator=(HttpsTransport &&) noexcept;

        [[nodiscard]] const HttpsTransportOptions &options() const noexcept;

        void connect() override;
        void close() override;
        void send(const JsonString &message) override;
        void set_message_handler(MessageHandler handler) override;
        void set_error_handler(ErrorHandler handler) override;
        void set_close_handler(CloseHandler handler) override;

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace mcp::client
