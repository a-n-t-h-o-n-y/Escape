#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <esc/text.hpp>
#include <functional>
#include <limits>
#include <string_view>

namespace esc::detail {

// Scan printable ASCII eight bytes at a time, without alignment requirements or
// reads past the view. High bits detect non-ASCII; byte-wise subtraction/addition
// detect values below space or at DEL. Carries can only reject a word containing
// an out-of-range byte, which is then examined by the scalar tail.
[[nodiscard]] inline auto printable_ascii_prefix(std::string_view input) noexcept
    -> std::size_t
{
    constexpr std::uint64_t high_bits = 0x8080808080808080;
    constexpr std::uint64_t spaces = 0x2020202020202020;
    constexpr std::uint64_t ones = 0x0101010101010101;
    std::size_t offset = 0;
    while (input.size() - offset >= sizeof(std::uint64_t)) {
        std::uint64_t word;
        std::memcpy(&word, input.data() + offset, sizeof(word));
        if (((word | (word - spaces) | (word + ones)) & high_bits) != 0) break;
        offset += sizeof(word);
    }
    while (offset < input.size()) {
        auto byte = static_cast<unsigned char>(input[offset]);
        if (byte < 32 || byte >= 127) break;
        ++offset;
    }
    return offset;
}

// Detect a borrowed input pointing into existing owned storage before growth.
// std::less supplies a total ordering even for pointers to unrelated objects.
template <class T>
[[nodiscard]] auto points_into(void const* input, T const* storage, std::size_t count)
    -> bool
{
    auto const less = std::less<void const*>{};
    return count && !less(input, storage) && less(input, storage + count);
}

// Decode one complete scalar at a valid offset. On error, the offset identifies
// the initial byte of the ill-formed subsequence, including truncated prefixes.
[[nodiscard]] inline auto decode_utf8(std::string_view input, std::size_t& offset)
    -> char32_t
{
    auto const start = offset;
    auto const lead = static_cast<unsigned char>(input[start]);
    if (lead < 0x80) {
        ++offset;
        return lead;
    }
    unsigned width;
    char32_t scalar;
    char32_t minimum;
    if (lead >= 0xC2 && lead <= 0xDF) {
        width = 2;
        scalar = lead & 0x1F;
        minimum = 0x80;
    }
    else if (lead >= 0xE0 && lead <= 0xEF) {
        width = 3;
        scalar = lead & 0x0F;
        minimum = 0x800;
    }
    else if (lead >= 0xF0 && lead <= 0xF4) {
        width = 4;
        scalar = lead & 0x07;
        minimum = 0x10000;
    }
    else
        throw UnicodeError{start};
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
    for (std::size_t i = 0; i < input.size(); ++i)
        validate_scalar(input[i], i);
}

[[nodiscard]] inline auto utf8_byte_count(std::u32string_view input) -> std::size_t
{
    std::size_t count = 0;
    for (std::size_t i = 0; i < input.size(); ++i) {
        auto const scalar = input[i];
        validate_scalar(scalar, i);
        auto const width = scalar < 0x80      ? 1u
                           : scalar < 0x800   ? 2u
                           : scalar < 0x10000 ? 3u
                                              : 4u;
        if (width > (std::numeric_limits<std::size_t>::max)() - count)
            throw std::length_error{"Unicode: encoded size overflow"};
        count += width;
    }
    return count;
}

template <class Function>
void for_each_scalar(std::string_view input, Function&& function)
{
    for (std::size_t offset = 0; offset < input.size();)
        function(decode_utf8(input, offset));
}

template <class Function>
void for_each_scalar(std::u32string_view input, Function&& function)
{
    for (auto scalar : input)
        function(scalar);
}
}  // namespace esc::detail
