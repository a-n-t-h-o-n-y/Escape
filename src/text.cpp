#include <esc/detail/unicode.hpp>
#include <esc/text.hpp>

namespace esc {
void append_utf32(std::u32string& output, std::string_view utf8)
{
    auto const count = detail::utf8_scalar_count(utf8);
    auto const prefix = output.size();
    if (count > output.max_size() - prefix)
        throw std::length_error{"Unicode: UTF-32 output exceeds string size limit"};
    if (!count) return;
    std::string snapshot;
    if (detail::points_into(utf8.data(), output.data(), prefix + 1)) {
        snapshot.assign(utf8);
        utf8 = snapshot;
    }
    output.resize(prefix + count);
    auto* destination = output.data() + prefix;
    detail::for_each_scalar(utf8, [&](char32_t scalar) { *destination++ = scalar; });
}

auto utf8_to_utf32(std::string_view input) -> std::u32string
{
    std::u32string result;
    append_utf32(result, input);
    return result;
}

void append_utf8(std::string& output, std::u32string_view utf32)
{
    auto const count = detail::utf8_byte_count(utf32);
    auto const prefix = output.size();
    if (count > output.max_size() - prefix)
        throw std::length_error{"Unicode: UTF-8 output exceeds string size limit"};
    if (!count) return;
    output.resize(prefix + count);
    auto* destination = output.data() + prefix;
    for (auto scalar : utf32)
        destination += detail::encode_utf8_to(destination, scalar);
}

auto utf32_to_utf8(std::u32string_view input) -> std::string
{
    std::string result;
    append_utf8(result, input);
    return result;
}
}  // namespace esc
