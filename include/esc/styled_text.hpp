#pragma once

#include <esc/detail/unicode.hpp>
#include <esc/style.hpp>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace esc {
// Composable values own their children. Views require an explicit borrowed API.
template <class... Values>
struct Sequence {
    std::tuple<Values...> values;
};
template <class Content, Attribute Appearance = StylePatch>
struct StyledText {
    Content content;
    Appearance appearance;
};
namespace detail {
template <class T>
struct IsSequence : std::false_type {};
template <class... T>
struct IsSequence<Sequence<T...>> : std::true_type {};
template <class T>
struct IsStyledText : std::false_type {};
template <class T, Attribute A>
struct IsStyledText<StyledText<T, A>> : std::true_type {};
template <class T>
concept ByteText = std::convertible_to<T const&, std::string_view>;
template <class T>
concept ScalarText = std::convertible_to<T const&, std::u32string_view>;

template <class T>
auto owned(T&& value)
{
    if constexpr (std::same_as<std::remove_cvref_t<T>, std::string> ||
                  std::same_as<std::remove_cvref_t<T>, std::u32string>)
        return std::remove_cvref_t<T>{std::forward<T>(value)};
    else if constexpr (ByteText<T>) {
        if constexpr (std::is_pointer_v<std::remove_cvref_t<T>>)
            if (!value) throw std::invalid_argument{"Text: null string"};
        return std::string{std::string_view{value}};
    }
    else if constexpr (ScalarText<T>) {
        if constexpr (std::is_pointer_v<std::remove_cvref_t<T>>)
            if (!value) throw std::invalid_argument{"Text: null string"};
        return std::u32string{std::u32string_view{value}};
    }
    else
        return std::remove_cvref_t<T>{std::forward<T>(value)};
}
}  // namespace detail

template <class... Values>
[[nodiscard]] auto sequence(Values&&... values)
{
    return Sequence<decltype(detail::owned(std::forward<Values>(values)))...>{
        {detail::owned(std::forward<Values>(values))...}};
}

template <class Content, Attribute A>
    requires(!std::ranges::range<Content> || detail::ByteText<Content> ||
             detail::ScalarText<Content>)
[[nodiscard]] auto styled(Content&& content, A const& appearance)
{
    detail::validate_attribute(appearance);
    auto value = detail::owned(std::forward<Content>(content));
    if constexpr (detail::ByteText<decltype(value)>)
        static_cast<void>(detail::utf8_scalar_count(value));
    else if constexpr (detail::ScalarText<decltype(value)>)
        detail::validate_utf32(value);
    if constexpr (std::same_as<A, Style>)
        return StyledText<decltype(value), Style>{std::move(value), appearance};
    else
        return StyledText<decltype(value)>{std::move(value), detail::patch(appearance)};
}

// Borrowing from an owning temporary is almost always an accidental dangling view.
template <class Char, class TraitsType, class Allocator, Attribute A>
auto styled_view(std::basic_string<Char, TraitsType, Allocator>&&, A const&) = delete;
template <class Char, class TraitsType, class Allocator, Attribute A>
auto styled_view(std::basic_string<Char, TraitsType, Allocator> const&&,
                 A const&) = delete;

// Explicit borrowing: the source must outlive every use of the returned value.
template <Attribute A>
[[nodiscard]] auto styled_view(std::string_view content, A const& appearance)
{
    static_cast<void>(detail::utf8_scalar_count(content));
    detail::validate_attribute(appearance);
    if constexpr (std::same_as<A, Style>)
        return StyledText<std::string_view, Style>{content, appearance};
    else
        return StyledText<std::string_view>{content, detail::patch(appearance)};
}
template <Attribute A>
[[nodiscard]] auto styled_view(std::u32string_view content, A const& appearance)
{
    detail::validate_utf32(content);
    detail::validate_attribute(appearance);
    if constexpr (std::same_as<A, Style>)
        return StyledText<std::u32string_view, Style>{content, appearance};
    else
        return StyledText<std::u32string_view>{content, detail::patch(appearance)};
}
template <Attribute A>
[[nodiscard]] auto operator|(std::string_view text, A const& appearance)
{ return styled(text, appearance); }
template <Attribute A>
[[nodiscard]] auto operator|(std::u32string_view text, A const& appearance)
{ return styled(text, appearance); }
template <class Content, Attribute A, Attribute B>
[[nodiscard]] auto operator|(StyledText<Content, A> text, B const& appearance)
{
    auto combined = text.appearance | appearance;
    return StyledText<Content, decltype(combined)>{std::move(text.content), combined};
}
}  // namespace esc
