#ifndef ESC_COLORS_HPP
#define ESC_COLORS_HPP
#include <variant>

#include <esc/color_index.hpp>
#include <esc/true_color.hpp>

namespace esc {

/// Default Color of the terminal.
struct Default_color {};

/// Variant for the two types of color that can be used.
using Colors = std::variant<Color_index, True_color, Default_color>;

}  // namespace esc
#endif  // ESC_COLORS_HPP
