#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <mcp/client/options.hpp>
#include <mcp/client/types.hpp>

namespace mcp::client
{

    class IClientTransport;

    /**
     * @brief High-level MCP client facade.
     *
     * `Client` owns session state, performs initialization, exposes discovery
     * results, and provides a generic request/notification API for protocol calls.
     * Transport ownership is injected so the same client surface can work over
     * stdio, sockets, named pipes, or custom message channels.
     */
    class Client
    {
    public:
        /**
         * @brief Construct a client with the supplied runtime options.
         */
        explicit Client(ClientOptions options = {});

        ~Client();

        Client(const Client &) = delete;
        Client &operator=(const Client &) = delete;
        Client(Client &&) noexcept;
        Client &operator=(Client &&) noexcept;

        /**
         * @brief Install the transport used for subsequent connection attempts.
         */
        void set_transport(std::shared_ptr<IClientTransport> transport);

        /**
         * @brief Access the client configuration.
         */
        [[nodiscard]] const ClientOptions &options() const noexcept;

        /**
         * @brief Report the current lifecycle state.
         */
        [[nodiscard]] ConnectionState state() const noexcept;

        /**
         * @brief Return true when the session is ready for protocol traffic.
         */
        [[nodiscard]] bool is_ready() const noexcept;

        /**
         * @brief Open the transport connection.
         *
         * If `auto_initialize` is enabled, the client should immediately begin the
         * MCP initialize handshake after the transport becomes available.
         */
        void connect();

        /**
         * @brief Close the active session and underlying transport.
         */
        void disconnect();

        /**
         * @brief Explicitly perform the MCP initialize handshake.
         */
        void initialize();

        /**
         * @brief Send a raw MCP request.
         *
         * This function is intentionally generic so early adopters can use the
         * client before all typed protocol wrappers have been added to the SDK.
         */
        [[nodiscard]] Response request(
            std::string method,
            JsonString params = {},
            RequestOptions options = {}) const;

        /**
         * @brief Send a raw MCP notification.
         */
        void notify(std::string method, JsonString params = {}) const;

        /**
         * @brief Return cached information about the connected server.
         */
        [[nodiscard]] const ServerInfo &server_info() const;

        /**
         * @brief Return the latest discovered tool list.
         */
        [[nodiscard]] const std::vector<ToolDescriptor> &tools() const noexcept;

        /**
         * @brief Return the latest discovered resource list.
         */
        [[nodiscard]] const std::vector<ResourceDescriptor> &resources() const noexcept;

        /**
         * @brief Return the latest discovered prompt list.
         */
        [[nodiscard]] const std::vector<PromptDescriptor> &prompts() const noexcept;

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace mcp::client
