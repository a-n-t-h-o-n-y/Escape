#ifndef ESC_DEFAULT_COLOR_HPP
#define ESC_DEFAULT_COLOR_HPP
namespace esc {

/// Default Color of the terminal.
struct Default_color {};

/// Checks for equality, always returns true.
[[nodiscard]] constexpr auto operator==(Default_color, Default_color) -> bool
{
    return true;
}

/// Checks for inequality, always returns false.
[[nodiscard]] constexpr auto operator!=(Default_color, Default_color) -> bool
{
    return false;
}

/// Tag type for terminal default color background.
struct BG_Default_color {};

/// Tag type for terminal default color foreground.
struct FG_Default_color {};

/// Return a background tag type to use with escape(...) function.
[[nodiscard]] auto constexpr background(Default_color) -> BG_Default_color
{
    return {};
}

/// Return a foreground tag type to use with escape(...) function.
[[nodiscard]] auto constexpr foreground(Default_color) -> FG_Default_color
{
    return {};
}

}  // namespace esc
#endif  // ESC_DEFAULT_COLOR_HPP
