#include <esc/capabilities.hpp>
#include <esc/detail/unicode.hpp>
#include <esc/sequence.hpp>
#include <functional>

namespace esc::detail {
namespace {
void counted(SequenceWriter& out, unsigned count, std::string_view final)
{
    if (count) {
        out.text("\x1b[");
        out.number(count);
        out.text(final);
    }
}
void osc_text(std::string_view input)
{
    for_each_scalar(input, [](char32_t cp) {
        if (cp < 32 || (cp >= 0x7f && cp <= 0x9f))
            throw std::invalid_argument{"OSC: control character in payload"};
    });
}
void validate(WindowTitle const& value) { osc_text(value.text); }
void validate(BeginHyperlink const& value)
{
    if (value.uri.empty())
        throw std::invalid_argument{"Hyperlink: empty URI; use EndHyperlink"};
    osc_text(value.uri);
    for (unsigned char c : value.id)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~'))
            throw std::invalid_argument{
                "Hyperlink: id must use ASCII unreserved characters"};
}
auto relocate(std::string_view input,
              char const* data,
              std::size_t size,
              char const* copy) -> std::string_view
{
    if (!overlaps(input, data, size)) return input;
    auto less = std::less<char const*>{};
    if (less(input.data(), data) || less(data + size, input.data() + input.size()))
        throw std::invalid_argument{
            "escape: aliased payload extends beyond owned string"};
    return {copy + (input.data() - data), input.size()};
}
template <class Command>
auto prepare_optional(TerminalProfile const& profile,
                      Command command,
                      bool supported,
                      Feature feature) -> std::optional<Command>
{
    SequenceWriter validate;
    encode(validate, command);
    if (supported || validate.size() == 0) return command;
    if (profile.fallback == FallbackPolicy::Strict) throw UnsupportedFeature{feature};
    return {};
}
}  // namespace
void encode(SequenceWriter& out, CursorStyle style)
{
    auto n = static_cast<unsigned>(style);
    if (n > 6) throw std::invalid_argument{"CursorStyle: invalid value"};
    out.text("\x1b[");
    out.number(n);
    out.text(" q");
}
void encode(SequenceWriter& out, SaveCursor)
{
    out.text(
        "\x1b"
        "7");
}
void encode(SequenceWriter& out, RestoreCursor)
{
    out.text(
        "\x1b"
        "8");
}
void encode(SequenceWriter& out, ScrollRegion value)
{
    if (value.top < 0 || value.bottom <= value.top)
        throw std::invalid_argument{"ScrollRegion: require 0 <= top < bottom"};
    out.text("\x1b[");
    out.number(static_cast<std::uint64_t>(value.top) + 1);
    out.text(";");
    out.number(static_cast<std::uint64_t>(value.bottom) + 1);
    out.text("r");
}
void encode(SequenceWriter& out, ResetScrollRegion) { out.text("\x1b[r"); }
void encode(SequenceWriter& out, ScrollUp value) { counted(out, value.count, "S"); }
void encode(SequenceWriter& out, ScrollDown value) { counted(out, value.count, "T"); }
void encode(SequenceWriter& out, InsertCharacters value)
{ counted(out, value.count, "@"); }
void encode(SequenceWriter& out, DeleteCharacters value)
{ counted(out, value.count, "P"); }
void encode(SequenceWriter& out, InsertLines value) { counted(out, value.count, "L"); }
void encode(SequenceWriter& out, DeleteLines value) { counted(out, value.count, "M"); }
void encode(SequenceWriter& out, WindowTitle value)
{
    validate(value);
    out.text("\x1b]2;");
    out.text(value.text);
    out.text("\x1b\\");
}
void encode(SequenceWriter& out, BeginHyperlink value)
{
    validate(value);
    out.text("\x1b]8;");
    if (!value.id.empty()) {
        out.text("id=");
        out.text(value.id);
    }
    out.text(";");
    out.text(value.uri);
    out.text("\x1b\\");
}
void encode(SequenceWriter& out, EndHyperlink) { out.text("\x1b]8;;\x1b\\"); }
void encode(SequenceWriter& out, SynchronizedOutput value)
{
    if (value != SynchronizedOutput::On && value != SynchronizedOutput::Off)
        throw std::invalid_argument{"SynchronizedOutput: invalid value"};
    out.text(value == SynchronizedOutput::On ? "\x1b[?2026h" : "\x1b[?2026l");
}
auto overlaps(std::string_view input, char const* data, std::size_t size) noexcept
    -> bool
{
    if (input.empty() || !size) return false;
    auto less = std::less<char const*>{};
    return less(input.data(), data + size) && less(data, input.data() + input.size());
}
auto payload_overlaps(WindowTitle const& value,
                      char const* data,
                      std::size_t size) noexcept -> bool
{ return overlaps(value.text, data, size); }
auto payload_overlaps(BeginHyperlink const& value,
                      char const* data,
                      std::size_t size) noexcept -> bool
{ return overlaps(value.uri, data, size) || overlaps(value.id, data, size); }
void relocate_payload(WindowTitle& value,
                      char const* data,
                      std::size_t size,
                      char const* copy)
{ value.text = relocate(value.text, data, size, copy); }
void relocate_payload(BeginHyperlink& value,
                      char const* data,
                      std::size_t size,
                      char const* copy)
{
    value.uri = relocate(value.uri, data, size, copy);
    value.id = relocate(value.id, data, size, copy);
}
#define ESC_PREPARE(Type, member, feature)                                           \
    auto prepare(TerminalProfile const& p, Type const& value) -> std::optional<Type> \
    { return prepare_optional(p, value, p.member, Feature::feature); }
ESC_PREPARE(CursorStyle, cursor_style, CursorStyle)
ESC_PREPARE(SaveCursor, cursor_save_restore, CursorSaveRestore)
ESC_PREPARE(RestoreCursor, cursor_save_restore, CursorSaveRestore)
ESC_PREPARE(ScrollRegion, scrolling, Scrolling)
ESC_PREPARE(ResetScrollRegion, scrolling, Scrolling)
ESC_PREPARE(ScrollUp, scrolling, Scrolling)
ESC_PREPARE(ScrollDown, scrolling, Scrolling)
ESC_PREPARE(InsertCharacters, editing, Editing)
ESC_PREPARE(DeleteCharacters, editing, Editing)
ESC_PREPARE(InsertLines, editing, Editing)
ESC_PREPARE(DeleteLines, editing, Editing)
ESC_PREPARE(WindowTitle, window_title, WindowTitle)
ESC_PREPARE(BeginHyperlink, hyperlinks, Hyperlinks)
ESC_PREPARE(EndHyperlink, hyperlinks, Hyperlinks)
ESC_PREPARE(SynchronizedOutput, synchronized_output, SynchronizedOutput)
#undef ESC_PREPARE
}  // namespace esc::detail
