#pragma once

#include <variant>

#include <esc/color_index.hpp>
#include <esc/default_color.hpp>
#include <esc/true_color.hpp>

namespace esc {

/// Variant for any of the color types that can be used.
using Color = std::variant<ColorIndex, TrueColor, DefaultColor>;

}  // namespace esc