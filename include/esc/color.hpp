#ifndef ESC_COLOR_HPP
#define ESC_COLOR_HPP
#include <variant>

#include <esc/color_index.hpp>
#include <esc/default_color.hpp>
#include <esc/true_color.hpp>

namespace esc {

/// Variant for any of the color types that can be used.
using Color = std::variant<Color_index, True_color, Default_color>;

}  // namespace esc
#endif  // ESC_COLOR_HPP
