#include <mcp/server.hpp>
#include <mcp/server/streamable_http_transport.hpp>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
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

#if !defined(_WIN32)
    class TcpClient
    {
    public:
        explicit TcpClient(unsigned short port)
        {
            fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
            if (fd_ < 0)
            {
                throw std::runtime_error("failed to create client socket");
            }

            timeval timeout{};
            timeout.tv_sec = 2;
            timeout.tv_usec = 0;
            ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            ::setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_port = htons(port);
            if (::inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) != 1)
            {
                ::close(fd_);
                throw std::runtime_error("failed to encode loopback address");
            }

            if (::connect(fd_, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0)
            {
                ::close(fd_);
                throw std::runtime_error("failed to connect to HTTP server transport");
            }
        }

        ~TcpClient()
        {
            if (fd_ >= 0)
            {
                ::close(fd_);
            }
        }

        void send(std::string_view payload)
        {
            std::size_t offset = 0;
            while (offset < payload.size())
            {
                const auto written = ::write(fd_, payload.data() + offset, payload.size() - offset);
                if (written <= 0)
                {
                    throw std::runtime_error("failed to send HTTP client payload");
                }
                offset += static_cast<std::size_t>(written);
            }
        }

        [[nodiscard]] std::string receive_once() const
        {
            char buffer[4096];
            const auto received = ::recv(fd_, buffer, sizeof(buffer), 0);
            if (received <= 0)
            {
                return {};
            }
            return std::string{buffer, static_cast<std::size_t>(received)};
        }

        [[nodiscard]] std::string receive_until_contains(std::string_view needle) const
        {
            std::string data;
            while (data.find(needle) == std::string::npos)
            {
                const auto chunk = receive_once();
                if (chunk.empty())
                {
                    break;
                }

                data += chunk;
            }

            return data;
        }

    private:
        int fd_ = -1;
    };

    std::string build_post_request(std::string_view accept_header, std::string_view body)
    {
        std::string request;
        request += "POST /mcp HTTP/1.1\r\n";
        request += "Host: 127.0.0.1\r\n";
        request += "Content-Type: application/json\r\n";
        request += "Accept: ";
        request += accept_header;
        request += "\r\n";
        request += "Content-Length: ";
        request += std::to_string(body.size());
        request += "\r\n\r\n";
        request += body;
        return request;
    }

#endif

} // namespace

int main()
{
#if defined(_WIN32)
    std::cout << "[test] server streamable HTTP transport smoke test skipped on Windows" << std::endl;
    return EXIT_SUCCESS;
#else
    try
    {
        std::cout << "[test] starting server streamable HTTP transport smoke test" << std::endl;

        auto transport = std::make_shared<mcp::server::StreamableHttpTransport>();

        mcp::server::ServerOptions options;
        options.server_info.name = "mcp_cpp_http_server";
        options.server_info.version = "0.1.0";

        mcp::server::Server server{options};
        server.set_transport(transport);
        server.start();

        TcpClient json_client{transport->port()};
        json_client.send(build_post_request("application/json", R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})"));
        const auto json_response = json_client.receive_once();
        expect(json_response.find("HTTP/1.1 200 OK") != std::string::npos,
               "JSON client should receive HTTP 200 response");
        expect(json_response.find("Content-Type: application/json") != std::string::npos,
               "JSON client should receive JSON content type");
        expect(json_response.find("\"serverInfo\"") != std::string::npos,
               "JSON client should receive initialize result");

        TcpClient stream_client{transport->port()};
        stream_client.send(build_post_request("text/event-stream", R"({"jsonrpc":"2.0","id":2,"method":"initialize","params":{}})"));
        const auto initial_stream = stream_client.receive_until_contains("\"serverInfo\"");
        expect(initial_stream.find("Content-Type: text/event-stream") != std::string::npos,
               "stream client should receive SSE headers");
        expect(initial_stream.find("\"serverInfo\"") != std::string::npos,
               "stream client should receive initialize result as SSE event");

        server.notify("notifications/ready", R"({"ok":true})");
        const auto pushed_stream = stream_client.receive_until_contains("\"method\":\"notifications/ready\"");
        expect(pushed_stream.find("event: message") != std::string::npos,
               "stream client should receive SSE event framing");
        expect(pushed_stream.find("\"method\":\"notifications/ready\"") != std::string::npos,
               "stream client should receive pushed notification");

        server.stop();
        std::cout << "[test] server streamable HTTP transport smoke test passed" << std::endl;
        return EXIT_SUCCESS;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "[test] server streamable HTTP transport smoke test failed: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
#endif
}
