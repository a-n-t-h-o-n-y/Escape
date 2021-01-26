#ifndef ESC_AREA_HPP
#define ESC_AREA_HPP
#include <cstddef>

namespace esc {

/// Holds a width and height as std::size_t objects.
struct Area {
    std::size_t width;
    std::size_t height;
};

/// Compares the dimension values, not calculated areas.
inline auto operator==(Area const& x, Area const& y) -> bool
{
    return x.width == y.width && x.height == y.height;
}

/// Compares the dimension values, not calculated areas.
inline auto operator!=(Area const& x, Area const& y) -> bool
{
    return !(x == y);
}

/// Compares calculated area values, not the individual dimensions.
inline auto operator<(Area const& x, Area const& y) -> bool
{
    return (x.width * x.height) < (y.width * y.height);
}

}  // namespace esc
#endif  // ESC_AREA_HPP
