#include <mcp/server.hpp>
#include <mcp/server/transport.hpp>
#include <mcp/logging.hpp>

#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace
{

    class ConsoleServerTransport final : public mcp::server::IServerTransport
    {
    public:
        void listen() override
        {
            mcp::log_output << "[transport] listen" << std::endl;
        }

        void close() override
        {
            mcp::log_output << "[transport] close" << std::endl;

            if (close_handler_)
            {
                close_handler_();
            }
        }

        void send(const mcp::server::JsonString &message) override
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

        void simulate_request(const std::string &message)
        {
            if (message_handler_)
            {
                message_handler_(message);
            }
        }

    private:
        MessageHandler message_handler_;
        ErrorHandler error_handler_;
        CloseHandler close_handler_;
    };

} // namespace

int main()
{
    mcp::server::ServerOptions options;
    options.server_info.name = "mcp_cpp_server_sample";
    options.server_info.version = "0.1.0";
    options.server_info.instructions = "Call tools/list or tools/call to inspect the sample server.";
    options.callbacks.log = [](mcp::server::LogLevel, std::string_view message)
    {
        mcp::log_output << "[server] " << message << std::endl;
    };
    options.callbacks.state_changed = [](mcp::server::ServerState state)
    {
        mcp::log_output << "[server] state changed to " << static_cast<int>(state) << std::endl;
    };

    auto transport = std::make_shared<ConsoleServerTransport>();

    mcp::server::Server server{options};
    server.set_transport(transport);
    server.register_tool({"echo", "Echo", "Returns a simple acknowledgement.", R"({"type":"object"})", {}});
    server.set_request_handler("tools/call", [](const mcp::server::RequestContext &context)
                               { return std::string{"{"} +
                                        "\"ok\":true,\"method\":\"" + context.method + "\"}"; });

    server.start();

    transport->simulate_request(R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");
    transport->simulate_request(R"({"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}})");
    transport->simulate_request(R"({"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"echo"}})");

    server.notify("notifications/ready", R"({"message":"server sample is running"})");
    server.stop();

    return 0;
}
