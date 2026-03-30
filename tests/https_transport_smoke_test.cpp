#include <mcp/client/https_transport.hpp>

#include <chrono>
#include <cstdlib>
#include <future>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

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
    auto expect_ready(TFuture &future, const char *message)
    {
        if (future.wait_for(std::chrono::seconds{2}) != std::future_status::ready)
        {
            throw std::runtime_error(message);
        }
        return future.get();
    }

#if !defined(_WIN32)
    class OneShotHttpServer
    {
    public:
        OneShotHttpServer(std::string content_type, std::string response_body)
            : content_type_(std::move(content_type)), response_body_(std::move(response_body))
        {
            listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
            if (listen_fd_ < 0)
            {
                throw std::runtime_error("failed to create test server socket");
            }

            int enable = 1;
            ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            address.sin_port = 0;

            if (::bind(listen_fd_, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0)
            {
                ::close(listen_fd_);
                throw std::runtime_error("failed to bind test server socket");
            }

            socklen_t address_length = sizeof(address);
            if (::getsockname(listen_fd_, reinterpret_cast<sockaddr *>(&address), &address_length) != 0)
            {
                ::close(listen_fd_);
                throw std::runtime_error("failed to resolve test server port");
            }

            port_ = ntohs(address.sin_port);

            if (::listen(listen_fd_, 1) != 0)
            {
                ::close(listen_fd_);
                throw std::runtime_error("failed to listen on test server socket");
            }

            worker_ = std::thread([this]()
                                  { serve(); });
        }

        ~OneShotHttpServer()
        {
            if (listen_fd_ >= 0)
            {
                ::close(listen_fd_);
            }

            if (worker_.joinable())
            {
                worker_.join();
            }
        }

        [[nodiscard]] unsigned short port() const noexcept
        {
            return port_;
        }

        [[nodiscard]] std::future<std::string> request_future()
        {
            return request_promise_.get_future();
        }

    private:
        void serve()
        {
            const int client_fd = ::accept(listen_fd_, nullptr, nullptr);
            if (client_fd < 0)
            {
                request_promise_.set_value({});
                return;
            }

            std::string request = read_request(client_fd);
            request_promise_.set_value(request);

            const std::string response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: " +
                content_type_ + "\r\n"
                                "Content-Length: " +
                std::to_string(response_body_.size()) + "\r\n"
                                                        "Connection: close\r\n\r\n" +
                response_body_;

            ::send(client_fd, response.data(), response.size(), 0);
            ::close(client_fd);
        }

        static std::string read_request(int client_fd)
        {
            std::string request;
            std::size_t header_end = std::string::npos;
            std::size_t content_length = 0;

            while (true)
            {
                char buffer[1024];
                const auto received = ::recv(client_fd, buffer, sizeof(buffer), 0);
                if (received <= 0)
                {
                    break;
                }

                request.append(buffer, static_cast<std::size_t>(received));
                header_end = request.find("\r\n\r\n");
                if (header_end != std::string::npos)
                {
                    const auto header_block = request.substr(0, header_end);
                    const auto content_length_pos = header_block.find("Content-Length:");
                    if (content_length_pos != std::string::npos)
                    {
                        const auto line_end = header_block.find("\r\n", content_length_pos);
                        const auto value = header_block.substr(
                            content_length_pos + std::string{"Content-Length:"}.size(),
                            line_end - content_length_pos - std::string{"Content-Length:"}.size());
                        content_length = static_cast<std::size_t>(std::stoul(value));
                    }

                    const auto body_size = request.size() - (header_end + 4);
                    if (body_size >= content_length)
                    {
                        break;
                    }
                }
            }

            return request;
        }

        int listen_fd_ = -1;
        unsigned short port_ = 0;
        std::promise<std::string> request_promise_;
        std::string content_type_;
        std::string response_body_;
        std::thread worker_;
    };

    void run_json_response_test()
    {
        OneShotHttpServer server{"application/json; charset=utf-8",
                                 R"({"jsonrpc":"2.0","result":{"ok":true}})"};
        auto request_future = server.request_future();

        mcp::client::HttpsTransportOptions options;
        options.url = "http://127.0.0.1:" + std::to_string(server.port()) + "/mcp";
        options.connect_timeout = std::chrono::milliseconds{1000};
        options.request_timeout = std::chrono::milliseconds{2000};
        options.verify_peer = false;
        options.headers.emplace("X-Test-Header", "mcp_cpp");

        mcp::client::HttpsTransport transport{options};

        std::promise<std::string> response_promise;
        auto response_future = response_promise.get_future();
        std::promise<void> close_promise;
        auto close_future = close_promise.get_future();

        transport.set_message_handler([&response_promise](const std::string &message)
                                      { response_promise.set_value(message); });
        transport.set_error_handler([](const std::string &message)
                                    { throw std::runtime_error(message); });
        transport.set_close_handler([&close_promise]()
                                    { close_promise.set_value(); });

        transport.connect();
        transport.send(R"({"jsonrpc":"2.0","method":"initialize","params":{}})");
        transport.close();

        const auto request = expect_ready(request_future, "expected HTTP request from transport");
        const auto response = expect_ready(response_future, "expected HTTP response callback");
        expect_ready(close_future, "expected close callback");

        expect(request.find("POST /mcp HTTP/1.1") != std::string::npos,
               "transport should send POST request to MCP path");
        expect(request.find("X-Test-Header: mcp_cpp") != std::string::npos,
               "transport should include configured headers");
        expect(request.find("Accept: application/json, text/event-stream") != std::string::npos,
               "transport should advertise JSON and event-stream support");
        expect(request.find("\"method\":\"initialize\"") != std::string::npos,
               "transport should send the JSON request body");
        expect(response.find("\"ok\":true") != std::string::npos,
               "transport should forward the HTTP response body");
    }

    void run_event_stream_response_test()
    {
        OneShotHttpServer server{
            "text/event-stream; charset=utf-8",
            "event: message\r\ndata: {\"jsonrpc\":\"2.0\",\"result\":{\"step\":1}}\r\n\r\n"
            "data: {\"jsonrpc\":\"2.0\",\"result\":{\"step\":2}}\r\n\r\n"};
        auto request_future = server.request_future();

        mcp::client::HttpsTransportOptions options;
        options.url = "http://127.0.0.1:" + std::to_string(server.port()) + "/mcp";
        options.connect_timeout = std::chrono::milliseconds{1000};
        options.request_timeout = std::chrono::milliseconds{2000};
        options.verify_peer = false;

        mcp::client::HttpsTransport transport{options};

        std::mutex messages_mutex;
        std::vector<std::string> messages;
        std::promise<void> messages_promise;
        auto messages_future = messages_promise.get_future();

        transport.set_message_handler([&](const std::string &message)
                                      {
                                          std::lock_guard<std::mutex> lock(messages_mutex);
                                          messages.push_back(message);
                                          if (messages.size() == 2)
                                          {
                                              messages_promise.set_value();
                                          } });
        transport.set_error_handler([](const std::string &message)
                                    { throw std::runtime_error(message); });
        transport.set_close_handler([]() {});

        transport.connect();
        transport.send(R"({"jsonrpc":"2.0","method":"tools/list","params":{}})");
        transport.close();

        expect_ready(request_future, "expected HTTP request for event-stream transport");
        expect_ready(messages_future, "expected event-stream messages from transport");

        expect(messages.size() == 2, "transport should emit one callback per SSE data event");
        expect(messages[0].find("\"step\":1") != std::string::npos,
               "transport should decode the first SSE event");
        expect(messages[1].find("\"step\":2") != std::string::npos,
               "transport should decode the second SSE event");
    }
#endif

} // namespace

int main()
{
    try
    {
#if defined(_WIN32)
        std::cout << "[test] https transport smoke test skipped on Windows" << std::endl;
        return EXIT_SUCCESS;
#else
        std::cout << "[test] starting https transport smoke test" << std::endl;

        run_json_response_test();
        run_event_stream_response_test();

        std::cout << "[test] https transport smoke test passed" << std::endl;
        return EXIT_SUCCESS;
#endif
    }
    catch (const std::exception &ex)
    {
        std::cerr << "[test] https transport smoke test failed: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
