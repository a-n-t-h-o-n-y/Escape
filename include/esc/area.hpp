#pragma once

namespace esc {

/**
 * Holds height/width for a 2D rectangle.
 */
struct Area {
    int width;
    int height;

    constexpr auto operator==(Area const& other) const -> bool = default;
    constexpr auto operator!=(Area const& other) const -> bool = default;
};

}  // namespace esc