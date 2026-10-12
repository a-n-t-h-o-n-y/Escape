#pragma once

#include <concepts>
#include <type_traits>

#include <esc/detail/any_of.hpp>
#include <esc/color.hpp>
#include <esc/trait.hpp>

namespace esc {

/**
 * Holds traits and colors for text display on a terminal.
 */
struct Brush {
    Color background = TermColor::Default;
    Color foreground = TermColor::Default;
    Traits traits = Traits{Trait::None};

    [[nodiscard]] constexpr bool operator==(Brush const&) const = default;
    [[nodiscard]] constexpr bool operator!=(Brush const&) const = default;
};

template<class T>
concept Attribute = detail::AnyOf<std::remove_cvref_t<T>,
    ColorFG, ColorBG, Trait, Traits, RemoveTrait, Brush>;

namespace detail {
constexpr void validate_color(Color const& color)
{
    if (color.valueless_by_exception()) throw std::invalid_argument{"Style: valueless color"};
    if (auto const* value = std::get_if<TermColor>(&color); value && *value != TermColor::Default)
        throw std::invalid_argument{"Style: invalid default color"};
}
constexpr void validate_brush(Brush const& brush)
{
    validate_color(brush.background);
    validate_color(brush.foreground);
}
template<Attribute A>
constexpr void validate_attribute(A const& attribute)
{
    if constexpr (std::same_as<A, ColorFG> || std::same_as<A, ColorBG>) validate_color(attribute.value);
    else if constexpr (std::same_as<A, Trait>) static_cast<void>(Traits{attribute});
    else if constexpr (std::same_as<A, RemoveTrait>) static_cast<void>(Traits{attribute.trait});
    else if constexpr (std::same_as<A, Brush>) validate_brush(attribute);
}
// Validation is performed once before a bulk transformation, then this helper
// applies the checked attribute without repeating checks on every glyph.
template<Attribute A>
constexpr void apply_attribute(Brush& brush, A const& attribute)
{
    if constexpr (std::same_as<A, ColorFG>) brush.foreground = attribute.value;
    else if constexpr (std::same_as<A, ColorBG>) brush.background = attribute.value;
    else if constexpr (std::same_as<A, Trait> || std::same_as<A, Traits>) brush.traits.insert(attribute);
    else if constexpr (std::same_as<A, RemoveTrait>) brush.traits.remove(attribute.trait);
    else brush = attribute;
}
} // namespace detail

// Pure transformations preserve their inputs; named mutation belongs to glyph ranges.
template<Attribute A>
[[nodiscard]] constexpr auto operator|(Brush brush, A const& attribute) -> Brush
{
    detail::validate_brush(brush);
    detail::validate_attribute(attribute);
    detail::apply_attribute(brush, attribute);
    return brush;
}

} // namespace esc
