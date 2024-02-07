#pragma once

namespace esc {

/**
 * Represents a rectangular area with a width and height.
 */
struct Area {
    int width;
    int height;

    auto constexpr operator==(Area const& other) const -> bool = default;
    auto constexpr operator!=(Area const& other) const -> bool = default;
};

}  // namespace esc