#pragma once

namespace esc {

/**
 * Holds height/width for a 2D rectangle.
 */
struct Area {
    int width = 0;
    int height = 0;

    constexpr auto operator==(Area const& other) const -> bool = default;
    constexpr auto operator!=(Area const& other) const -> bool = default;
};

}  // namespace esc