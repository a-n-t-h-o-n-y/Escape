#pragma once

#include <concepts>
#include <esc/color.hpp>
#include <esc/detail/any_of.hpp>
#include <esc/trait.hpp>
#include <optional>
#include <type_traits>

namespace esc {
// A complete appearance. Applying it replaces colors and modeled traits.
struct Style {
    Color background = default_color;
    Color foreground = default_color;
    Traits traits = {};
    [[nodiscard]] constexpr bool operator==(Style const&) const = default;
};

// A partial change. Absence preserves a component; default color resets it.
struct StylePatch {
    std::optional<Color> background;
    std::optional<Color> foreground;
    Traits enabled;
    Traits disabled;
    [[nodiscard]] constexpr bool operator==(StylePatch const&) const = default;
};

template <class T>
concept Attribute = detail::AnyOf<std::remove_cvref_t<T>,
                                  ColorFG,
                                  ColorBG,
                                  Trait,
                                  Traits,
                                  WithoutTraits,
                                  StylePatch,
                                  Style>;

template <class T>
concept PatchAttribute = Attribute<T> && !std::same_as<std::remove_cvref_t<T>, Style>;

namespace detail {
constexpr void validate_color(Color const& color)
{
    if (color.valueless_by_exception())
        throw std::invalid_argument{"Style: valueless color"};
}

constexpr void validate_style(Style const& style)
{
    validate_color(style.background);
    validate_color(style.foreground);
}

template <Attribute A>
constexpr void validate_attribute(A const& attribute)
{
    using T = std::remove_cvref_t<A>;
    if constexpr (std::same_as<T, ColorFG> || std::same_as<T, ColorBG>)
        validate_color(attribute.value);
    else if constexpr (std::same_as<T, Trait>)
        static_cast<void>(Traits{attribute});
    else if constexpr (std::same_as<T, Style>)
        validate_style(attribute);
    else if constexpr (std::same_as<T, StylePatch>) {
        if (attribute.background) validate_color(*attribute.background);
        if (attribute.foreground) validate_color(*attribute.foreground);
        if (attribute.enabled.data() & attribute.disabled.data())
            throw std::invalid_argument{
                "StylePatch: a trait cannot be both enabled and disabled"};
    }
}

template <PatchAttribute A>
[[nodiscard]] constexpr auto patch(A const& attribute) -> StylePatch
{
    validate_attribute(attribute);
    using T = std::remove_cvref_t<A>;
    StylePatch result;
    if constexpr (std::same_as<T, ColorFG>)
        result.foreground = attribute.value;
    else if constexpr (std::same_as<T, ColorBG>)
        result.background = attribute.value;
    else if constexpr (std::same_as<T, Trait> || std::same_as<T, Traits>)
        result.enabled = attribute;
    else if constexpr (std::same_as<T, WithoutTraits>)
        result.disabled = attribute.traits;
    else
        result = attribute;
    return result;
}

constexpr void apply_attribute(Style& style, StylePatch const& patch)
{
    if (patch.background) style.background = *patch.background;
    if (patch.foreground) style.foreground = *patch.foreground;
    style.traits.remove(patch.disabled).insert(patch.enabled);
}

template <Attribute A>
constexpr void apply_attribute(Style& style, A const& attribute)
{
    if constexpr (std::same_as<std::remove_cvref_t<A>, Style>)
        style = attribute;
    else
        apply_attribute(style, patch(attribute));
}
}  // namespace detail

template <Attribute A>
[[nodiscard]] constexpr auto operator|(Style style, A const& attribute) -> Style
{
    detail::validate_style(style);
    detail::validate_attribute(attribute);
    detail::apply_attribute(style, attribute);
    return style;
}

template <PatchAttribute A, PatchAttribute B>
[[nodiscard]] constexpr auto operator|(A const& first, B const& second) -> StylePatch
{
    auto left = detail::patch(first), right = detail::patch(second);
    if (right.background) left.background = right.background;
    if (right.foreground) left.foreground = right.foreground;
    left.enabled.remove(right.disabled).insert(right.enabled);
    left.disabled.remove(right.enabled).insert(right.disabled);
    return left;
}

template <PatchAttribute A>
[[nodiscard]] constexpr auto operator|(A const& first, Style const& replacement)
    -> Style
{
    detail::validate_attribute(first);
    detail::validate_style(replacement);
    return replacement;
}
}  // namespace esc
