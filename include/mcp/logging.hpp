#pragma once

#include <iostream>

namespace mcp
{

    /**
     * @brief Default sink for diagnostic logging when stdio transports are in use.
     *
     * This reference is bound to `std::cerr` so protocol traffic on `stdout`
     * remains unmodified.
     */
    inline std::ostream &log_output = std::cerr;

} // namespace mcp
