#pragma once

#include <string_view>

namespace esc::detail {

/**
 * Check if \p TERM is a unicode rxvt terminal type.
 *
 * @param TERM The terminal type to check.
 * @return True if \p TERM is a unicode rxvt terminal type.
 */
[[nodiscard]] auto is_urxvt(std::string_view TERM) -> bool;

}  // namespace esc::detail