#ifndef ESC_DETAIL_IS_URXVT_HPP
#define ESC_DETAIL_IS_URXVT_HPP
#include <string_view>

namespace esc::detail {

/// Return true if \p TERM is a unicode rxvt terminal type.
[[nodiscard]] auto is_urxvt(std::string_view TERM) -> bool;

}  // namespace esc::detail
#endif  // ESC_DETAIL_IS_URXVT_HPP
