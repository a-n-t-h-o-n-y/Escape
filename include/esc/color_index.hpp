#ifndef ESC_COLOR_INDEX_HPP
#define ESC_COLOR_INDEX_HPP
#include <cstdint>

namespace esc {

/// Color Index for XTerm like palette [0 - 255].
struct Color_index {
    std::uint8_t value;
};

/// Return true if \p a and \p b contain the same index value.
inline auto operator==(Color_index a, Color_index b) -> bool
{
    return a.value == b.value;
}

/// Return false if \p a and \p b contain the same index value.
inline auto operator!=(Color_index a, Color_index b) -> bool
{
    return a.value != b.value;
}

}  // namespace esc
#endif  // ESC_COLOR_INDEX_HPP
