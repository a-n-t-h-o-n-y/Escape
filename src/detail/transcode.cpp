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
    auto result = std::string{};
    icu::UnicodeString{static_cast<UChar32>(c)}.toUTF8String(result);
    return result;
}

auto u32_to_u8(std::u32string_view s) -> std::string
{
    auto result = std::string{};
    icu::UnicodeString::fromUTF32(reinterpret_cast<UChar32 const*>(s.data()), s.size())
        .toUTF8String(result);
    return result;
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
    auto i = std::int32_t{0};  // Index in the UTF-8 string
    auto const length = static_cast<std::int32_t>(sv.length());

    auto result = std::u32string{};
    while (i < length) {
        UChar32 ch;
        U8_NEXT(sv.data(), i, length, ch);
        if (ch < 0) { throw std::runtime_error{"Invalid UTF-8 sequence"}; }
        result.push_back(static_cast<char32_t>(ch));
    }
    return result;
}

}  // namespace esc::detail