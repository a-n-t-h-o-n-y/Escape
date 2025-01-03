#include <esc/detail/transcode.hpp>

#include <array>
#include <string>

#include <unicode/stringpiece.h>
#include <unicode/unistr.h>

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

}  // namespace esc::detail