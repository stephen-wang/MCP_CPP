#include <mcp/server.hpp>
#include <mcp/server/transport.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

    class RecordingServerTransport final : public mcp::server::IServerTransport
    {
    public:
        void listen() override
        {
            listening = true;
        }

        void close() override
        {
            listening = false;

            if (close_handler_)
            {
                close_handler_();
            }
        }

        void send(const mcp::server::JsonString &message) override
        {
            outbound_messages.push_back(message);
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

        void emit_inbound(const std::string &message)
        {
            if (message_handler_)
            {
                message_handler_(message);
            }
        }

        bool listening = false;
        std::vector<std::string> outbound_messages;

    private:
        MessageHandler message_handler_;
        ErrorHandler error_handler_;
        CloseHandler close_handler_;
    };

    void expect(bool condition, const char *message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

} // namespace

int main()
{
    try
    {
        std::cout << "[test] starting server smoke test" << std::endl;

        mcp::server::ServerOptions options;
        options.server_info.name = "mcp_cpp_test_server";
        options.server_info.version = "0.1.0";
        options.server_info.instructions = "Use the test server.";

        auto transport = std::make_shared<RecordingServerTransport>();

        mcp::server::Server server{options};
        server.set_transport(transport);
        server.register_tool({"echo", {}, "Echo tool", R"({"type":"object"})", {}});
        server.set_request_handler("tools/call", [](const mcp::server::RequestContext &context)
                                   { return std::string{"{"} +
                                            "\"handled\":true,\"method\":\"" + context.method + "\"}"; });

        server.start();
        expect(server.is_running(), "server should be running after start");
        expect(transport->listening, "transport should be listening");
        std::cout << "[test] server started" << std::endl;

        transport->emit_inbound(R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");
        expect(!transport->outbound_messages.empty(), "initialize response should be sent");
        expect(transport->outbound_messages.back().find("\"serverInfo\"") != std::string::npos,
               "initialize response should include server info");
        std::cout << "[test] initialize response verified" << std::endl;

        transport->emit_inbound(R"({"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}})");
        expect(transport->outbound_messages.back().find("\"echo\"") != std::string::npos,
               "tools/list response should include registered tool");
        std::cout << "[test] tools list verified" << std::endl;

        transport->emit_inbound(R"({"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"echo"}})");
        expect(transport->outbound_messages.back().find("\"handled\":true") != std::string::npos,
               "custom handler response should be sent");
        std::cout << "[test] custom handler verified" << std::endl;

        server.notify("notifications/ready", R"({"ok":true})");
        expect(transport->outbound_messages.back().find("\"method\":\"notifications/ready\"") != std::string::npos,
               "notification should be sent");
        std::cout << "[test] outbound notification verified" << std::endl;

        server.stop();
        expect(server.state() == mcp::server::ServerState::stopped,
               "server should be stopped after stop");
        std::cout << "[test] stop verified" << std::endl;
        std::cout << "[test] server smoke test passed" << std::endl;

        return EXIT_SUCCESS;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "[test] server smoke test failed: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
