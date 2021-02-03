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

inline auto constexpr Black          = Color_index{0};
inline auto constexpr Red            = Color_index{1};
inline auto constexpr Green          = Color_index{2};
inline auto constexpr Yellow         = Color_index{3};
inline auto constexpr Blue           = Color_index{4};
inline auto constexpr Magenta        = Color_index{5};
inline auto constexpr Cyan           = Color_index{6};
inline auto constexpr White          = Color_index{7};
inline auto constexpr Bright_black   = Color_index{8};
inline auto constexpr Bright_red     = Color_index{9};
inline auto constexpr Bright_green   = Color_index{10};
inline auto constexpr Bright_yellow  = Color_index{11};
inline auto constexpr Bright_blue    = Color_index{12};
inline auto constexpr Bright_magenta = Color_index{13};
inline auto constexpr Bright_cyan    = Color_index{14};
inline auto constexpr Bright_white   = Color_index{15};

/// Tag type for Color_index that is used as a background.
struct BG_Color_index {
    Color_index value;
};

/// Tag type for Color_index that is used as a foreground.
struct FG_Color_index {
    Color_index value;
};

/// Return a background tag type to use with escape(...) function.
[[nodiscard]] inline auto background(Color_index x) -> BG_Color_index
{
    return {x};
}

/// Return a foreground tag type to use with escape(...) function.
[[nodiscard]] inline auto foreground(Color_index x) -> FG_Color_index
{
    return {x};
}

}  // namespace esc
#endif  // ESC_COLOR_INDEX_HPP
