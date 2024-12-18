#pragma once

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <unicode/uchar.h>

#include <esc/brush.hpp>
#include <esc/color.hpp>
#include <esc/trait.hpp>

namespace esc {

/**
 * A single terminal cell display unit.
 *
 * @details Contains a symbol and a Brush to describe how the symbol should be
 * displayed.
 */
struct Glyph {
    char32_t symbol = U' ';
    Brush brush     = {};

    [[nodiscard]] constexpr bool operator==(Glyph const&) const = default;
    [[nodiscard]] constexpr bool operator!=(Glyph const&) const = default;
};

// ------------------------------ CONCEPTS -------------------------------------

/**
 * Defines the requirements for a type to be considered a Character.
 */
template <typename T>
concept Character =
    std::same_as<T, char> || std::same_as<T, signed char> ||
    std::same_as<T, unsigned char> || std::same_as<T, char8_t> ||
    std::same_as<T, char16_t> || std::same_as<T, char32_t>;

/**
 * Defines the requirements for a type to be considered an Attribute.
 */
template <typename T>
concept Attribute = std::same_as<T, ColorBG> || std::same_as<T, ColorFG> ||
                    std::same_as<T, Trait> || std::same_as<T, RemoveTrait> ||
                    std::same_as<T, Brush>;

/**
 * Defines the requirements for a type to be considered a GlyphString.
 */
template <typename T>
concept GlyphString = std::ranges::forward_range<T> &&
                      std::same_as<std::ranges::range_value_t<T>, Glyph>;

/**
 * Defines the requirements for a type that is a GlyphString and can have Glyphs
 * inserted into it.
 */
template <typename T>
concept InsertableGlyphString = GlyphString<T> && requires(T& t, Glyph g) {
    {
        *std::inserter(t, std::begin(t)) = g
    } -> std::same_as<std::insert_iterator<T>&>;
};

// ------------------------------ HELPERS --------------------------------------

namespace detail {

/**
 * Creates a std::vector<Glyph> from the given UTF8 string_view.
 *
 * @param sv The string_view to create Glyphs from.
 * @return A std::vector<Glyph> with each code point as a Glyph.
 * @throws std::runtime_error If the input string is not valid UTF-8.
 */
[[nodiscard]] inline auto utf8_to_glyphs(std::string_view sv)
    -> std::vector<Glyph>
{
    auto glyphs       = std::vector<Glyph>{};
    auto i            = std::int32_t{0};  // Index in the UTF-8 string
    auto const length = static_cast<std::int32_t>(sv.length());

    while (i < length) {
        UChar32 ch;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        U8_NEXT(sv.data(), i, length, ch);
#pragma GCC diagnostic pop
        if (ch < 0) {
            throw std::runtime_error{"Invalid UTF-8 sequence"};
        }
        glyphs.push_back({static_cast<char32_t>(ch)});
    }

    return glyphs;
}

/**
 * Creates a std::vector<Glyph> from the given UTF32 string_view.
 *
 * @param sv The string_view to create Glyphs from.
 * @return A std::vector<Glyph> with each code point as a Glyph.
 */
[[nodiscard]] inline auto utf32_to_glyphs(std::u32string_view sv)
    -> std::vector<Glyph>
{
    auto glyphs = std::vector<Glyph>{};
    glyphs.reserve(sv.length());
    std::ranges::transform(sv, std::back_inserter(glyphs),
                           [](auto ch) { return Glyph{.symbol = ch}; });
    return glyphs;
}

}  // namespace detail

// ------------------------------ PIPE OPS -------------------------------------
// ------------------------------ COLORBG --------------------------------------

/**
 * Creates a Glyph with the given symbol and background Color.
 *
 * @param symbol The symbol to display.
 * @param c The ColorBG to apply to the return Glyph's brush.
 * @return A Glyph with the given symbol and Color background.
 */
[[nodiscard]] constexpr auto operator|(Character auto symbol, ColorBG c)
    -> Glyph
{
    return {static_cast<char32_t>(symbol), {.background = c.value}};
}

/**
 * Copy the Glyph and modify its background Color.
 *
 * @param g The Glyph to work from.
 * @param c The ColorBG to apply to the return Glyph's brush.
 * @return The passed in Glyph with the given Color background.
 */
[[nodiscard]] constexpr auto operator|(Glyph g, ColorBG c) -> Glyph
{
    g.brush.background = c.value;
    return g;
}

/**
 * Modify the Glyphs in the GlyphString to have the given background Color.
 *
 * @param gs The GlyphString to work from.
 * @param c The ColorBG to apply to each Glyph's brush.
 * @return The passed in GlyphString with the given Color background.
 */
template <GlyphString T>
auto operator|(T&& gs, ColorBG c) -> decltype(auto)
{
    for (auto& glyph : gs) {
        glyph.brush.background = c.value;
    }
    return std::forward<T>(gs);
}

// ------------------------------ COLORFG --------------------------------------

/**
 * Creates a Glyph with the given symbol and foreground Color.
 *
 * @param symbol The symbol to display.
 * @param c The ColorFG to apply to the return Glyph's brush.
 * @return A Glyph with the given symbol and Color foreground.
 */
template <Character T>
[[nodiscard]] constexpr auto operator|(T symbol, ColorFG c) -> Glyph
{
    return {static_cast<char32_t>(symbol), {.foreground = c.value}};
}

/**
 * Copy the Glyph and modify its foreground Color.
 *
 * @param g The Glyph to work from.
 * @param c The ColorFG to apply to the return Glyph's brush.
 * @return The passed in Glyph with the given Color foreground.
 */
[[nodiscard]] constexpr auto operator|(Glyph g, ColorFG c) -> Glyph
{
    g.brush.foreground = c.value;
    return g;
}

/**
 * Modify the Glyphs in the GlyphString to have the given foreground Color.
 *
 * @param gs The GlyphString to work from.
 * @param c The ColorFG to apply to each Glyph's brush.
 * @return The passed in GlyphString with the given Color foreground.
 */
template <GlyphString T>
auto operator|(T&& gs, ColorFG c) -> decltype(auto)
{
    for (auto& glyph : gs) {
        glyph.brush.foreground = c.value;
    }
    return std::forward<T>(gs);
}

// ------------------------------- TRAIT ---------------------------------------

/**
 * Creates a Glyph with the given symbol and Trait.
 *
 * @param symbol The symbol to display.
 * @param t The Trait to apply to the return Glyph's brush.
 * @return A Glyph with the given symbol and Trait.
 */
[[nodiscard]] constexpr auto operator|(Character auto symbol, Trait t) -> Glyph
{
    return {static_cast<char32_t>(symbol), {.traits = t}};
}

/**
 * Copy the Glyph and add a Trait.
 *
 * @param g The Glyph to work from.
 * @param t The Trait to apply to the return Glyph's brush.
 * @return The passed in Glyph with the given Trait added.
 */
[[nodiscard]] constexpr auto operator|(Glyph g, Trait t) -> Glyph
{
    g.brush.traits = g.brush.traits | t;
    return g;
}

/**
 * Modify the Glyphs in the GlyphString to have the given Trait.
 *
 * @param gs The GlyphString to work from.
 * @param t The Trait to apply to each Glyph's brush.
 * @return The passed in GlyphString with the given Trait.
 */
template <GlyphString T>
auto operator|(T&& gs, Trait t) -> decltype(auto)
{
    for (auto& glyph : gs) {
        glyph.brush.traits = glyph.brush.traits | t;
    }
    return std::forward<T>(gs);
}

// ---------------------------- REMOVE TRAIT -----------------------------------

/**
 * Removes a Trait from a Glyph's brush.
 *
 * @details Use the remove_trait(Trait) function to create the RemoveTrait
 * object.
 *
 * @param g The Glyph to remove the Trait from.
 * @param t The Trait to remove from the Glyph's brush.
 * @return The Glyph with the Trait removed from its brush.
 */
[[nodiscard]] constexpr auto operator|(Glyph g, RemoveTrait t) -> Glyph
{
    g.brush.traits = g.brush.traits | t;
    return g;
}

/**
 * Modify the Glyphs in the GlyphString to remove the given Trait.
 *
 * @param gs The GlyphString to work from.
 * @param c The Trait to remove from each Glyph's brush.
 * @return The passed in GlyphString with the given Trait removed.
 */
template <GlyphString T>
auto operator|(T&& gs, RemoveTrait t) -> decltype(auto)
{
    for (auto& glyph : gs) {
        glyph.brush.traits = glyph.brush.traits | t;
    }
    return std::forward<T>(gs);
}

// -------------------------------- BRUSH --------------------------------------

/**
 * Creates a Glyph with the given symbol and Brush.
 *
 * @param symbol The symbol to display.
 * @param b The Brush to apply to the return Glyph.
 * @return A Glyph with the given symbol and Brush.
 */
[[nodiscard]] constexpr auto operator|(Character auto symbol, Brush b) -> Glyph
{
    return {.symbol = static_cast<char32_t>(symbol), .brush = b};
}

/**
 * Copy the Glyph and replace its Brush.
 *
 * @param g The Glyph to work from.
 * @param b The Brush to apply to the return Glyph.
 * @return The passed in Glyph with the given Brush.
 */
[[nodiscard]] constexpr auto operator|(Glyph g, Brush b) -> Glyph
{
    return {.symbol = g.symbol, .brush = b};
}

/**
 * Modify the Glyphs in the GlyphString to have the given Brush.
 *
 * @param gs The GlyphString to work from.
 * @param b The Brush to apply to each Glyph.
 * @return The passed in GlyphString with the given Trait.
 */
template <GlyphString T>
auto operator|(T&& gs, Brush b) -> decltype(auto)
{
    for (auto& glyph : gs) {
        glyph.brush = b;
    }
    return std::forward<T>(gs);
}

// ------------------------------- STRINGS -------------------------------------

/**
 * Creates a std::vector<Glyph> from the given u32string_view and Attribute.
 *
 * @param sv The string_view to create Glyphs from.
 * @param attr The Attribute to apply to each Glyph's brush.
 * @return A std::vector<Glyph> with the given Attribute applied to each Glyph.
 */
template <Attribute T>
[[nodiscard]] auto operator|(std::u32string_view sv, T attr)
    -> std::vector<Glyph>
{
    return detail::utf32_to_glyphs(sv) | attr;
}

/**
 * Creates a std::vector<Glyph> from the given string_view and Attribute.
 *
 * @details UTF-8 encoding of the input string is assumed.
 * @param sv The string_view to create Glyphs from.
 * @param attr The Attribute to apply to each Glyph's brush.
 * @return A std::vector<Glyph> with the given Attribute applied to each Glyph.
 * @throws std::runtime_error If the input string is not valid UTF-8.
 */
template <Attribute T>
[[nodiscard]] auto operator|(std::string_view sv, T attr) -> std::vector<Glyph>
{
    return detail::utf8_to_glyphs(sv) | attr;
}

// ----------------------------- OPERATOR+= ------------------------------------

template <InsertableGlyphString T, GlyphString U>
auto operator+=(T& lhs, U const& rhs) -> T&
{
    lhs.reserve(lhs.size() + rhs.size());
    std::ranges::copy(rhs, std::inserter(lhs, std::end(lhs)));
    return lhs;
}

template <InsertableGlyphString T>
auto operator+=(T& lhs, std::u32string_view rhs) -> T&
{
    lhs.reserve(lhs.size() + rhs.size());
    std::ranges::transform(rhs, std::inserter(lhs, std::end(lhs)),
                           [](auto ch) { return Glyph{.symbol = ch}; });
    return lhs;
}

template <InsertableGlyphString T>
auto operator+=(T& lhs, std::string_view rhs) -> T&
{
    auto i            = std::int32_t{0};  // Index in the UTF-8 string
    auto const length = static_cast<std::int32_t>(rhs.length());

    auto inserter = std::inserter(lhs, std::end(lhs));
    while (i < length) {
        UChar32 ch;
        U8_NEXT(rhs.data(), i, length, ch);
        if (ch < 0) {
            throw std::runtime_error{"Invalid UTF-8 sequence"};
        }
        *inserter = Glyph{.symbol = static_cast<char32_t>(ch)};
    }

    return lhs;
}

template <InsertableGlyphString T>
auto operator+=(T& lhs, Glyph const& rhs) -> T&
{
    lhs.insert(std::end(lhs), rhs);
    return lhs;
}

template <InsertableGlyphString T, Character U>
auto operator+=(T& lhs, U rhs) -> T&
{
    lhs.insert(std::end(lhs), {.symbol = static_cast<char32_t>(rhs)});
    return lhs;
}

// ----------------------------- OPERATOR+ -------------------------------------

[[nodiscard]] inline auto operator+(Glyph const& lhs, Glyph const& rhs)
    -> std::vector<Glyph>
{
    return {lhs, rhs};
}

template <Character T>
[[nodiscard]] auto operator+(Glyph const& lhs, T rhs) -> std::vector<Glyph>
{
    return {lhs, {.symbol = static_cast<char32_t>(rhs)}};
}

template <Character T>
[[nodiscard]] auto operator+(T lhs, Glyph const& rhs) -> std::vector<Glyph>
{
    return {{.symbol = static_cast<char32_t>(lhs)}, rhs};
}

template <InsertableGlyphString T>
[[nodiscard]] auto operator+(T lhs, std::string_view rhs) -> T
{
    return lhs += rhs;
}

template <InsertableGlyphString T>
[[nodiscard]] auto operator+(std::string_view lhs, T rhs) -> T
{
    auto i            = std::int32_t{0};  // Index in the UTF-8 string
    auto const length = static_cast<std::int32_t>(lhs.length());

    auto inserter = std::inserter(rhs, std::begin(rhs));
    while (i < length) {
        UChar32 ch;
        U8_NEXT(lhs.data(), i, length, ch);
        if (ch < 0) {
            throw std::runtime_error{"Invalid UTF-8 sequence"};
        }
        *inserter = Glyph{.symbol = static_cast<char32_t>(ch)};
    }

    return rhs;
}

template <InsertableGlyphString T>
[[nodiscard]] auto operator+(T lhs, std::u32string_view rhs) -> T
{
    return lhs += rhs;
}

template <InsertableGlyphString T>
[[nodiscard]] auto operator+(std::u32string_view lhs, T rhs) -> T
{
    rhs.reserve(rhs.size() + lhs.size());
    std::ranges::transform(lhs, std::inserter(rhs, std::begin(rhs)),
                           [](auto ch) { return Glyph{.symbol = ch}; });
    return rhs;
}

template <InsertableGlyphString T, GlyphString U>
[[nodiscard]] auto operator+(T lhs, U const& rhs) -> T
{
    lhs.reserve(lhs.size() + rhs.size());
    std::ranges::copy(rhs, std::inserter(lhs, std::end(lhs)));
    return lhs;
}

template <InsertableGlyphString T>
[[nodiscard]] auto operator+(T lhs, Glyph const& rhs) -> T
{
    lhs.insert(std::end(lhs), rhs);
    return lhs;
}

template <InsertableGlyphString T, Character U>
[[nodiscard]] auto operator+(T lhs, U rhs) -> T
{
    lhs.insert(std::end(lhs), Glyph{.symbol = static_cast<char32_t>(rhs)});
    return lhs;
}

template <InsertableGlyphString T>
[[nodiscard]] auto operator+(Glyph const& lhs, T rhs) -> T
{
    rhs.insert(std::begin(rhs), lhs);
    return rhs;
}

template <InsertableGlyphString T, Character U>
[[nodiscard]] auto operator+(U lhs, T rhs) -> T
{
    rhs.insert(std::begin(rhs), Glyph{.symbol = static_cast<char32_t>(lhs)});
    return rhs;
}

[[nodiscard]] inline auto operator+(Glyph const& lhs, std::string_view rhs)
    -> std::vector<Glyph>
{
    return lhs + detail::utf8_to_glyphs(rhs);
}

[[nodiscard]] inline auto operator+(Glyph const& lhs, std::u32string_view rhs)
    -> std::vector<Glyph>
{
    return lhs + detail::utf32_to_glyphs(rhs);
}

[[nodiscard]] inline auto operator+(std::string_view lhs, Glyph const& rhs)
    -> std::vector<Glyph>
{
    return detail::utf8_to_glyphs(lhs) + rhs;
}

[[nodiscard]] inline auto operator+(std::u32string_view lhs, Glyph const& rhs)
    -> std::vector<Glyph>
{
    return detail::utf32_to_glyphs(lhs) + rhs;
}

}  // namespace esc