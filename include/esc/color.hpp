#pragma once

#include <variant>

#include <esc/color_index.hpp>
#include <esc/default_color.hpp>
#include <esc/true_color.hpp>

namespace esc {

/// Variant for any of the color types that can be used.
using Color = std::variant<ColorIndex, TrueColor, DefaultColor>;

struct ColorFG {
    Color value;
};

struct ColorBG {
    Color value;
};

/// Return a background tag type to use with escape(...) function.
[[nodiscard]] constexpr auto background(Color c) -> ColorBG { return {c}; }

/// Return a background tag type to use with escape(...) function.
[[nodiscard]] constexpr auto bg(Color c) -> ColorBG { return {c}; }

/// Return a foreground tag type to use with escape(...) function.
[[nodiscard]] constexpr auto foreground(Color c) -> ColorFG { return {c}; }

/// Return a foreground tag type to use with escape(...) function.
[[nodiscard]] constexpr auto fg(Color c) -> ColorFG { return {c}; }

}  // namespace esc