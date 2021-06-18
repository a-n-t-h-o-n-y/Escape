#ifndef ESC_SRC_DETAIL_TTY_FILE_HPP
#define ESC_SRC_DETAIL_TTY_FILE_HPP
#include <optional>

namespace esc::detail {

/// If has_value(), then alt keyboard mode is in use and this is readable.
inline auto tty_file_descriptor = std::optional<int>{std::nullopt};

}  // namespace esc::detail
#endif  // ESC_SRC_DETAIL_TTY_FILE_HPP
