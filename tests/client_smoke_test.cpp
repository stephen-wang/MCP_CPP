#include <mcp/client.hpp>
#include <mcp/client/transport.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

    class RecordingTransport final : public mcp::client::IClientTransport
    {
    public:
        void connect() override
        {
            connected = true;
        }

        void close() override
        {
            connected = false;

            if (close_handler_)
            {
                close_handler_();
            }
        }

        void send(const mcp::client::JsonString &message) override
        {
            messages.push_back(message);
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

        bool connected = false;
        std::vector<std::string> messages;

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
        std::cout << "[test] starting client smoke test" << std::endl;

        mcp::client::ClientOptions options;
        options.initialize.protocol_version = "2025-03-26";
        options.initialize.client_info.name = "mcp_cpp_test";
        options.initialize.client_info.version = "0.1.0";

        auto transport = std::make_shared<RecordingTransport>();

        mcp::client::Client client{options};
        client.set_transport(transport);
        client.connect();
        std::cout << "[test] client connected" << std::endl;

        expect(transport->connected, "transport should be connected");
        expect(client.is_ready(), "client should be ready after connect");
        expect(!transport->messages.empty(), "initialize request should be sent");
        expect(transport->messages.front().find("\"method\":\"initialize\"") != std::string::npos,
               "first outbound message should be initialize");
        std::cout << "[test] initialize request verified" << std::endl;

        client.notify("notifications/test", R"({"ok":true})");
        expect(transport->messages.size() >= 2, "notification should be sent");
        std::cout << "[test] notification verified" << std::endl;

        const auto response = client.request("tools/list", "{}");
        expect(response.error.has_value(), "placeholder response should carry an error message");
        std::cout << "[test] placeholder response verified" << std::endl;

        client.disconnect();
        expect(client.state() == mcp::client::ConnectionState::disconnected,
               "client should be disconnected after disconnect");
        std::cout << "[test] disconnect verified" << std::endl;
        std::cout << "[test] client smoke test passed" << std::endl;

        return EXIT_SUCCESS;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "[test] client smoke test failed: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
