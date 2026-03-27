#include <mcp/client.hpp>
#include <mcp/client/transport.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace mcp::client
{
    namespace
    {

        std::string escape_json_string(std::string_view value)
        {
            std::string escaped;
            escaped.reserve(value.size() + 8);

            for (const char ch : value)
            {
                switch (ch)
                {
                case '\\':
                    escaped += "\\\\";
                    break;
                case '"':
                    escaped += "\\\"";
                    break;
                case '\n':
                    escaped += "\\n";
                    break;
                case '\r':
                    escaped += "\\r";
                    break;
                case '\t':
                    escaped += "\\t";
                    break;
                default:
                    escaped += ch;
                    break;
                }
            }

            return escaped;
        }

        std::string quote_json_string(std::string_view value)
        {
            return std::string{"\""} + escape_json_string(value) + '"';
        }

        std::string build_client_info_json(const ClientInfo &client_info)
        {
            std::string json = "{";
            json += "\"name\":" + quote_json_string(client_info.name);
            json += ",\"version\":" + quote_json_string(client_info.version);

            if (client_info.title)
            {
                json += ",\"title\":" + quote_json_string(*client_info.title);
            }

            json += '}';
            return json;
        }

        std::string build_capabilities_json(const ClientCapabilities &capabilities)
        {
            std::string json = "{";
            json += "\"roots\":";
            json += capabilities.roots ? "true" : "false";
            json += ",\"sampling\":";
            json += capabilities.sampling ? "true" : "false";
            json += ",\"elicitation\":";
            json += capabilities.elicitation ? "true" : "false";

            if (!capabilities.experimental.empty())
            {
                json += ",\"experimental\":{";

                bool first = true;
                for (const auto &[key, value] : capabilities.experimental)
                {
                    if (!first)
                    {
                        json += ',';
                    }

                    json += quote_json_string(key);
                    json += ':';
                    json += value.empty() ? "null" : value;
                    first = false;
                }

                json += '}';
            }

            json += '}';
            return json;
        }

        std::string build_initialize_params_json(const InitializeParams &params)
        {
            std::string json = "{";
            json += "\"protocolVersion\":" + quote_json_string(params.protocol_version);
            json += ",\"clientInfo\":" + build_client_info_json(params.client_info);
            json += ",\"capabilities\":" + build_capabilities_json(params.capabilities);
            json += '}';
            return json;
        }

        std::string build_request_message(std::uint64_t id, std::string_view method, std::string_view params)
        {
            std::string json = "{";
            json += "\"jsonrpc\":\"2.0\"";
            json += ",\"id\":" + std::to_string(id);
            json += ",\"method\":" + quote_json_string(method);
            json += ",\"params\":";
            json += params.empty() ? "{}" : std::string{params};
            json += '}';
            return json;
        }

        std::string build_notification_message(std::string_view method, std::string_view params)
        {
            std::string json = "{";
            json += "\"jsonrpc\":\"2.0\"";
            json += ",\"method\":" + quote_json_string(method);
            json += ",\"params\":";
            json += params.empty() ? "{}" : std::string{params};
            json += '}';
            return json;
        }

    } // namespace

    class Client::Impl
    {
    public:
        explicit Impl(ClientOptions options)
            : options_(std::move(options))
        {
        }

        void set_transport(std::shared_ptr<IClientTransport> transport)
        {
            if (state_ != ConnectionState::disconnected && state_ != ConnectionState::failed)
            {
                disconnect();
            }

            transport_ = std::move(transport);
            bind_transport_handlers();
        }

        [[nodiscard]] const ClientOptions &options() const noexcept
        {
            return options_;
        }

        [[nodiscard]] ConnectionState state() const noexcept
        {
            return state_;
        }

        [[nodiscard]] bool is_ready() const noexcept
        {
            return state_ == ConnectionState::ready;
        }

        void connect()
        {
            auto transport = require_transport();

            if (state_ == ConnectionState::ready || state_ == ConnectionState::initializing ||
                state_ == ConnectionState::connecting)
            {
                return;
            }

            bind_transport_handlers();
            set_state(ConnectionState::connecting);
            transport->connect();

            if (options_.auto_initialize)
            {
                initialize();
            }
            else
            {
                set_state(ConnectionState::ready);
            }
        }

        void disconnect()
        {
            if (state_ == ConnectionState::disconnected)
            {
                return;
            }

            set_state(ConnectionState::closing);

            if (transport_)
            {
                transport_->close();
            }

            set_state(ConnectionState::disconnected);
        }

        void initialize()
        {
            auto transport = require_transport();

            if (state_ == ConnectionState::disconnected)
            {
                throw std::logic_error("client transport is not connected");
            }

            set_state(ConnectionState::initializing);

            const auto payload = build_initialize_params_json(options_.initialize);
            transport->send(build_request_message(next_request_id_++, "initialize", payload));

            log(LogLevel::debug, "Initialize request sent; typed handshake response handling is not implemented yet.");
            set_state(ConnectionState::ready);
        }

        [[nodiscard]] Response request(
            std::string method,
            JsonString params,
            const RequestOptions & /*request_options*/) const
        {
            const auto transport = require_transport();

            if (state_ != ConnectionState::ready && method != "initialize")
            {
                throw std::logic_error("client is not ready to send requests");
            }

            transport->send(build_request_message(next_request_id_++, method, params));

            Response response;
            response.error =
                "Request sent, but synchronous response handling is not implemented yet.";
            return response;
        }

        void notify(std::string method, JsonString params) const
        {
            const auto transport = require_transport();

            if (state_ != ConnectionState::ready)
            {
                throw std::logic_error("client is not ready to send notifications");
            }

            transport->send(build_notification_message(method, params));
        }

        [[nodiscard]] const ServerInfo &server_info() const
        {
            return server_info_;
        }

        [[nodiscard]] const std::vector<ToolDescriptor> &tools() const noexcept
        {
            return tools_;
        }

        [[nodiscard]] const std::vector<ResourceDescriptor> &resources() const noexcept
        {
            return resources_;
        }

        [[nodiscard]] const std::vector<PromptDescriptor> &prompts() const noexcept
        {
            return prompts_;
        }

    private:
        [[nodiscard]] std::shared_ptr<IClientTransport> require_transport() const
        {
            if (!transport_)
            {
                throw std::logic_error("client transport has not been configured");
            }

            return transport_;
        }

        void bind_transport_handlers()
        {
            if (!transport_)
            {
                return;
            }

            transport_->set_message_handler([this](const JsonString &message)
                                            { on_transport_message(message); });

            transport_->set_error_handler([this](const std::string &message)
                                          { on_transport_error(message); });

            transport_->set_close_handler([this]()
                                          { on_transport_closed(); });
        }

        void on_transport_message(const JsonString &message)
        {
            log(LogLevel::trace, "Inbound transport message received.");

            if (options_.callbacks.notification_received)
            {
                options_.callbacks.notification_received(Notification{"transport.message", message});
            }
        }

        void on_transport_error(const std::string &message)
        {
            log(LogLevel::error, message);
            set_state(ConnectionState::failed);

            if (options_.callbacks.transport_error)
            {
                options_.callbacks.transport_error(message);
            }
        }

        void on_transport_closed()
        {
            if (state_ != ConnectionState::closing)
            {
                set_state(ConnectionState::disconnected);
            }
        }

        void set_state(ConnectionState next_state)
        {
            if (state_ == next_state)
            {
                return;
            }

            state_ = next_state;

            if (options_.callbacks.state_changed)
            {
                options_.callbacks.state_changed(state_);
            }
        }

        void log(LogLevel level, std::string_view message) const
        {
            if (options_.callbacks.log)
            {
                options_.callbacks.log(level, message);
            }
        }

        ClientOptions options_;
        std::shared_ptr<IClientTransport> transport_;
        ConnectionState state_ = ConnectionState::disconnected;
        ServerInfo server_info_{};
        std::vector<ToolDescriptor> tools_;
        std::vector<ResourceDescriptor> resources_;
        std::vector<PromptDescriptor> prompts_;
        mutable std::uint64_t next_request_id_ = 1;
    };

    Client::Client(ClientOptions options)
        : impl_(std::make_unique<Impl>(std::move(options)))
    {
    }

    Client::~Client() = default;

    Client::Client(Client &&) noexcept = default;
    Client &Client::operator=(Client &&) noexcept = default;

    void Client::set_transport(std::shared_ptr<IClientTransport> transport)
    {
        impl_->set_transport(std::move(transport));
    }

    const ClientOptions &Client::options() const noexcept
    {
        return impl_->options();
    }

    ConnectionState Client::state() const noexcept
    {
        return impl_->state();
    }

    bool Client::is_ready() const noexcept
    {
        return impl_->is_ready();
    }

    void Client::connect()
    {
        impl_->connect();
    }

    void Client::disconnect()
    {
        impl_->disconnect();
    }

    void Client::initialize()
    {
        impl_->initialize();
    }

    Response Client::request(std::string method, JsonString params, RequestOptions options) const
    {
        return impl_->request(std::move(method), std::move(params), options);
    }

    void Client::notify(std::string method, JsonString params) const
    {
        impl_->notify(std::move(method), std::move(params));
    }

    const ServerInfo &Client::server_info() const
    {
        return impl_->server_info();
    }

    const std::vector<ToolDescriptor> &Client::tools() const noexcept
    {
        return impl_->tools();
    }

    const std::vector<ResourceDescriptor> &Client::resources() const noexcept
    {
        return impl_->resources();
    }

    const std::vector<PromptDescriptor> &Client::prompts() const noexcept
    {
        return impl_->prompts();
    }

} // namespace mcp::client
