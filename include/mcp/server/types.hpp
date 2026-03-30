#pragma once

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

namespace mcp::server
{

    /**
     * @brief Temporary JSON payload representation.
     */
    using JsonString = std::string;

    /**
     * @brief Optional value wrapper used by public SDK types.
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
     * @brief Lifecycle states for an MCP server instance.
     */
    enum class ServerState
    {
        stopped,
        starting,
        running,
        stopping,
        failed,
    };

    /**
     * @brief Logging severity used by server callbacks.
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
     * @brief Public identity advertised by the server during initialization.
     */
    struct ServerInfo
    {
        std::string name;
        std::string version;
        Optional<std::string> title;
        Optional<std::string> instructions;
    };

    /**
     * @brief Capability flags and experimental extensions exposed by the server.
     */
    struct ServerCapabilities
    {
        bool tools = true;
        bool resources = true;
        bool prompts = true;
        PropertyMap experimental;
    };

    /**
     * @brief Registered tool metadata.
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
     * @brief Registered resource metadata.
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
     * @brief Registered prompt metadata.
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
     * @brief Generic request context delivered to a server handler.
     */
    struct RequestContext
    {
        JsonString id;
        std::string method;
        JsonString params;
    };

    /**
     * @brief Generic client notification delivered to the server.
     */
    struct Notification
    {
        std::string method;
        JsonString params;
    };

} // namespace mcp::server
