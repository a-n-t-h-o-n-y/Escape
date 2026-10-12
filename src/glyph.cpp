#include <esc/glyph.hpp>

namespace esc {
namespace {
template<class Text>
void append_text(GlyphString& output, Text input, Brush brush)
{
    detail::validate_brush(brush);
    auto const count = detail::text_count(input);
    auto const total = detail::appended_size(output, count);
    std::basic_string<typename Text::value_type> snapshot;
    if (total > output.capacity() && detail::points_into(input.data(), output.data(), output.size())) {
        snapshot.assign(input);
        input = snapshot;
    }
    output.reserve(total);
    detail::for_each_scalar(input, [&](char32_t scalar) { output.push_back(Glyph{scalar, brush}); });
}
} // namespace
void append_glyphs(GlyphString& output, std::string_view utf8, Brush brush)
{ append_text(output, utf8, brush); }
void append_glyphs(GlyphString& output, std::u32string_view utf32, Brush brush)
{ append_text(output, utf32, brush); }
auto glyphs(std::string_view utf8, Brush brush) -> GlyphString
{
    GlyphString result;
    append_glyphs(result, utf8, brush);
    return result;
}
auto glyphs(std::u32string_view utf32, Brush brush) -> GlyphString
{
    GlyphString result;
    append_glyphs(result, utf32, brush);
    return result;
}
} // namespace esc
