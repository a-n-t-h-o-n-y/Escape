#ifndef ESC_POINT_HPP
#define ESC_POINT_HPP
#include <cstddef>
#include <functional>

namespace esc {

/// Represents a point on the Terminal in 2 dimensions.
/** Top-left cell of the terminal is Point{x:0, y:0}. x is horizontal and y is
 *  vertical. */
struct Point {
    int x;
    int y;
};

/// Return true if lhs and rhs have the same values for x and y.
[[nodiscard]] auto constexpr operator==(Point lhs, Point rhs) -> bool
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

/// Return false if lhs and rhs have the same values for x and y.
[[nodiscard]] auto constexpr operator!=(Point lhs, Point rhs) -> bool
{
    return !(lhs == rhs);
}

/// Only useful for total ordering, y is considered first, then x.
[[nodiscard]] auto constexpr operator<(Point lhs, Point rhs) -> bool
{
    return (lhs.y < rhs.y) || (lhs.y == rhs.y && lhs.x < rhs.x);
}

/// Adds x coordinates and y coordinates together.
[[nodiscard]] auto constexpr operator+(Point lhs, Point rhs) -> Point
{
    return {lhs.x + rhs.x, lhs.y + rhs.y};
}

/// Subtracts rhs coordinates from lhs coordinates.
[[nodiscard]] auto constexpr operator-(Point lhs, Point rhs) -> Point
{
    return {lhs.x - rhs.x, lhs.y - rhs.y};
}

}  // namespace esc

/// Custom specialization of std::hash for esc::Point.
namespace std {
template <>
struct hash<esc::Point> {
    using argument_type = esc::Point;
    using result_type   = std::size_t;
    auto operator()(argument_type const& point) const noexcept -> result_type;
};
}  // namespace std
#endif  // ESC_POINT_HPP
