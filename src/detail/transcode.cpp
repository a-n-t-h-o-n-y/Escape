#include <esc/detail/transcode.hpp>
#include <esc/detail/unicode.hpp>
#include <esc/text.hpp>

// Transitional adapters for the existing I/O implementation. Public text and
// glyph APIs use the strict, allocation-free scalar decoder directly.
namespace esc::detail {
auto u32_to_u8(char32_t scalar) -> std::string
{ auto const encoded = encode_utf8(scalar); return std::string{encoded.view()}; }
auto u32_to_u8(std::u32string_view input) -> std::string { return utf32_to_utf8(input); }
void u32_to_u8(std::u32string_view input, std::string& output) { output = utf32_to_utf8(input); }
auto u8_to_u32(std::array<char, 4> bytes) -> char32_t
{
    std::size_t offset = 0;
    return decode_utf8({bytes.data(), bytes.size()}, offset);
}
auto u8_string_to_u32(std::string_view input) -> zzz::Generator<char32_t>
{
    for (std::size_t offset = 0; offset < input.size();) co_yield decode_utf8(input, offset);
}
auto u8_string_to_u32_string(std::string_view input) -> std::u32string { return utf8_to_utf32(input); }
void u8_string_to_u32_string(std::string_view input, std::u32string& output) { output = utf8_to_utf32(input); }
} // namespace esc::detail
