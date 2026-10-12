#include <esc/glyph.hpp>

namespace esc {
namespace {
template <class Text>
void append_text(GlyphString& output, Text input, Style style)
{
    detail::validate_style(style);
    auto const count = detail::text_count(input);
    auto const total = detail::appended_size(output, count);
    std::basic_string<typename Text::value_type> snapshot;
    if (total > output.capacity() &&
        detail::points_into(input.data(), output.data(), output.size())) {
        snapshot.assign(input);
        input = snapshot;
    }
    output.reserve(total);
    detail::for_each_scalar(
        input, [&](char32_t scalar) { output.push_back(Glyph{scalar, style}); });
}
}  // namespace

void append_glyphs(GlyphString& output, std::string_view utf8, Style style)
{ append_text(output, utf8, style); }

void append_glyphs(GlyphString& output, std::u32string_view utf32, Style style)
{ append_text(output, utf32, style); }

auto glyphs(std::string_view utf8, Style style) -> GlyphString
{
    GlyphString result;
    append_glyphs(result, utf8, style);
    return result;
}

auto glyphs(std::u32string_view utf32, Style style) -> GlyphString
{
    GlyphString result;
    append_glyphs(result, utf32, style);
    return result;
}
}  // namespace esc
