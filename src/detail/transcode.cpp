#include <esc/detail/transcode.hpp>

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

#include <unicode/stringpiece.h>
#include <unicode/uchar.h>
#include <unicode/unistr.h>

#include <zzz/coro.hpp>

namespace esc::detail {

auto u32_to_u8(char32_t c) -> std::string
{
    auto buffer = std::array<char, U8_MAX_LENGTH>{};  // UTF-8 max length is 4 bytes
    auto i = 0;
    U8_APPEND_UNSAFE(buffer.data(), i, static_cast<UChar32>(c));  // ICU macro
    return std::string{buffer.data(), static_cast<std::size_t>(i)};
}

auto u32_to_u8(std::u32string_view sv) -> std::string
{
    auto result = std::string{};
    u32_to_u8(sv, result);
    return result;
}

void u32_to_u8(std::u32string_view sv, std::string& out)
{
    out.clear();
    out.reserve(sv.size());  // Common case every char is 1 byte.

    for (auto c : sv) {
        char utf8_buffer[U8_MAX_LENGTH];
        auto i = 0;
        U8_APPEND_UNSAFE(utf8_buffer, i, static_cast<UChar32>(c));
        out.append(utf8_buffer, i);
    }
}

auto u8_to_u32(std::array<char, 4> bytes) -> char32_t
{
    auto u_str = icu::UnicodeString::fromUTF8(icu::StringPiece{
        bytes.data(),
        bytes.size(),
    });
    return u_str.char32At(0);
}

auto u8_string_to_u32(std::string_view sv) -> zzz::Generator<char32_t>
{
    auto i = std::int32_t{0};  // Index in the UTF-8 string
    auto const length = static_cast<std::int32_t>(sv.length());

    while (i < length) {
        UChar32 ch;
        U8_NEXT(sv.data(), i, length, ch);
        if (ch < 0) { throw std::runtime_error{"Invalid UTF-8 sequence"}; }
        co_yield static_cast<char32_t>(ch);
    }
}

[[nodiscard]]
auto u8_string_to_u32_string(std::string_view sv) -> std::u32string
{
    auto result = std::u32string{};
    u8_string_to_u32_string(sv, result);
    return result;
}

void u8_string_to_u32_string(std::string_view sv, std::u32string& out)
{
    auto i = std::int32_t{0};  // Index in the UTF-8 string
    auto const length = static_cast<std::int32_t>(sv.length());

    out.clear();
    out.reserve(sv.length());
    while (i < length) {
        UChar32 ch;
        U8_NEXT(sv.data(), i, length, ch);
        if (ch < 0) { throw std::runtime_error{"Invalid UTF-8 sequence"}; }
        out.push_back(static_cast<char32_t>(ch));
    }
}

}  // namespace esc::detail