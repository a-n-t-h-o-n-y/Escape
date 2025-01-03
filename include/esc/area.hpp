#pragma once

namespace esc {

/**
 * Holds height/width for a 2D rectangle.
 */
struct Area {
    int width;
    int height;

    auto constexpr operator==(Area const& other) const -> bool = default;
    auto constexpr operator!=(Area const& other) const -> bool = default;
};

}  // namespace esc