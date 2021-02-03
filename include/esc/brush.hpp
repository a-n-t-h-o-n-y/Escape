#ifndef ESC_BRUSH_HPP
#define ESC_BRUSH_HPP
#include <esc/color.hpp>
#include <esc/default_color.hpp>
#include <esc/trait.hpp>

namespace esc {

/// Convenience type to hold visual aspects of the terminal display.
/** use set_brush() */
struct Brush {
    Color background = Default_color{};
    Color foreground = Default_color{};
    Traits traits    = Traits{Trait::None};
};

}  // namespace esc
#endif  // ESC_BRUSH_HPP
