#pragma once

#include <esc/color.hpp>
#include <esc/trait.hpp>

namespace esc {

/**
 * Represents characteristics of the terminal display for text.
 *
 * @details An escape sequence can be generated from this type using
 *          escape(Brush) in sequence.hpp
 */
struct Brush {
    Color background = DefaultColor{};
    Color foreground = DefaultColor{};
    Traits traits    = Traits{Trait::None};

    [[nodiscard]] constexpr bool operator==(Brush const&) const = default;
    [[nodiscard]] constexpr bool operator!=(Brush const&) const = default;
};

}  // namespace esc