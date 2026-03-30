#include <mcp/client/stdio_transport.hpp>
#include <mcp/server/stdio_transport.hpp>

#include <chrono>
#include <cstdlib>
#include <future>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{

    void expect(bool condition, const char *message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    template <typename TFuture>
    void expect_ready(TFuture &future, const char *message)
    {
        if (future.wait_for(std::chrono::seconds{1}) != std::future_status::ready)
        {
            throw std::runtime_error(message);
        }
    }

    void run_client_stdio_transport_test()
    {
        std::istringstream inbound(
            R"({"jsonrpc":"2.0","method":"notifications/test","params":{"ok":true}})");
        std::ostringstream outbound;

        mcp::client::StdioTransport transport{inbound, outbound};

        std::promise<std::string> message_promise;
        auto message_future = message_promise.get_future();
        std::promise<void> close_promise;
        auto close_future = close_promise.get_future();

        transport.set_message_handler([&message_promise](const std::string &message)
                                      { message_promise.set_value(message); });
        transport.set_error_handler([](const std::string &) {});
        transport.set_close_handler([&close_promise]()
                                    { close_promise.set_value(); });

        transport.connect();
        transport.send(R"({"jsonrpc":"2.0","method":"client/ping","params":{}})");

        expect_ready(message_future, "client stdio transport should receive inbound message");
        const auto message = message_future.get();
        expect(message.find("\"notifications/test\"") != std::string::npos,
               "client stdio transport should deliver the inbound line");
        expect(outbound.str().find("\"client/ping\"") != std::string::npos,
               "client stdio transport should write outbound message");
        expect(!outbound.str().empty() && outbound.str().back() == '\n',
               "client stdio transport should newline-frame outbound messages");

        transport.close();
        expect_ready(close_future, "client stdio transport should invoke close handler");
    }

    void run_server_stdio_transport_test()
    {
        std::istringstream inbound(
            R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");
        std::ostringstream outbound;

        mcp::server::StdioTransport transport{inbound, outbound};

        std::promise<std::string> message_promise;
        auto message_future = message_promise.get_future();
        std::promise<void> close_promise;
        auto close_future = close_promise.get_future();

        transport.set_message_handler([&message_promise](const std::string &message)
                                      { message_promise.set_value(message); });
        transport.set_error_handler([](const std::string &) {});
        transport.set_close_handler([&close_promise]()
                                    { close_promise.set_value(); });

        transport.listen();
        transport.send(R"({"jsonrpc":"2.0","result":{"ok":true}})");

        expect_ready(message_future, "server stdio transport should receive inbound message");
        const auto message = message_future.get();
        expect(message.find("\"initialize\"") != std::string::npos,
               "server stdio transport should deliver the inbound line");
        expect(outbound.str().find("\"ok\":true") != std::string::npos,
               "server stdio transport should write outbound message");
        expect(!outbound.str().empty() && outbound.str().back() == '\n',
               "server stdio transport should newline-frame outbound messages");

        transport.close();
        expect_ready(close_future, "server stdio transport should invoke close handler");
    }

} // namespace

int main()
{
    try
    {
        std::cout << "[test] starting stdio transport smoke test" << std::endl;
        run_client_stdio_transport_test();
        std::cout << "[test] client stdio transport verified" << std::endl;
        run_server_stdio_transport_test();
        std::cout << "[test] server stdio transport verified" << std::endl;
        std::cout << "[test] stdio transport smoke test passed" << std::endl;
        return EXIT_SUCCESS;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "[test] stdio transport smoke test failed: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
