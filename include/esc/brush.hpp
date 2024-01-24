#ifndef ESC_BRUSH_HPP
#define ESC_BRUSH_HPP
#include <esc/color.hpp>
#include <esc/default_color.hpp>
#include <esc/trait.hpp>

namespace esc {

/// Convenience type to hold visual aspects of the terminal display.
/** use set_brush() */
struct Brush {
    Color background = DefaultColor{};
    Color foreground = DefaultColor{};
    Traits traits    = Traits{Trait::None};
};

/// Returns true if the two Brushes have the same background, foreground, and
/// traits.
[[nodiscard]] inline auto operator==(Brush const& lhs, Brush const& rhs) -> bool
{
    return lhs.background == rhs.background &&
           lhs.foreground == rhs.foreground && lhs.traits == rhs.traits;
}

/// Returns true if the two Brushes do not have the same background, foreground,
/// and traits.
[[nodiscard]] inline auto operator!=(Brush const& lhs, Brush const& rhs) -> bool
{
    return !(lhs == rhs);
}

}  // namespace esc
#endif  // ESC_BRUSH_HPP
