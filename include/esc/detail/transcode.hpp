#pragma once

#include <array>
#include <string>
#include <string_view>

namespace esc::detail {

/**
 * Convert the given char32_t to its UTF8 reprensentation.
 * @param c The char32_t to convert.
 * @return The UTF8 bytes.
 */
[[nodiscard]] auto u32_to_u8(char32_t c) -> std::string;

/**
 * Convert the given char32_t string to its UTF8 reprensentation.
 * @param s The char32_t string to convert.
 * @return The UTF8 bytes.
 */
[[nodiscard]] auto u32_to_u8(std::u32string_view s) -> std::string;

/**
 * Convert a UTF8 array of bytes into a UTF32 char32_t.
 * @param bytes The UTF8 array of bytes to convert. Unused bytes should be null.
 * @return The char32_t bytes.
 */
[[nodiscard]] auto u8_to_u32(std::array<char, 4> bytes) -> char32_t;

}  // namespace esc::detail