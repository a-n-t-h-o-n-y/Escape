#pragma once

#include <array>
#include <string>
#include <string_view>

#include <zzz/coro.hpp>

namespace esc::detail {

/**
 * Convert the given char32_t to its UTF8 reprensentation.
 * @details The input must be a valid UTF32 sequence, otherwise UB.
 * @param c The char32_t to convert.
 * @return The UTF8 bytes.
 */
[[nodiscard]]
auto u32_to_u8(char32_t c) -> std::string;

/**
 * Convert the given char32_t string to its UTF8 reprensentation.
 * @details The input must be a valid UTF32 sequence, otherwise UB.
 * @param s The char32_t string to convert.
 * @return The UTF8 bytes.
 */
[[nodiscard]]
auto u32_to_u8(std::u32string_view sv) -> std::string;

/**
 * Convert the given char32_t string to its UTF8 reprensentation.
 * @details The input must be a valid UTF32 sequence, otherwise UB.
 * @param s The char32_t string to convert.
 * @param out The string to write out the utf8 bytes to, it will be `clear()`ed.
 */
void u32_to_u8(std::u32string_view sv, std::string& out);

/**
 * Convert a UTF8 array of bytes into a UTF32 char32_t.
 * @param bytes The UTF8 array of bytes to convert. Unused bytes should be null.
 * @return The char32_t bytes.
 */
[[nodiscard]]
auto u8_to_u32(std::array<char, 4> bytes) -> char32_t;

/**
 * Convert a UTF8 string into a series of UTF32 char32_t.
 * @param sv The UTF8 string to convert.
 * @return A generator of UTF32 char32_t.
 */
[[nodiscard]]
auto u8_string_to_u32(std::string_view sv) -> zzz::Generator<char32_t>;

/**
 * Convert a UTF8 string to a UTF32 string.
 * @param sv The UTF8 string to convert.
 * @return A UTF32 std::u32string.
 */
[[nodiscard]]
auto u8_string_to_u32_string(std::string_view sv) -> std::u32string;

/**
 * Convert a UTF8 string to a UTF32 string.
 * @param sv The UTF8 string to convert.
 * @param out The u32string to write out the utf32 bytes to, it will be `clear()`ed.
 */
void u8_string_to_u32_string(std::string_view sv, std::u32string& out);

}  // namespace esc::detail