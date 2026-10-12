#pragma once

#include <concepts>
#include <functional>
#include <iterator>
#include <ranges>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <esc/style.hpp>
#include <esc/detail/unicode.hpp>

namespace esc {

// A styled Unicode scalar, without an implied terminal cell width.
struct Glyph {
    char32_t symbol = U' ';
    Style style = {};
    [[nodiscard]] constexpr auto operator==(Glyph const&) const -> bool = default;
};
using GlyphString = std::vector<Glyph>;

template<class T>
concept GlyphRange = std::ranges::forward_range<T> &&
    std::same_as<std::ranges::range_value_t<T>, Glyph>;
template<class T>
concept Character = detail::AnyOf<std::remove_cvref_t<T>, char, signed char, unsigned char, char32_t>;

[[nodiscard]] auto glyphs(std::string_view utf8, Style style = {}) -> GlyphString;
[[nodiscard]] auto glyphs(std::u32string_view utf32, Style style = {}) -> GlyphString;
void append_glyphs(GlyphString& output, std::string_view utf8, Style style = {});
void append_glyphs(GlyphString& output, std::u32string_view utf32, Style style = {});

namespace detail {
constexpr void validate_glyph(Glyph const& glyph, std::size_t offset)
{
    validate_scalar(glyph.symbol, offset);
    validate_style(glyph.style);
}
[[nodiscard]] constexpr auto character_scalar(Character auto character) -> char32_t
{
    if constexpr (std::same_as<decltype(character), char32_t>) {
        validate_scalar(character, 0);
        return character;
    } else {
        auto const byte = static_cast<unsigned char>(character);
        if (byte > 127) throw std::invalid_argument{"Glyph: a narrow character must be ASCII"};
        return byte;
    }
}
[[nodiscard]] inline auto text_count(std::string_view input) -> std::size_t { return utf8_scalar_count(input); }
[[nodiscard]] inline auto text_count(std::u32string_view input) -> std::size_t
{ validate_utf32(input); return input.size(); }
[[nodiscard]] inline auto appended_size(GlyphString const& output, std::size_t count) -> std::size_t
{
    if (count > output.max_size() - output.size())
        throw std::length_error{"Glyph: output exceeds vector size limit"};
    return output.size() + count;
}
template<class Range> requires GlyphRange<Range const>
[[nodiscard]] auto validated_glyph_count(Range const& input) -> std::size_t
{
    std::size_t count = 0;
    for (auto const& glyph : input) validate_glyph(glyph, count++);
    return count;
}
} // namespace detail

// No intermediate container. Input and style are validated before any insertion;
// insertion failures are allowed to leave the caller's destination partially filled.
template<std::output_iterator<Glyph> Output>
auto append_glyphs_to(Output output, std::string_view utf8, Style style = {}) -> Output
{
    detail::validate_style(style);
    static_cast<void>(detail::text_count(utf8));
    detail::for_each_scalar(utf8, [&](char32_t scalar) { *output++ = Glyph{scalar, style}; });
    return output;
}
template<std::output_iterator<Glyph> Output>
auto append_glyphs_to(Output output, std::u32string_view utf32, Style style = {}) -> Output
{
    detail::validate_style(style);
    detail::validate_utf32(utf32);
    for (auto scalar : utf32) *output++ = Glyph{scalar, style};
    return output;
}

template<Attribute A>
[[nodiscard]] constexpr auto operator|(Glyph glyph, A const& attribute) -> Glyph
{
    detail::validate_glyph(glyph, 0);
    detail::validate_attribute(attribute);
    detail::apply_attribute(glyph.style, attribute);
    return glyph;
}
template<Character C, Attribute A>
[[nodiscard]] constexpr auto operator|(C character, A const& attribute) -> Glyph
{ return Glyph{detail::character_scalar(character)} | attribute; }

template<class Range, Attribute A>
    requires GlyphRange<Range> && std::same_as<std::ranges::range_reference_t<Range>, Glyph&>
void style_in_place(Range& input, A const& attribute)
{
    detail::validate_attribute(attribute);
    std::size_t offset = 0;
    for (auto const& glyph : input) detail::validate_glyph(glyph, offset++);
    for (auto& glyph : input) detail::apply_attribute(glyph.style, attribute);
}
template<class Range, Attribute A> requires GlyphRange<Range const>
[[nodiscard]] auto styled(Range const& input, A const& attribute) -> GlyphString
{
    detail::validate_attribute(attribute);
    auto const count = detail::validated_glyph_count(input);
    GlyphString result;
    result.reserve(count);
    for (auto const& value : input) {
        Glyph glyph = value;
        detail::apply_attribute(glyph.style, attribute);
        result.push_back(std::move(glyph));
    }
    return result;
}
template<class Range, Attribute A> requires GlyphRange<Range const>
[[nodiscard]] auto operator|(Range const& input, A const& attribute) -> GlyphString
{ return styled(input, attribute); }
template<Attribute A>
[[nodiscard]] auto operator|(GlyphString&& input, A const& attribute) -> GlyphString
{ style_in_place(input, attribute); return std::move(input); }
// Contiguous sources need only the destination allocation, including self-append
// and overlapping spans. General forward ranges are snapshotted before mutation:
// their iterators or lazy projections may depend on the destination's storage.
template<class Range> requires GlyphRange<Range const>
auto operator+=(GlyphString& output, Range const& input) -> GlyphString&
{
    auto const count = detail::validated_glyph_count(input);
    if (!count) return output;
    auto const total = detail::appended_size(output, count);
    if constexpr (std::ranges::contiguous_range<Range const>) {
        auto const* source = std::ranges::data(input);
        std::size_t alias_offset = 0;
        auto const less = std::less<Glyph const*>{};
        bool const aliases = !output.empty() && !less(source, output.data()) &&
                             less(source, output.data() + output.size());
        if (aliases) alias_offset = static_cast<std::size_t>(source - output.data());
        output.reserve(total);
        if (aliases) source = output.data() + alias_offset;
        for (std::size_t i = 0; i < count; ++i) output.push_back(source[i]);
    } else {
        GlyphString snapshot;
        snapshot.reserve(count);
        for (auto const& glyph : input) snapshot.push_back(glyph);
        output.reserve(total);
        for (auto const& glyph : snapshot) output.push_back(glyph);
    }
    return output;
}
inline auto operator+=(GlyphString& output, Glyph glyph) -> GlyphString&
{ detail::validate_glyph(glyph, 0); output.push_back(std::move(glyph)); return output; }
inline auto operator+=(GlyphString& output, Character auto character) -> GlyphString&
{ return output += Glyph{detail::character_scalar(character)}; }
inline auto operator+=(GlyphString& output, std::string_view utf8) -> GlyphString&
{ append_glyphs(output, utf8); return output; }
inline auto operator+=(GlyphString& output, std::u32string_view utf32) -> GlyphString&
{ append_glyphs(output, utf32); return output; }

namespace detail {
template<class T>
concept GlyphOperand = Character<T> || AnyOf<std::remove_cvref_t<T>, Glyph, GlyphString> ||
    std::convertible_to<T const&, std::string_view> || std::convertible_to<T const&, std::u32string_view>;
template<GlyphOperand T>
[[nodiscard]] auto operand_count(T const& input) -> std::size_t
{
    if constexpr (Character<T>) { static_cast<void>(character_scalar(input)); return 1; }
    else if constexpr (std::same_as<T, Glyph>) { validate_glyph(input, 0); return 1; }
    else if constexpr (std::same_as<T, GlyphString>) return validated_glyph_count(input);
    else if constexpr (std::convertible_to<T const&, std::string_view>) return text_count(std::string_view{input});
    else return text_count(std::u32string_view{input});
}
template<GlyphOperand T>
void append_operand(GlyphString& output, T const& input)
{
    if constexpr (Character<T>) output.push_back(Glyph{character_scalar(input)});
    else if constexpr (std::same_as<T, Glyph>) output.push_back(input);
    else if constexpr (std::same_as<T, GlyphString>)
        for (auto const& glyph : input) output.push_back(glyph);
    else if constexpr (std::convertible_to<T const&, std::string_view>)
        for_each_scalar(std::string_view{input}, [&](char32_t scalar) { output.push_back(Glyph{scalar}); });
    else
        for (auto scalar : std::u32string_view{input}) output.push_back(Glyph{scalar});
}
} // namespace detail

// Restrict operators to the owning vector and glyph values. Named APIs/ranges
// provide customization without adding operators to arbitrary standard containers.
template<detail::GlyphOperand Left, detail::GlyphOperand Right>
    requires (detail::AnyOf<Left, Glyph, GlyphString> || detail::AnyOf<Right, Glyph, GlyphString>)
[[nodiscard]] auto operator+(Left const& left, Right const& right) -> GlyphString
{
    auto const left_count = detail::operand_count(left);
    auto const right_count = detail::operand_count(right);
    GlyphString result;
    if (left_count > result.max_size() || right_count > result.max_size() - left_count)
        throw std::length_error{"Glyph: concatenation exceeds vector size limit"};
    result.reserve(left_count + right_count);
    detail::append_operand(result, left);
    detail::append_operand(result, right);
    return result;
}
} // namespace esc
