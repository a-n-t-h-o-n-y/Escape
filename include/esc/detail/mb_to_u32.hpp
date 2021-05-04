#ifndef ESC_DETAIL_MB_TO_U32_HPP
#define ESC_DETAIL_MB_TO_U32_HPP
#include <array>

namespace esc::detail {

/// Convert the given multi-byte array character into a char32_t.
/** Depends on the currently set clocale to transform the bytes. */
[[nodiscard]] auto mb_to_u32(std::array<char, 4> bytes) -> char32_t;

}  // namespace esc::detail
#endif  // ESC_DETAIL_MB_TO_U32_HPP
