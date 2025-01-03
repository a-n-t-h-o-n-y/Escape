#pragma once

#include <tuple>

namespace esc {

/**
 * A point on the terminal screen.
 * @details The top-left cell of the terminal is Point{x:0, y:0}. x is horizontal and y
 * is vertical.
 * @note Provide your own comparison and/or hash operators at the point of use for map
 * like containers. Do not define those here.
 */
struct Point {
    int x;
    int y;

    auto constexpr operator==(Point const& other) const -> bool = default;
    auto constexpr operator!=(Point const& other) const -> bool = default;

    [[nodiscard]] auto constexpr operator+(Point const& other) const -> Point
    {
        return Point{x + other.x, y + other.y};
    }

    [[nodiscard]] auto constexpr operator-(Point const& other) const -> Point
    {
        return Point{x - other.x, y - other.y};
    }

    [[nodiscard]] constexpr auto operator<(Point const& other) const -> bool
    {
        return std::tie(x, y) < std::tie(other.x, other.y);
    }
};

}  // namespace esc