#pragma once

#include <cctype>
#include <string>
#include <string_view>

namespace mcp::detail
{

    [[nodiscard]] inline std::string trim_ascii_whitespace(std::string_view value)
    {
        std::size_t begin = 0;
        while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])) != 0)
        {
            ++begin;
        }

        std::size_t end = value.size();
        while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
        {
            --end;
        }

        return std::string{value.substr(begin, end - begin)};
    }

    [[nodiscard]] inline std::string to_lower_ascii(std::string_view value)
    {
        std::string normalized;
        normalized.reserve(value.size());
        for (const char ch : value)
        {
            normalized += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        return normalized;
    }

    [[nodiscard]] inline std::string normalize_content_type(std::string_view content_type)
    {
        const auto separator = content_type.find(';');
        const auto mime_type = separator == std::string_view::npos
                                   ? content_type
                                   : content_type.substr(0, separator);
        return to_lower_ascii(trim_ascii_whitespace(mime_type));
    }

} // namespace mcp::detail