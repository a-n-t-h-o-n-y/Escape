#pragma once

#include <esc/color.hpp>
#include <esc/default_color.hpp>
#include <esc/trait.hpp>

namespace esc {

/// Convenience type to hold visual aspects of the terminal display.
/** use set_brush() */
struct Brush {
    Color background = DefaultColor{};
    Color foreground = DefaultColor{};
    Traits traits    = Traits{Trait::None};

    [[nodiscard]] constexpr bool operator==(Brush const&) const = default;
    [[nodiscard]] constexpr bool operator!=(Brush const&) const = default;
};

}  // namespace esc