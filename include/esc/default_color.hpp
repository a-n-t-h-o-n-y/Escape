#pragma once

namespace esc {

/// Default Color of the terminal.
struct DefaultColor {
    [[nodiscard]] constexpr bool operator==(DefaultColor const&) const =
        default;
    [[nodiscard]] constexpr bool operator!=(DefaultColor const&) const =
        default;
};

}  // namespace esc