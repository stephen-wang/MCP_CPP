#pragma once

#include <string>
#include <string_view>

namespace mcp::detail
{

    inline std::string escape_json_string(std::string_view value)
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

    inline std::string quote_json_string(std::string_view value)
    {
        return std::string{"\""} + escape_json_string(value) + '"';
    }

    template <typename TPropertyMap>
    std::string build_property_map_json(const TPropertyMap &metadata)
    {
        std::string json = "{";
        bool first = true;
        for (const auto &[key, value] : metadata)
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
        return json;
    }

    inline std::string build_notification_message(std::string_view method, std::string_view params)
    {
        std::string json = "{";
        json += "\"jsonrpc\":\"2.0\"";
        json += ",\"method\":" + quote_json_string(method);
        json += ",\"params\":";
        json += params.empty() ? "{}" : std::string{params};
        json += '}';
        return json;
    }

} // namespace mcp::detail