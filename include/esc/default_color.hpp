#pragma once

namespace esc {

/// Default Color of the terminal.
struct DefaultColor {};

/// Checks for equality, always returns true.
[[nodiscard]] constexpr auto operator==(DefaultColor, DefaultColor) -> bool
{
    return true;
}

/// Checks for inequality, always returns false.
[[nodiscard]] constexpr auto operator!=(DefaultColor, DefaultColor) -> bool
{
    return false;
}

/// Tag type for terminal default color background.
struct DefaultColorBG {};

/// Tag type for terminal default color foreground.
struct DefaultColorFG {};

/// Return a background tag type to use with escape(...) function.
[[nodiscard]] constexpr auto background(DefaultColor) -> DefaultColorBG
{
    return {};
}

/// Return a background tag type to use with escape(...) function.
[[nodiscard]] constexpr auto bg(DefaultColor) -> DefaultColorBG { return {}; }

/// Return a foreground tag type to use with escape(...) function.
[[nodiscard]] constexpr auto foreground(DefaultColor) -> DefaultColorFG
{
    return {};
}

/// Return a foreground tag type to use with escape(...) function.
[[nodiscard]] constexpr auto fg(DefaultColor) -> DefaultColorFG { return {}; }

}  // namespace esc