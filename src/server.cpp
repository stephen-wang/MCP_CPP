#include <mcp/server.hpp>
#include <mcp/server/transport.hpp>

#include "json_rpc_util.hpp"

#include <cctype>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace mcp::server
{
    namespace
    {
        std::size_t skip_whitespace(std::string_view json, std::size_t pos)
        {
            while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos])) != 0)
            {
                ++pos;
            }

            return pos;
        }

        Optional<std::string> extract_json_member(std::string_view json, std::string_view key)
        {
            const std::string needle = std::string{"\""} + std::string{key} + '"';
            const auto key_pos = json.find(needle);
            if (key_pos == std::string_view::npos)
            {
                return {};
            }

            auto value_pos = json.find(':', key_pos + needle.size());
            if (value_pos == std::string_view::npos)
            {
                return {};
            }

            value_pos = skip_whitespace(json, value_pos + 1);
            if (value_pos >= json.size())
            {
                return {};
            }

            const auto start = value_pos;
            const char first = json[value_pos];
            if (first == '"')
            {
                ++value_pos;
                bool escaped = false;
                while (value_pos < json.size())
                {
                    const char ch = json[value_pos];
                    if (escaped)
                    {
                        escaped = false;
                    }
                    else if (ch == '\\')
                    {
                        escaped = true;
                    }
                    else if (ch == '"')
                    {
                        ++value_pos;
                        break;
                    }

                    ++value_pos;
                }

                return std::string{json.substr(start, value_pos - start)};
            }

            if (first == '{' || first == '[')
            {
                const char open = first;
                const char close = first == '{' ? '}' : ']';
                std::size_t depth = 0;
                bool in_string = false;
                bool escaped = false;

                while (value_pos < json.size())
                {
                    const char ch = json[value_pos];
                    if (in_string)
                    {
                        if (escaped)
                        {
                            escaped = false;
                        }
                        else if (ch == '\\')
                        {
                            escaped = true;
                        }
                        else if (ch == '"')
                        {
                            in_string = false;
                        }
                    }
                    else
                    {
                        if (ch == '"')
                        {
                            in_string = true;
                        }
                        else if (ch == open)
                        {
                            ++depth;
                        }
                        else if (ch == close)
                        {
                            --depth;
                            if (depth == 0)
                            {
                                ++value_pos;
                                break;
                            }
                        }
                    }

                    ++value_pos;
                }

                return std::string{json.substr(start, value_pos - start)};
            }

            while (value_pos < json.size() && json[value_pos] != ',' && json[value_pos] != '}')
            {
                ++value_pos;
            }

            return std::string{json.substr(start, value_pos - start)};
        }

        Optional<std::string> extract_json_string_member(std::string_view json, std::string_view key)
        {
            const auto raw = extract_json_member(json, key);
            if (!raw || raw->size() < 2 || raw->front() != '"' || raw->back() != '"')
            {
                return {};
            }

            std::string value;
            value.reserve(raw->size() - 2);
            bool escaped = false;
            for (std::size_t index = 1; index + 1 < raw->size(); ++index)
            {
                const char ch = (*raw)[index];
                if (escaped)
                {
                    switch (ch)
                    {
                    case 'n':
                        value += '\n';
                        break;
                    case 'r':
                        value += '\r';
                        break;
                    case 't':
                        value += '\t';
                        break;
                    default:
                        value += ch;
                        break;
                    }
                    escaped = false;
                }
                else if (ch == '\\')
                {
                    escaped = true;
                }
                else
                {
                    value += ch;
                }
            }

            return value;
        }

        std::string build_server_info_json(const ServerInfo &server_info)
        {
            std::string json = "{";
            json += "\"name\":" + mcp::detail::quote_json_string(server_info.name);
            json += ",\"version\":" + mcp::detail::quote_json_string(server_info.version);

            if (server_info.title)
            {
                json += ",\"title\":" + mcp::detail::quote_json_string(*server_info.title);
            }

            json += '}';
            return json;
        }

        std::string build_capabilities_json(const ServerCapabilities &capabilities)
        {
            std::string json = "{";
            json += "\"tools\":";
            json += capabilities.tools ? "true" : "false";
            json += ",\"resources\":";
            json += capabilities.resources ? "true" : "false";
            json += ",\"prompts\":";
            json += capabilities.prompts ? "true" : "false";

            if (!capabilities.experimental.empty())
            {
                json += ",\"experimental\":" + mcp::detail::build_property_map_json(capabilities.experimental);
            }

            json += '}';
            return json;
        }

        std::string build_property_map_json(const PropertyMap &metadata)
        {
            return mcp::detail::build_property_map_json(metadata);
        }

        std::string build_tool_json(const ToolDescriptor &tool)
        {
            std::string json = "{";
            json += "\"name\":" + mcp::detail::quote_json_string(tool.name);

            if (tool.title)
            {
                json += ",\"title\":" + mcp::detail::quote_json_string(*tool.title);
            }

            if (tool.description)
            {
                json += ",\"description\":" + mcp::detail::quote_json_string(*tool.description);
            }

            if (tool.input_schema)
            {
                json += ",\"inputSchema\":" + *tool.input_schema;
            }

            if (!tool.metadata.empty())
            {
                json += ",\"metadata\":" + build_property_map_json(tool.metadata);
            }

            json += '}';
            return json;
        }

        std::string build_resource_json(const ResourceDescriptor &resource)
        {
            std::string json = "{";
            json += "\"uri\":" + mcp::detail::quote_json_string(resource.uri);
            json += ",\"name\":" + mcp::detail::quote_json_string(resource.name);

            if (resource.title)
            {
                json += ",\"title\":" + mcp::detail::quote_json_string(*resource.title);
            }

            if (resource.description)
            {
                json += ",\"description\":" + mcp::detail::quote_json_string(*resource.description);
            }

            if (resource.mime_type)
            {
                json += ",\"mimeType\":" + mcp::detail::quote_json_string(*resource.mime_type);
            }

            if (!resource.metadata.empty())
            {
                json += ",\"metadata\":" + build_property_map_json(resource.metadata);
            }

            json += '}';
            return json;
        }

        std::string build_prompt_json(const PromptDescriptor &prompt)
        {
            std::string json = "{";
            json += "\"name\":" + mcp::detail::quote_json_string(prompt.name);

            if (prompt.title)
            {
                json += ",\"title\":" + mcp::detail::quote_json_string(*prompt.title);
            }

            if (prompt.description)
            {
                json += ",\"description\":" + mcp::detail::quote_json_string(*prompt.description);
            }

            json += ",\"arguments\":[";
            bool first = true;
            for (const auto &argument : prompt.arguments)
            {
                if (!first)
                {
                    json += ',';
                }

                json += mcp::detail::quote_json_string(argument);
                first = false;
            }
            json += ']';

            if (!prompt.metadata.empty())
            {
                json += ",\"metadata\":" + build_property_map_json(prompt.metadata);
            }

            json += '}';
            return json;
        }

        std::string build_initialize_result_json(const ServerOptions &options)
        {
            std::string json = "{";
            json += "\"protocolVersion\":" + mcp::detail::quote_json_string(options.protocol_version);
            json += ",\"capabilities\":" + build_capabilities_json(options.capabilities);
            json += ",\"serverInfo\":" + build_server_info_json(options.server_info);

            if (options.server_info.instructions)
            {
                json += ",\"instructions\":" + mcp::detail::quote_json_string(*options.server_info.instructions);
            }

            json += '}';
            return json;
        }

        std::string build_list_result_json(std::string_view collection_name, const std::vector<ToolDescriptor> &tools)
        {
            std::string json = "{";
            json += mcp::detail::quote_json_string(collection_name);
            json += ":[";

            bool first = true;
            for (const auto &tool : tools)
            {
                if (!first)
                {
                    json += ',';
                }

                json += build_tool_json(tool);
                first = false;
            }

            json += "]}";
            return json;
        }

        std::string build_list_result_json(std::string_view collection_name, const std::vector<ResourceDescriptor> &resources)
        {
            std::string json = "{";
            json += mcp::detail::quote_json_string(collection_name);
            json += ":[";

            bool first = true;
            for (const auto &resource : resources)
            {
                if (!first)
                {
                    json += ',';
                }

                json += build_resource_json(resource);
                first = false;
            }

            json += "]}";
            return json;
        }

        std::string build_list_result_json(std::string_view collection_name, const std::vector<PromptDescriptor> &prompts)
        {
            std::string json = "{";
            json += mcp::detail::quote_json_string(collection_name);
            json += ":[";

            bool first = true;
            for (const auto &prompt : prompts)
            {
                if (!first)
                {
                    json += ',';
                }

                json += build_prompt_json(prompt);
                first = false;
            }

            json += "]}";
            return json;
        }

        std::string build_response_message(std::string_view id, std::string_view result)
        {
            std::string json = "{";
            json += "\"jsonrpc\":\"2.0\"";
            json += ",\"id\":" + std::string{id};
            json += ",\"result\":" + std::string{result};
            json += '}';
            return json;
        }

        std::string build_error_message(std::string_view id, int code, std::string_view message)
        {
            std::string json = "{";
            json += "\"jsonrpc\":\"2.0\"";
            json += ",\"id\":" + std::string{id};
            json += ",\"error\":{";
            json += "\"code\":" + std::to_string(code);
            json += ",\"message\":" + mcp::detail::quote_json_string(message);
            json += "}}";
            return json;
        }

    } // namespace

    class Server::Impl
    {
    public:
        explicit Impl(ServerOptions options)
            : options_(std::move(options))
        {
        }

        void set_transport(std::shared_ptr<IServerTransport> transport)
        {
            if (state_ != ServerState::stopped && state_ != ServerState::failed)
            {
                stop();
            }

            transport_ = std::move(transport);
            bind_transport_handlers();
        }

        [[nodiscard]] const ServerOptions &options() const noexcept
        {
            return options_;
        }

        [[nodiscard]] ServerState state() const noexcept
        {
            return state_;
        }

        [[nodiscard]] bool is_running() const noexcept
        {
            return state_ == ServerState::running;
        }

        void start()
        {
            auto transport = require_transport();

            if (state_ == ServerState::running || state_ == ServerState::starting)
            {
                return;
            }

            bind_transport_handlers();
            set_state(ServerState::starting);
            transport->listen();
            set_state(ServerState::running);
        }

        void stop()
        {
            if (state_ == ServerState::stopped)
            {
                return;
            }

            set_state(ServerState::stopping);

            if (transport_)
            {
                transport_->close();
            }

            set_state(ServerState::stopped);
        }

        void notify(std::string method, JsonString params) const
        {
            const auto transport = require_transport();
            if (state_ != ServerState::running)
            {
                throw std::logic_error("server is not running");
            }

            transport->send(mcp::detail::build_notification_message(method, params));
        }

        void set_request_handler(std::string method, RequestHandler handler)
        {
            request_handlers_[std::move(method)] = std::move(handler);
        }

        void clear_request_handler(std::string_view method)
        {
            request_handlers_.erase(std::string{method});
        }

        void register_tool(ToolDescriptor tool)
        {
            replace_descriptor(tools_, std::move(tool), [](const ToolDescriptor &item)
                               { return item.name; });
        }

        void register_resource(ResourceDescriptor resource)
        {
            replace_descriptor(resources_, std::move(resource), [](const ResourceDescriptor &item)
                               { return item.uri; });
        }

        void register_prompt(PromptDescriptor prompt)
        {
            replace_descriptor(prompts_, std::move(prompt), [](const PromptDescriptor &item)
                               { return item.name; });
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
        template <typename TCollection, typename TItem, typename TKeySelector>
        void replace_descriptor(TCollection &collection, TItem item, TKeySelector key_selector)
        {
            const auto key = key_selector(item);
            for (auto &existing : collection)
            {
                if (key_selector(existing) == key)
                {
                    existing = std::move(item);
                    return;
                }
            }

            collection.push_back(std::move(item));
        }

        [[nodiscard]] std::shared_ptr<IServerTransport> require_transport() const
        {
            if (!transport_)
            {
                throw std::logic_error("server transport has not been configured");
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

            const auto method = extract_json_string_member(message, "method");
            if (!method)
            {
                log(LogLevel::warn, "Inbound message ignored because no method field was found.");
                return;
            }

            const auto params = extract_json_member(message, "params").value_or("{}");
            const auto id = extract_json_member(message, "id");

            if (!id)
            {
                if (options_.callbacks.notification_received)
                {
                    options_.callbacks.notification_received(Notification{*method, params});
                }
                return;
            }

            handle_request(*id, *method, params);
        }

        void handle_request(const JsonString &id, const std::string &method, const JsonString &params)
        {
            auto transport = require_transport();

            if (method == "initialize" && options_.auto_handle_initialize)
            {
                transport->send(build_response_message(id, build_initialize_result_json(options_)));
                return;
            }

            if (method == "tools/list")
            {
                transport->send(build_response_message(id, build_list_result_json("tools", tools_)));
                return;
            }

            if (method == "resources/list")
            {
                transport->send(build_response_message(id, build_list_result_json("resources", resources_)));
                return;
            }

            if (method == "prompts/list")
            {
                transport->send(build_response_message(id, build_list_result_json("prompts", prompts_)));
                return;
            }

            const auto handler = request_handlers_.find(method);
            if (handler == request_handlers_.end())
            {
                transport->send(build_error_message(id, -32601, "Method not found"));
                return;
            }

            const RequestContext context{id, method, params};
            transport->send(build_response_message(id, handler->second(context)));
        }

        void on_transport_error(const std::string &message)
        {
            log(LogLevel::error, message);
            set_state(ServerState::failed);

            if (options_.callbacks.transport_error)
            {
                options_.callbacks.transport_error(message);
            }
        }

        void on_transport_closed()
        {
            if (state_ != ServerState::stopping)
            {
                set_state(ServerState::stopped);
            }
        }

        void set_state(ServerState next_state)
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

        ServerOptions options_;
        std::shared_ptr<IServerTransport> transport_;
        ServerState state_ = ServerState::stopped;
        std::unordered_map<std::string, RequestHandler> request_handlers_;
        std::vector<ToolDescriptor> tools_;
        std::vector<ResourceDescriptor> resources_;
        std::vector<PromptDescriptor> prompts_;
    };

    Server::Server(ServerOptions options)
        : impl_(std::make_unique<Impl>(std::move(options)))
    {
    }

    Server::~Server() = default;

    Server::Server(Server &&) noexcept = default;
    Server &Server::operator=(Server &&) noexcept = default;

    void Server::set_transport(std::shared_ptr<IServerTransport> transport)
    {
        impl_->set_transport(std::move(transport));
    }

    const ServerOptions &Server::options() const noexcept
    {
        return impl_->options();
    }

    ServerState Server::state() const noexcept
    {
        return impl_->state();
    }

    bool Server::is_running() const noexcept
    {
        return impl_->is_running();
    }

    void Server::start()
    {
        impl_->start();
    }

    void Server::stop()
    {
        impl_->stop();
    }

    void Server::notify(std::string method, JsonString params) const
    {
        impl_->notify(std::move(method), std::move(params));
    }

    void Server::set_request_handler(std::string method, RequestHandler handler)
    {
        impl_->set_request_handler(std::move(method), std::move(handler));
    }

    void Server::clear_request_handler(std::string_view method)
    {
        impl_->clear_request_handler(method);
    }

    void Server::register_tool(ToolDescriptor tool)
    {
        impl_->register_tool(std::move(tool));
    }

    void Server::register_resource(ResourceDescriptor resource)
    {
        impl_->register_resource(std::move(resource));
    }

    void Server::register_prompt(PromptDescriptor prompt)
    {
        impl_->register_prompt(std::move(prompt));
    }

    const std::vector<ToolDescriptor> &Server::tools() const noexcept
    {
        return impl_->tools();
    }

    const std::vector<ResourceDescriptor> &Server::resources() const noexcept
    {
        return impl_->resources();
    }

    const std::vector<PromptDescriptor> &Server::prompts() const noexcept
    {
        return impl_->prompts();
    }

} // namespace mcp::server
