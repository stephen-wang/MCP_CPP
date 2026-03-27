#pragma once

#include <chrono>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

#if defined(__has_include)
#if __has_include(<optional>)
#include <optional>
#define MCP_CPP_HAS_STD_OPTIONAL 1
#elif __has_include(<experimental/optional>)
#include <experimental/optional>
#define MCP_CPP_HAS_EXPERIMENTAL_OPTIONAL 1
#endif
#else
#include <optional>
#define MCP_CPP_HAS_STD_OPTIONAL 1
#endif

namespace mcp::client
{

    /**
     * @brief Temporary JSON payload representation.
     *
     * The SDK intentionally keeps the public client interface independent from a
     * specific JSON library. Until the serialization layer is finalized, raw JSON
     * text is used at the API boundary.
     */
    using JsonString = std::string;

    /**
     * @brief Optional value wrapper used by public SDK types.
     *
     * The alias prefers `std::optional` when the active toolchain supports it,
     * but can fall back to `std::experimental::optional` for older IntelliSense
     * or compiler configurations.
     */
#if defined(MCP_CPP_HAS_STD_OPTIONAL)
    template <typename T>
    using Optional = std::optional<T>;
#elif defined(MCP_CPP_HAS_EXPERIMENTAL_OPTIONAL)
    template <typename T>
    using Optional = std::experimental::optional<T>;
#else
#error "mcp_cpp requires <optional> or <experimental/optional> support"
#endif

    /**
     * @brief Generic key-value map for experimental or transport-specific fields.
     */
    using PropertyMap = std::map<std::string, JsonString>;

    /**
     * @brief Lifecycle states for an MCP client connection.
     */
    enum class ConnectionState
    {
        disconnected,
        connecting,
        initializing,
        ready,
        closing,
        failed,
    };

    /**
     * @brief Logging severity used by client callbacks.
     */
    enum class LogLevel
    {
        trace,
        debug,
        info,
        warn,
        error,
        critical,
    };

    /**
     * @brief Identifies the client during the MCP initialize handshake.
     */
    struct ClientInfo
    {
        std::string name;
        std::string version;
        Optional<std::string> title;
    };

    /**
     * @brief Advertised capabilities supported by the client.
     *
     * These flags map to major capability groups that an MCP server may use when
     * deciding what interactions are available for this session.
     */
    struct ClientCapabilities
    {
        bool roots = false;
        bool sampling = false;
        bool elicitation = false;
        PropertyMap experimental;
    };

    /**
     * @brief Parameters required to initialize a client session.
     */
    struct InitializeParams
    {
        std::string protocol_version;
        ClientInfo client_info;
        ClientCapabilities capabilities;
    };

    /**
     * @brief Basic information learned about the connected server.
     */
    struct ServerInfo
    {
        std::string name;
        std::string version;
        Optional<std::string> title;
        Optional<std::string> instructions;
        PropertyMap capabilities;
    };

    /**
     * @brief Per-request behavior overrides.
     */
    struct RequestOptions
    {
        std::chrono::milliseconds timeout{30000};
        bool report_progress = false;
    };

    /**
     * @brief Client-side view of an MCP tool.
     */
    struct ToolDescriptor
    {
        std::string name;
        Optional<std::string> title;
        Optional<std::string> description;
        Optional<JsonString> input_schema;
        PropertyMap metadata;
    };

    /**
     * @brief Client-side view of an MCP resource.
     */
    struct ResourceDescriptor
    {
        std::string uri;
        std::string name;
        Optional<std::string> title;
        Optional<std::string> description;
        Optional<std::string> mime_type;
        PropertyMap metadata;
    };

    /**
     * @brief Client-side view of an MCP prompt.
     */
    struct PromptDescriptor
    {
        std::string name;
        Optional<std::string> title;
        Optional<std::string> description;
        std::vector<std::string> arguments;
        PropertyMap metadata;
    };

    /**
     * @brief Generic server notification delivered to the client.
     */
    struct Notification
    {
        std::string method;
        JsonString params;
    };

    /**
     * @brief Generic response container for raw protocol access.
     *
     * Higher-level wrappers can project this payload into typed result objects once
     * the protocol model layer is introduced.
     */
    struct Response
    {
        JsonString result;
        Optional<JsonString> error;
    };

} // namespace mcp::client
