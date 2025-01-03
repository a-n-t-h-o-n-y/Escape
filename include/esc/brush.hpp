#pragma once

#include <esc/color.hpp>
#include <esc/trait.hpp>

namespace esc {

/**
 * Holds traits and colors for text display on a terminal.
 */
struct Brush {
    Color background = XColor::Default;
    Color foreground = XColor::Default;
    Traits traits = Traits{Trait::None};

    [[nodiscard]] constexpr bool operator==(Brush const&) const = default;
    [[nodiscard]] constexpr bool operator!=(Brush const&) const = default;
};

}  // namespace esc