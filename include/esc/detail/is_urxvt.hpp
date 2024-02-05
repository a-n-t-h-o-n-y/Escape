#pragma once

#include <string_view>

namespace esc::detail {

/// Return true if \p TERM is a unicode rxvt terminal type.
[[nodiscard]] auto is_urxvt(std::string_view TERM) -> bool;

}  // namespace esc::detail