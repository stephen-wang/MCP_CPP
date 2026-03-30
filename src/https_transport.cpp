#include <mcp/client/https_transport.hpp>
#include <mcp/detail/http_util.hpp>

#include <curl/curl.h>

#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace mcp::client
{
    namespace
    {

        void ensure_curl_initialized()
        {
            static std::once_flag init_flag;
            static CURLcode init_result = CURLE_OK;

            std::call_once(init_flag, []()
                           { init_result = curl_global_init(CURL_GLOBAL_DEFAULT); });

            if (init_result != CURLE_OK)
            {
                throw std::runtime_error("failed to initialize libcurl");
            }
        }

        [[nodiscard]] bool has_supported_scheme(std::string_view url)
        {
            return url.rfind("https://", 0) == 0 || url.rfind("http://", 0) == 0;
        }

        [[nodiscard]] std::size_t write_callback(char *contents, std::size_t size, std::size_t count, void *user_data)
        {
            const auto total_bytes = size * count;
            auto *buffer = static_cast<std::string *>(user_data);
            buffer->append(contents, total_bytes);
            return total_bytes;
        }

        [[nodiscard]] long to_milliseconds(std::chrono::milliseconds duration)
        {
            return static_cast<long>(duration.count());
        }

        template <typename TEmitMessage>
        void parse_event_stream(std::string_view body, TEmitMessage emit_message)
        {
            std::string accumulated_data;

            auto flush_event = [&]()
            {
                if (!accumulated_data.empty())
                {
                    emit_message(accumulated_data);
                    accumulated_data.clear();
                }
            };

            std::size_t offset = 0;
            while (offset <= body.size())
            {
                const auto line_end = body.find('\n', offset);
                const auto raw_line = line_end == std::string_view::npos
                                          ? body.substr(offset)
                                          : body.substr(offset, line_end - offset);
                const auto line = !raw_line.empty() && raw_line.back() == '\r'
                                      ? raw_line.substr(0, raw_line.size() - 1)
                                      : raw_line;

                if (line.empty())
                {
                    flush_event();
                }
                else if (line.front() != ':')
                {
                    const auto separator = line.find(':');
                    const auto field = separator == std::string_view::npos ? line : line.substr(0, separator);
                    auto value = separator == std::string_view::npos ? std::string_view{} : line.substr(separator + 1);
                    if (!value.empty() && value.front() == ' ')
                    {
                        value.remove_prefix(1);
                    }

                    if (field == "data")
                    {
                        if (!accumulated_data.empty())
                        {
                            accumulated_data += '\n';
                        }

                        accumulated_data += value;
                    }
                }

                if (line_end == std::string_view::npos)
                {
                    break;
                }

                offset = line_end + 1;
            }

            flush_event();
        }

    } // namespace

    class HttpsTransport::Impl
    {
    public:
        explicit Impl(HttpsTransportOptions options)
            : options_(std::move(options))
        {
        }

        [[nodiscard]] const HttpsTransportOptions &options() const noexcept
        {
            return options_;
        }

        void connect()
        {
            if (!has_supported_scheme(options_.url))
            {
                throw std::invalid_argument("https transport requires an http:// or https:// URL");
            }

            ensure_curl_initialized();

            std::lock_guard<std::mutex> lock(state_mutex_);
            active_ = true;
            close_notified_ = false;
        }

        void close()
        {
            CloseHandler close_handler;
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                if (close_notified_)
                {
                    return;
                }

                active_ = false;
                close_notified_ = true;
            }

            {
                std::lock_guard<std::mutex> lock(handler_mutex_);
                close_handler = close_handler_;
            }

            if (close_handler)
            {
                close_handler();
            }
        }

        void send(const JsonString &message)
        {
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                if (!active_)
                {
                    throw std::logic_error("https transport is not connected");
                }
            }

            auto *curl = curl_easy_init();
            if (curl == nullptr)
            {
                emit_error("failed to allocate libcurl handle");
                throw std::runtime_error("failed to allocate libcurl handle");
            }

            struct curl_slist *header_list = nullptr;
            std::string response_body;
            std::string response_content_type;

            try
            {
                header_list = curl_slist_append(header_list, "Content-Type: application/json");
                header_list = curl_slist_append(header_list, "Accept: application/json, text/event-stream");
                for (const auto &[name, value] : options_.headers)
                {
                    header_list = curl_slist_append(header_list, (name + ": " + value).c_str());
                }

                curl_easy_setopt(curl, CURLOPT_URL, options_.url.c_str());
                curl_easy_setopt(curl, CURLOPT_POST, 1L);
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, message.c_str());
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(message.size()));
                curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, to_milliseconds(options_.connect_timeout));
                curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, to_milliseconds(options_.request_timeout));
                curl_easy_setopt(curl, CURLOPT_USERAGENT, options_.user_agent.c_str());
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &write_callback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
                curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, options_.verify_peer ? 1L : 0L);
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, options_.verify_peer ? 2L : 0L);

                const auto result = curl_easy_perform(curl);
                if (result != CURLE_OK)
                {
                    const std::string error_message = std::string{"https transport request failed: "} +
                                                      curl_easy_strerror(result);
                    emit_error(error_message);
                    throw std::runtime_error(error_message);
                }

                long status_code = 0;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
                if (status_code >= 400)
                {
                    const std::string error_message = "https transport received HTTP status " +
                                                      std::to_string(status_code);
                    emit_error(error_message);
                    throw std::runtime_error(error_message);
                }

                char *content_type = nullptr;
                curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type);
                if (content_type != nullptr)
                {
                    response_content_type = content_type;
                }

                MessageHandler message_handler;
                {
                    std::lock_guard<std::mutex> lock(handler_mutex_);
                    message_handler = message_handler_;
                }

                if (message_handler && !response_body.empty())
                {
                    const auto normalized_content_type = mcp::detail::normalize_content_type(response_content_type);
                    if (normalized_content_type.empty() || normalized_content_type == "application/json")
                    {
                        message_handler(response_body);
                    }
                    else if (normalized_content_type == "text/event-stream")
                    {
                        parse_event_stream(response_body, message_handler);
                    }
                    else
                    {
                        const std::string error_message = "https transport received unsupported content type: " +
                                                          response_content_type;
                        emit_error(error_message);
                        throw std::runtime_error(error_message);
                    }
                }
            }
            catch (...)
            {
                if (header_list != nullptr)
                {
                    curl_slist_free_all(header_list);
                }
                curl_easy_cleanup(curl);
                throw;
            }

            if (header_list != nullptr)
            {
                curl_slist_free_all(header_list);
            }
            curl_easy_cleanup(curl);
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

        HttpsTransportOptions options_;
        MessageHandler message_handler_;
        ErrorHandler error_handler_;
        CloseHandler close_handler_;
        std::mutex handler_mutex_;
        std::mutex state_mutex_;
        bool active_ = false;
        bool close_notified_ = false;
    };

    HttpsTransport::HttpsTransport(HttpsTransportOptions options)
        : impl_(std::make_unique<Impl>(std::move(options)))
    {
    }

    HttpsTransport::~HttpsTransport() = default;

    HttpsTransport::HttpsTransport(HttpsTransport &&) noexcept = default;
    HttpsTransport &HttpsTransport::operator=(HttpsTransport &&) noexcept = default;

    const HttpsTransportOptions &HttpsTransport::options() const noexcept
    {
        return impl_->options();
    }

    void HttpsTransport::connect()
    {
        impl_->connect();
    }

    void HttpsTransport::close()
    {
        impl_->close();
    }

    void HttpsTransport::send(const JsonString &message)
    {
        impl_->send(message);
    }

    void HttpsTransport::set_message_handler(MessageHandler handler)
    {
        impl_->set_message_handler(std::move(handler));
    }

    void HttpsTransport::set_error_handler(ErrorHandler handler)
    {
        impl_->set_error_handler(std::move(handler));
    }

    void HttpsTransport::set_close_handler(CloseHandler handler)
    {
        impl_->set_close_handler(std::move(handler));
    }

} // namespace mcp::client
