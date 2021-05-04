#ifndef ESC_DETAIL_U32_TO_MB_HPP
#define ESC_DETAIL_U32_TO_MB_HPP
#include <array>
#include <cstddef>
#include <utility>

namespace esc::detail {

/// Convert the given char32_t into a multi-byte array of chars.
/** Depends on the currently set clocale to transform the char32_t. */
[[nodiscard]] auto u32_to_mb(char32_t c)
    -> std::pair<std::size_t, std::array<char, 4>>;

}  // namespace esc::detail
#endif  // ESC_DETAIL_U32_TO_MB_HPP
