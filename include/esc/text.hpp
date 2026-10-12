#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace esc {

class UnicodeError : public std::runtime_error {
public:
    explicit UnicodeError(std::size_t offset)
        : std::runtime_error{"Invalid Unicode encoding"}, offset_{offset} {}
    [[nodiscard]] auto offset() const noexcept -> std::size_t { return offset_; }
private:
    std::size_t offset_;
};

// Strict conversion without normalization. Append operations preserve contents
// on invalid input, size overflow, or allocation failure.
[[nodiscard]] auto utf8_to_utf32(std::string_view input) -> std::u32string;
void append_utf32(std::u32string& output, std::string_view utf8);
[[nodiscard]] auto utf32_to_utf8(std::u32string_view input) -> std::string;
void append_utf8(std::string& output, std::u32string_view utf32);

struct Utf8CodePoint {
    std::array<char, 4> bytes{};
    std::uint8_t size = 1;
    [[nodiscard]] constexpr auto view() const noexcept -> std::string_view { return {bytes.data(), size}; }
};

namespace detail {
constexpr void validate_scalar(char32_t scalar, std::size_t offset)
{
    if (scalar > 0x10FFFF || (scalar >= 0xD800 && scalar <= 0xDFFF))
        throw UnicodeError{offset};
}
} // namespace detail

// One owned scalar encoding; unused bytes are zero and no allocation occurs.
[[nodiscard]] constexpr auto encode_utf8(char32_t scalar) -> Utf8CodePoint
{
    detail::validate_scalar(scalar, 0);
    Utf8CodePoint result;
    if (scalar < 0x80) {
        result.bytes[0] = static_cast<char>(scalar);
    } else if (scalar < 0x800) {
        result.size = 2;
        result.bytes[0] = static_cast<char>(0xC0 | (scalar >> 6));
        result.bytes[1] = static_cast<char>(0x80 | (scalar & 0x3F));
    } else if (scalar < 0x10000) {
        result.size = 3;
        result.bytes[0] = static_cast<char>(0xE0 | (scalar >> 12));
        result.bytes[1] = static_cast<char>(0x80 | ((scalar >> 6) & 0x3F));
        result.bytes[2] = static_cast<char>(0x80 | (scalar & 0x3F));
    } else {
        result.size = 4;
        result.bytes[0] = static_cast<char>(0xF0 | (scalar >> 18));
        result.bytes[1] = static_cast<char>(0x80 | ((scalar >> 12) & 0x3F));
        result.bytes[2] = static_cast<char>(0x80 | ((scalar >> 6) & 0x3F));
        result.bytes[3] = static_cast<char>(0x80 | (scalar & 0x3F));
    }
    return result;
}
} // namespace esc
