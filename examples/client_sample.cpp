#include <mcp/client.hpp>
#include <mcp/client/transport.hpp>
#include <mcp/logging.hpp>

#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace
{

    class ConsoleTransport final : public mcp::client::IClientTransport
    {
    public:
        void connect() override
        {
            mcp::log_output << "[transport] connect" << std::endl;
        }

        void close() override
        {
            mcp::log_output << "[transport] close" << std::endl;

            if (close_handler_)
            {
                close_handler_();
            }
        }

        void send(const mcp::client::JsonString &message) override
        {
            mcp::log_output << "[transport] send: " << message << std::endl;
        }

        void set_message_handler(MessageHandler handler) override
        {
            message_handler_ = std::move(handler);
        }

        void set_error_handler(ErrorHandler handler) override
        {
            error_handler_ = std::move(handler);
        }

        void set_close_handler(CloseHandler handler) override
        {
            close_handler_ = std::move(handler);
        }

    private:
        MessageHandler message_handler_;
        ErrorHandler error_handler_;
        CloseHandler close_handler_;
    };

} // namespace

int main()
{
    mcp::client::ClientOptions options;
    options.initialize.protocol_version = "2025-03-26";
    options.initialize.client_info.name = "mcp_cpp_sample";
    options.initialize.client_info.version = "0.1.0";
    options.callbacks.log = [](mcp::client::LogLevel, std::string_view message)
    {
        mcp::log_output << "[client] " << message << std::endl;
    };
    options.callbacks.state_changed = [](mcp::client::ConnectionState state)
    {
        mcp::log_output << "[client] state changed to " << static_cast<int>(state) << std::endl;
    };

    auto transport = std::make_shared<ConsoleTransport>();

    mcp::client::Client client{options};
    client.set_transport(transport);
    client.connect();
    client.notify("notifications/ping", R"({"message":"hello from sample"})");
    client.disconnect();

    return 0;
}
