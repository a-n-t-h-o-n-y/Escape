#pragma once

#include <cstddef>
#include <functional>
#include <limits>
#include <string_view>
#include <esc/text.hpp>

namespace esc::detail {

// Detect a borrowed input pointing into existing owned storage before growth.
// std::less supplies a total ordering even for pointers to unrelated objects.
template<class T>
[[nodiscard]] auto points_into(void const* input, T const* storage, std::size_t count) -> bool
{
    auto const less = std::less<void const*>{};
    return count && !less(input, storage) && less(input, storage + count);
}


// Decode one complete scalar at a valid offset. On error, the offset identifies
// the initial byte of the ill-formed subsequence, including truncated prefixes.
[[nodiscard]] inline auto decode_utf8(std::string_view input, std::size_t& offset) -> char32_t
{
    auto const start = offset;
    auto const lead = static_cast<unsigned char>(input[start]);
    if (lead < 0x80) { ++offset; return lead; }
    unsigned width;
    char32_t scalar;
    char32_t minimum;
    if (lead >= 0xC2 && lead <= 0xDF) { width = 2; scalar = lead & 0x1F; minimum = 0x80; }
    else if (lead >= 0xE0 && lead <= 0xEF) { width = 3; scalar = lead & 0x0F; minimum = 0x800; }
    else if (lead >= 0xF0 && lead <= 0xF4) { width = 4; scalar = lead & 0x07; minimum = 0x10000; }
    else throw UnicodeError{start};
    if (input.size() - start < width) throw UnicodeError{start};
    for (unsigned i = 1; i < width; ++i) {
        auto const byte = static_cast<unsigned char>(input[start + i]);
        if ((byte & 0xC0) != 0x80) throw UnicodeError{start};
        scalar = (scalar << 6) | (byte & 0x3F);
    }
    if (scalar < minimum) throw UnicodeError{start};
    validate_scalar(scalar, start);
    offset += width;
    return scalar;
}

[[nodiscard]] inline auto utf8_scalar_count(std::string_view input) -> std::size_t
{
    std::size_t count = 0;
    for (std::size_t offset = 0; offset < input.size(); ++count)
        static_cast<void>(decode_utf8(input, offset));
    return count;
}
inline void validate_utf32(std::u32string_view input)
{
    for (std::size_t i = 0; i < input.size(); ++i) validate_scalar(input[i], i);
}
[[nodiscard]] inline auto utf8_byte_count(std::u32string_view input) -> std::size_t
{
    std::size_t count = 0;
    for (std::size_t i = 0; i < input.size(); ++i) {
        auto const scalar = input[i];
        validate_scalar(scalar, i);
        auto const width = scalar < 0x80 ? 1u : scalar < 0x800 ? 2u : scalar < 0x10000 ? 3u : 4u;
        if (width > (std::numeric_limits<std::size_t>::max)() - count)
            throw std::length_error{"Unicode: encoded size overflow"};
        count += width;
    }
    return count;
}
template<class Function>
void for_each_scalar(std::string_view input, Function&& function)
{
    for (std::size_t offset = 0; offset < input.size();)
        function(decode_utf8(input, offset));
}
template<class Function>
void for_each_scalar(std::u32string_view input, Function&& function)
{
    for (auto scalar : input) function(scalar);
}
} // namespace esc::detail
