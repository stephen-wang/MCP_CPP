#include <mcp/server/streamable_http_transport.hpp>
#include <mcp/detail/http_util.hpp>

#include <cerrno>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace mcp::server
{
    namespace
    {
#if !defined(_WIN32)
        void close_socket(int fd)
        {
            if (fd >= 0)
            {
                ::close(fd);
            }
        }

        [[nodiscard]] bool write_all(int fd, std::string_view data)
        {
            std::size_t offset = 0;
            while (offset < data.size())
            {
                const auto written = ::send(fd, data.data() + offset, data.size() - offset, 0);
                if (written <= 0)
                {
                    return false;
                }

                offset += static_cast<std::size_t>(written);
            }

            return true;
        }

        struct HttpRequest
        {
            std::string method;
            std::string path;
            std::unordered_map<std::string, std::string> headers;
            std::string body;
        };

        [[nodiscard]] std::string get_header_value(const HttpRequest &request, std::string_view name)
        {
            const auto it = request.headers.find(mcp::detail::to_lower_ascii(name));
            return it == request.headers.end() ? std::string{} : it->second;
        }

        [[nodiscard]] bool prefers_event_stream(const HttpRequest &request)
        {
            const auto accept = mcp::detail::to_lower_ascii(get_header_value(request, "accept"));
            return accept.find("text/event-stream") != std::string::npos &&
                   accept.find("application/json") == std::string::npos;
        }

        [[nodiscard]] bool read_http_request(int fd, HttpRequest &request)
        {
            std::string raw;
            std::size_t header_end = std::string::npos;
            std::size_t content_length = 0;

            while (true)
            {
                char buffer[1024];
                const auto received = ::recv(fd, buffer, sizeof(buffer), 0);
                if (received <= 0)
                {
                    return false;
                }

                raw.append(buffer, static_cast<std::size_t>(received));
                header_end = raw.find("\r\n\r\n");
                if (header_end == std::string::npos)
                {
                    continue;
                }

                const auto header_block = raw.substr(0, header_end);
                const auto request_line_end = header_block.find("\r\n");
                if (request_line_end == std::string::npos)
                {
                    return false;
                }

                const auto request_line = header_block.substr(0, request_line_end);
                const auto first_space = request_line.find(' ');
                const auto second_space = request_line.find(' ', first_space == std::string::npos ? first_space : first_space + 1);
                if (first_space == std::string::npos || second_space == std::string::npos)
                {
                    return false;
                }

                request.method = request_line.substr(0, first_space);
                request.path = request_line.substr(first_space + 1, second_space - first_space - 1);

                std::size_t line_start = request_line_end + 2;
                while (line_start < header_block.size())
                {
                    const auto line_end = header_block.find("\r\n", line_start);
                    const auto line = header_block.substr(line_start, line_end - line_start);
                    const auto separator = line.find(':');
                    if (separator != std::string::npos)
                    {
                        request.headers.emplace(
                            mcp::detail::to_lower_ascii(mcp::detail::trim_ascii_whitespace(line.substr(0, separator))),
                            mcp::detail::trim_ascii_whitespace(line.substr(separator + 1)));
                    }

                    if (line_end == std::string::npos)
                    {
                        break;
                    }
                    line_start = line_end + 2;
                }

                const auto content_length_value = get_header_value(request, "content-length");
                if (!content_length_value.empty())
                {
                    content_length = static_cast<std::size_t>(std::stoul(content_length_value));
                }

                const auto body_start = header_end + 4;
                if (raw.size() - body_start >= content_length)
                {
                    request.body = raw.substr(body_start, content_length);
                    return true;
                }
            }
        }

        [[nodiscard]] std::string build_http_json_response(std::string_view body)
        {
            std::string response;
            response += "HTTP/1.1 200 OK\r\n";
            response += "Content-Type: application/json\r\n";
            response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
            response += "Connection: close\r\n\r\n";
            response += body;
            return response;
        }

        [[nodiscard]] std::string build_http_sse_headers()
        {
            return "HTTP/1.1 200 OK\r\n"
                   "Content-Type: text/event-stream\r\n"
                   "Cache-Control: no-cache\r\n"
                   "Connection: keep-alive\r\n\r\n";
        }

        [[nodiscard]] std::string build_sse_message(std::string_view message)
        {
            return std::string{"event: message\r\ndata: "} + std::string{message} + "\r\n\r\n";
        }

        [[nodiscard]] std::string build_http_error_response(int status_code, std::string_view status_text)
        {
            const std::string body = std::string{status_text};
            std::string response;
            response += "HTTP/1.1 " + std::to_string(status_code) + " " + std::string{status_text} + "\r\n";
            response += "Content-Type: text/plain\r\n";
            response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
            response += "Connection: close\r\n\r\n";
            response += body;
            return response;
        }
#endif
    } // namespace

    class StreamableHttpTransport::Impl
    {
    public:
        explicit Impl(StreamableHttpTransportOptions options)
            : options_(std::move(options))
        {
        }

        ~Impl()
        {
            close();
        }

        [[nodiscard]] const StreamableHttpTransportOptions &options() const noexcept
        {
            return options_;
        }

        [[nodiscard]] unsigned short port() const noexcept
        {
            return port_;
        }

        void listen()
        {
#if defined(_WIN32)
            throw std::runtime_error("streamable HTTP transport is not implemented on Windows");
#else
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (active_)
            {
                return;
            }

            listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
            if (listen_fd_ < 0)
            {
                throw std::runtime_error("failed to create HTTP server socket");
            }

            int enable = 1;
            ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_port = htons(options_.port);
            if (::inet_pton(AF_INET, options_.bind_address.c_str(), &address.sin_addr) != 1)
            {
                close_socket(listen_fd_);
                listen_fd_ = -1;
                throw std::invalid_argument("invalid bind address for streamable HTTP transport");
            }

            if (::bind(listen_fd_, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0)
            {
                const auto error = std::string{"failed to bind HTTP server socket: "} + std::strerror(errno);
                close_socket(listen_fd_);
                listen_fd_ = -1;
                throw std::runtime_error(error);
            }

            socklen_t address_length = sizeof(address);
            if (::getsockname(listen_fd_, reinterpret_cast<sockaddr *>(&address), &address_length) != 0)
            {
                close_socket(listen_fd_);
                listen_fd_ = -1;
                throw std::runtime_error("failed to query bound HTTP server port");
            }
            port_ = ntohs(address.sin_port);

            if (::listen(listen_fd_, options_.backlog) != 0)
            {
                const auto error = std::string{"failed to listen on HTTP server socket: "} + std::strerror(errno);
                close_socket(listen_fd_);
                listen_fd_ = -1;
                throw std::runtime_error(error);
            }

            active_ = true;
            close_notified_ = false;
            accept_thread_ = std::thread([this]()
                                         { accept_loop(); });
#endif
        }

        void close()
        {
#if defined(_WIN32)
            CloseHandler close_handler;
            {
                std::lock_guard<std::mutex> lock(handler_mutex_);
                close_handler = close_handler_;
            }
            if (close_handler)
            {
                close_handler();
            }
#else
            CloseHandler close_handler;
            std::thread accept_thread;
            int listen_fd = -1;
            int stream_fd = -1;

            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                if (!active_ && close_notified_)
                {
                    return;
                }

                active_ = false;
                listen_fd = listen_fd_;
                listen_fd_ = -1;
                if (accept_thread_.joinable())
                {
                    accept_thread = std::move(accept_thread_);
                }

                if (stream_connection_)
                {
                    stream_fd = stream_connection_->socket_fd;
                    stream_connection_.reset();
                }

                if (close_notified_)
                {
                    close_socket(listen_fd);
                    close_socket(stream_fd);
                    if (accept_thread.joinable())
                    {
                        accept_thread.join();
                    }
                    return;
                }

                close_notified_ = true;
            }

            close_socket(listen_fd);
            close_socket(stream_fd);
            if (accept_thread.joinable())
            {
                accept_thread.join();
            }

            {
                std::lock_guard<std::mutex> lock(handler_mutex_);
                close_handler = close_handler_;
            }

            if (close_handler)
            {
                close_handler();
            }
#endif
        }

        void send(const JsonString &message)
        {
#if defined(_WIN32)
            (void)message;
            throw std::runtime_error("streamable HTTP transport is not implemented on Windows");
#else
            auto current_request = current_request_context_;
            if (current_request)
            {
                send_to_request(*current_request, message);
                return;
            }

            std::shared_ptr<ConnectionContext> stream_connection;
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                stream_connection = stream_connection_;
            }

            if (!stream_connection)
            {
                throw std::logic_error("streamable HTTP transport has no active response context");
            }

            send_sse_message(*stream_connection, message);
#endif
        }

        void set_message_handler(MessageHandler handler)
        {
            std::lock_guard<std::mutex> lock(handler_mutex_);
            message_handler_ = std::move(handler);
        }

        void set_error_handler(ErrorHandler handler)
        {
            std::lock_guard<std::mutex> lock(handler_mutex_);
            error_handler_ = std::move(handler);
        }

        void set_close_handler(CloseHandler handler)
        {
            std::lock_guard<std::mutex> lock(handler_mutex_);
            close_handler_ = std::move(handler);
        }

    private:
#if !defined(_WIN32)
        struct ConnectionContext
        {
            int socket_fd = -1;
            bool streaming = false;
            bool response_started = false;
            bool closed = false;
            std::mutex write_mutex;
        };

        static thread_local std::shared_ptr<ConnectionContext> current_request_context_;

        void accept_loop()
        {
            while (true)
            {
                int client_fd = -1;
                {
                    std::lock_guard<std::mutex> lock(state_mutex_);
                    if (!active_ || listen_fd_ < 0)
                    {
                        return;
                    }
                }

                client_fd = ::accept(listen_fd_, nullptr, nullptr);
                if (client_fd < 0)
                {
                    std::lock_guard<std::mutex> lock(state_mutex_);
                    if (!active_)
                    {
                        return;
                    }

                    emit_error(std::string{"streamable HTTP accept failed: "} + std::strerror(errno));
                    continue;
                }

                std::thread([this, client_fd]()
                            { handle_client(client_fd); })
                    .detach();
            }
        }

        void handle_client(int client_fd)
        {
            HttpRequest request;
            if (!read_http_request(client_fd, request))
            {
                close_socket(client_fd);
                return;
            }

            if (request.method != "POST")
            {
                (void)write_all(client_fd, build_http_error_response(405, "Method Not Allowed"));
                close_socket(client_fd);
                return;
            }

            if (request.path != options_.path)
            {
                (void)write_all(client_fd, build_http_error_response(404, "Not Found"));
                close_socket(client_fd);
                return;
            }

            auto connection = std::make_shared<ConnectionContext>();
            connection->socket_fd = client_fd;
            connection->streaming = prefers_event_stream(request);

            if (connection->streaming)
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                stream_connection_ = connection;
            }

            MessageHandler message_handler;
            {
                std::lock_guard<std::mutex> lock(handler_mutex_);
                message_handler = message_handler_;
            }

            if (!message_handler)
            {
                (void)write_all(client_fd, build_http_error_response(503, "Service Unavailable"));
                close_socket(client_fd);
                return;
            }

            current_request_context_ = connection;
            try
            {
                message_handler(request.body);
            }
            catch (const std::exception &ex)
            {
                emit_error(ex.what());
            }
            catch (...)
            {
                emit_error("streamable HTTP request handler threw an unknown exception");
            }
            current_request_context_.reset();

            if (!connection->streaming)
            {
                if (!connection->closed)
                {
                    (void)write_all(client_fd, build_http_error_response(500, "Internal Server Error"));
                }
                close_socket(client_fd);
                return;
            }

            if (!connection->response_started)
            {
                (void)write_all(client_fd, build_http_sse_headers());
                connection->response_started = true;
            }
        }

        void send_to_request(ConnectionContext &connection, std::string_view message)
        {
            if (connection.streaming)
            {
                send_sse_message(connection, message);
                return;
            }

            std::lock_guard<std::mutex> lock(connection.write_mutex);
            if (connection.closed)
            {
                return;
            }

            if (!write_all(connection.socket_fd, build_http_json_response(message)))
            {
                connection.closed = true;
                close_socket(connection.socket_fd);
                emit_error("streamable HTTP transport failed to write JSON response");
                return;
            }

            connection.closed = true;
            close_socket(connection.socket_fd);
        }

        void send_sse_message(ConnectionContext &connection, std::string_view message)
        {
            std::lock_guard<std::mutex> lock(connection.write_mutex);
            if (connection.closed)
            {
                return;
            }

            if (!connection.response_started)
            {
                if (!write_all(connection.socket_fd, build_http_sse_headers()))
                {
                    connection.closed = true;
                    close_socket(connection.socket_fd);
                    emit_error("streamable HTTP transport failed to write SSE headers");
                    return;
                }
                connection.response_started = true;
            }

            if (!write_all(connection.socket_fd, build_sse_message(message)))
            {
                connection.closed = true;
                close_socket(connection.socket_fd);
                emit_error("streamable HTTP transport failed to write SSE message");
                std::lock_guard<std::mutex> state_lock(state_mutex_);
                if (stream_connection_.get() == &connection)
                {
                    stream_connection_.reset();
                }
            }
        }
#endif

        void emit_error(const std::string &message)
        {
            ErrorHandler error_handler;
            {
                std::lock_guard<std::mutex> lock(handler_mutex_);
                error_handler = error_handler_;
            }

            if (error_handler)
            {
                error_handler(message);
            }
        }

        StreamableHttpTransportOptions options_;
        MessageHandler message_handler_;
        ErrorHandler error_handler_;
        CloseHandler close_handler_;
        mutable std::mutex handler_mutex_;
        mutable std::mutex state_mutex_;
#if !defined(_WIN32)
        int listen_fd_ = -1;
        unsigned short port_ = 0;
        std::shared_ptr<ConnectionContext> stream_connection_;
        std::thread accept_thread_;
#else
        unsigned short port_ = 0;
#endif
        bool active_ = false;
        bool close_notified_ = false;
    };

#if !defined(_WIN32)
    thread_local std::shared_ptr<StreamableHttpTransport::Impl::ConnectionContext>
        StreamableHttpTransport::Impl::current_request_context_;
#endif

    StreamableHttpTransport::StreamableHttpTransport(StreamableHttpTransportOptions options)
        : impl_(std::make_unique<Impl>(std::move(options)))
    {
    }

    StreamableHttpTransport::~StreamableHttpTransport() = default;

    StreamableHttpTransport::StreamableHttpTransport(StreamableHttpTransport &&) noexcept = default;
    StreamableHttpTransport &StreamableHttpTransport::operator=(StreamableHttpTransport &&) noexcept = default;

    const StreamableHttpTransportOptions &StreamableHttpTransport::options() const noexcept
    {
        return impl_->options();
    }

    unsigned short StreamableHttpTransport::port() const noexcept
    {
        return impl_->port();
    }

    void StreamableHttpTransport::listen()
    {
        impl_->listen();
    }

    void StreamableHttpTransport::close()
    {
        impl_->close();
    }

    void StreamableHttpTransport::send(const JsonString &message)
    {
        impl_->send(message);
    }

    void StreamableHttpTransport::set_message_handler(MessageHandler handler)
    {
        impl_->set_message_handler(std::move(handler));
    }

    void StreamableHttpTransport::set_error_handler(ErrorHandler handler)
    {
        impl_->set_error_handler(std::move(handler));
    }

    void StreamableHttpTransport::set_close_handler(CloseHandler handler)
    {
        impl_->set_close_handler(std::move(handler));
    }

} // namespace mcp::server
