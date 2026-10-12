#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace esc {

/// Invalid Unicode with a byte offset for UTF-8 or scalar offset for UTF-32.
class UnicodeError : public std::runtime_error {
   public:
    explicit UnicodeError(std::size_t offset)
        : std::runtime_error{"Invalid Unicode encoding"}, offset_{offset}
    {}

    /// Read the failing input offset.
    [[nodiscard]] auto offset() const noexcept -> std::size_t { return offset_; }

   private:
    std::size_t offset_;
};

// Strict conversion without normalization. Append operations preserve contents
// on invalid input, size overflow, or allocation failure.
/// Strict UTF-8 to UTF-32 conversion without normalization.
[[nodiscard]] auto utf8_to_utf32(std::string_view input) -> std::u32string;

/// Append validated UTF-8 as scalars; preserve the destination on failure.
void append_utf32(std::u32string& output, std::string_view utf8);

/// Strict UTF-32 to UTF-8 conversion without normalization.
[[nodiscard]] auto utf32_to_utf8(std::u32string_view input) -> std::string;

/// Append validated scalars as UTF-8; preserve the destination on failure.
void append_utf8(std::string& output, std::u32string_view utf32);

/// One encoded Unicode scalar in up to four bytes.
struct Utf8CodePoint {
    std::array<char, 4> bytes{};
    std::uint8_t size = 1;

    /// Borrow the used bytes; the code-point value must remain alive.
    [[nodiscard]] constexpr auto view() const noexcept -> std::string_view
    { return {bytes.data(), size}; }
};

namespace detail {
constexpr void validate_scalar(char32_t scalar, std::size_t offset)
{
    if (scalar > 0x10FFFF || (scalar >= 0xD800 && scalar <= 0xDFFF))
        throw UnicodeError{offset};
}

// Encode a previously validated scalar into at least four writable bytes (or
// its known encoded length). Internal callers validate the complete input before
// any mutation/I/O, so this pass needs neither another check nor a temporary.
constexpr auto encode_utf8_to(char* bytes, char32_t scalar) noexcept -> std::uint8_t
{
    if (scalar < 0x80) {
        bytes[0] = static_cast<char>(scalar);
        return 1;
    }
    if (scalar < 0x800) {
        bytes[0] = static_cast<char>(0xC0 | (scalar >> 6));
        bytes[1] = static_cast<char>(0x80 | (scalar & 0x3F));
        return 2;
    }
    if (scalar < 0x10000) {
        bytes[0] = static_cast<char>(0xE0 | (scalar >> 12));
        bytes[1] = static_cast<char>(0x80 | ((scalar >> 6) & 0x3F));
        bytes[2] = static_cast<char>(0x80 | (scalar & 0x3F));
        return 3;
    }
    bytes[0] = static_cast<char>(0xF0 | (scalar >> 18));
    bytes[1] = static_cast<char>(0x80 | ((scalar >> 12) & 0x3F));
    bytes[2] = static_cast<char>(0x80 | ((scalar >> 6) & 0x3F));
    bytes[3] = static_cast<char>(0x80 | (scalar & 0x3F));
    return 4;
}
}  // namespace detail

// One owned scalar encoding; unused bytes are zero and no allocation occurs.
/// Validate and encode one scalar into fixed-size storage.
[[nodiscard]] constexpr auto encode_utf8(char32_t scalar) -> Utf8CodePoint
{
    detail::validate_scalar(scalar, 0);
    Utf8CodePoint result;
    result.size = detail::encode_utf8_to(result.bytes.data(), scalar);
    return result;
}
}  // namespace esc
