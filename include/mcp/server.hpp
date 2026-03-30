#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <mcp/server/options.hpp>
#include <mcp/server/types.hpp>

namespace mcp::server
{

    class IServerTransport;

    /**
     * @brief High-level MCP server facade.
     *
     * `Server` owns registration state, handles a minimal request-routing layer,
     * and exposes a transport-independent surface for building MCP endpoints.
     */
    class Server
    {
    public:
        using RequestHandler = std::function<JsonString(const RequestContext &)>;

        /**
         * @brief Construct a server with the supplied runtime options.
         */
        explicit Server(ServerOptions options = {});

        ~Server();

        Server(const Server &) = delete;
        Server &operator=(const Server &) = delete;
        Server(Server &&) noexcept;
        Server &operator=(Server &&) noexcept;

        /**
         * @brief Install the transport used for subsequent start attempts.
         */
        void set_transport(std::shared_ptr<IServerTransport> transport);

        /**
         * @brief Access the server configuration.
         */
        [[nodiscard]] const ServerOptions &options() const noexcept;

        /**
         * @brief Report the current lifecycle state.
         */
        [[nodiscard]] ServerState state() const noexcept;

        /**
         * @brief Return true when the server transport is active.
         */
        [[nodiscard]] bool is_running() const noexcept;

        /**
         * @brief Begin serving messages on the configured transport.
         */
        void start();

        /**
         * @brief Stop the active transport.
         */
        void stop();

        /**
         * @brief Send a raw MCP notification to the connected client.
         */
        void notify(std::string method, JsonString params = {}) const;

        /**
         * @brief Register or replace a generic request handler.
         */
        void set_request_handler(std::string method, RequestHandler handler);

        /**
         * @brief Remove a previously registered generic request handler.
         */
        void clear_request_handler(std::string_view method);

        /**
         * @brief Register tool metadata for automatic `tools/list` handling.
         */
        void register_tool(ToolDescriptor tool);

        /**
         * @brief Register resource metadata for automatic `resources/list` handling.
         */
        void register_resource(ResourceDescriptor resource);

        /**
         * @brief Register prompt metadata for automatic `prompts/list` handling.
         */
        void register_prompt(PromptDescriptor prompt);

        /**
         * @brief Return registered tools.
         */
        [[nodiscard]] const std::vector<ToolDescriptor> &tools() const noexcept;

        /**
         * @brief Return registered resources.
         */
        [[nodiscard]] const std::vector<ResourceDescriptor> &resources() const noexcept;

        /**
         * @brief Return registered prompts.
         */
        [[nodiscard]] const std::vector<PromptDescriptor> &prompts() const noexcept;

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace mcp::server
