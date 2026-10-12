#include <esc/capabilities.hpp>
#include <esc/sequence.hpp>

#include <charconv>
#include <cstring>
#include <limits>
#include <variant>

namespace esc::detail {

void SequenceWriter::add_size(std::size_t count)
{
    if (count > std::numeric_limits<std::size_t>::max() - size_)
        throw std::length_error{"escape: encoded size overflow"};
    size_ += count;
}
void SequenceWriter::text(std::string_view bytes)
{
    auto const offset = size_;
    add_size(bytes.size());
    if (output_ && !bytes.empty())
        std::memcpy(output_ + offset, bytes.data(), bytes.size());
    if (sink_ && !bytes.empty()) sink_(context_, bytes);
}

void SequenceWriter::number(std::uint64_t value)
{
    if (!measuring()) {
        std::array<char, 20> bytes;
        auto end = std::to_chars(bytes.data(), bytes.data() + bytes.size(), value).ptr;
        text({bytes.data(), static_cast<std::size_t>(end - bytes.data())});
    }
    else {
        std::size_t width = 1;
        for (auto remaining = value / 10; remaining; remaining /= 10)
            ++width;
        add_size(width);
    }
}

namespace {
void parameter(SequenceWriter& out, std::uint64_t value, std::string_view final)
{
    out.text("\x1b[");
    out.number(value);
    out.text(final);
}
void mode(SequenceWriter& out, unsigned value, bool enabled)
{
    out.text("\x1b[?");
    out.number(value);
    out.text(enabled ? "h" : "l");
}
auto erase_parameter(Erase extent) -> unsigned
{
    switch (extent) {
        case Erase::ToEnd: return 0;
        case Erase::ToBeginning: return 1;
        case Erase::All: return 2;
    }
    throw std::invalid_argument{"escape: invalid erase extent"};
}
void color(SequenceWriter& out, Color const& value, bool foreground)
{
    if (value.valueless_by_exception())
        throw std::invalid_argument{"escape: valueless color"};
    std::visit(
        [&](auto c) {
            using C = decltype(c);
            if constexpr (std::same_as<C, DefaultColor>) {
                parameter(out, foreground ? 39 : 49, "m");
            }
            else if constexpr (std::same_as<C, IndexedColor>) {
                if (c.value < 8)
                    parameter(out, (foreground ? 30 : 40) + c.value, "m");
                else if (c.value < 16)
                    parameter(out, (foreground ? 90 : 100) + c.value - 8, "m");
                else {
                    out.text(foreground ? "\x1b[38;5;" : "\x1b[48;5;");
                    out.number(c.value);
                    out.text("m");
                }
            }
            else {
                out.text(foreground ? "\x1b[38;2;" : "\x1b[48;2;");
                out.number(c.red);
                out.text(";");
                out.number(c.green);
                out.text(";");
                out.number(c.blue);
                out.text("m");
            }
        },
        value);
}
}  // namespace

void encode(SequenceWriter& out, Cursor position)
{
    if (position.x < 0 || position.y < 0)
        throw std::invalid_argument{"escape: cursor coordinates must be nonnegative"};
    out.text("\x1b[");
    out.number(static_cast<std::uint64_t>(position.y) + 1);
    out.text(";");
    out.number(static_cast<std::uint64_t>(position.x) + 1);
    out.text("H");
}
void encode(SequenceWriter& out, MoveCursor delta)
{
    // Widen before negating: INT_MIN has no positive int counterpart.
    auto magnitude = [](int n) -> std::uint64_t {
        auto const wide = static_cast<std::int64_t>(n);
        return static_cast<std::uint64_t>(wide < 0 ? -wide : wide);
    };
    if (delta.dy) parameter(out, magnitude(delta.dy), delta.dy < 0 ? "A" : "B");
    if (delta.dx) parameter(out, magnitude(delta.dx), delta.dx < 0 ? "D" : "C");
}
void encode(SequenceWriter& out, BlankRow row)
{ parameter(out, erase_parameter(row.extent), "K"); }
void encode(SequenceWriter& out, BlankScreen screen)
{ parameter(out, erase_parameter(screen.extent), "J"); }
void encode(SequenceWriter& out, ResetStyle) { out.text("\x1b[0m"); }
void encode(SequenceWriter& out, Trait trait) { encode(out, Traits{trait}); }
namespace {
void trait_parameters(SequenceWriter& out, Traits traits, bool has_parameter)
{
    auto add = [&](Trait trait, std::string_view parameter) {
        if (!traits.contains(trait)) return;
        if (has_parameter) out.text(";");
        out.text(parameter);
        has_parameter = true;
    };
    add(Trait::Bold, "1");
    add(Trait::Dim, "2");
    add(Trait::Italic, "3");
    if (traits.contains(Trait::DoubleUnderline))
        add(Trait::DoubleUnderline, "21");
    else
        add(Trait::Underline, "4");
    add(Trait::Blink, "5");
    add(Trait::Inverse, "7");
    add(Trait::Invisible, "8");
    add(Trait::CrossedOut, "9");
}
}  // namespace
void replace_traits(SequenceWriter& out, Traits traits)
{
    out.text("\x1b[22;23;24;25;27;28;29");
    trait_parameters(out, traits, true);
    out.text("m");
}
void encode(SequenceWriter& out, Traits traits)
{
    if (!traits.data()) return;
    out.text("\x1b[");
    trait_parameters(out, traits, false);
    out.text("m");
}
void encode(SequenceWriter& out, ColorFG const& c) { color(out, c.value, true); }
void encode(SequenceWriter& out, ColorBG const& c) { color(out, c.value, false); }
void encode(SequenceWriter& out, Style const& style)
{
    color(out, style.background, false);
    color(out, style.foreground, true);
    replace_traits(out, style.traits);
}
void encode(SequenceWriter& out, ScreenBuffer buffer)
{
    switch (buffer) {
        case ScreenBuffer::Normal: mode(out, 1049, false); return;
        case ScreenBuffer::Alternate: mode(out, 1049, true); return;
    }
    throw std::invalid_argument{"escape: invalid screen buffer"};
}
void encode(SequenceWriter& out, CursorMode cursor_mode)
{
    switch (cursor_mode) {
        case CursorMode::Show: mode(out, 25, true); return;
        case CursorMode::Hide: mode(out, 25, false); return;
    }
    throw std::invalid_argument{"escape: invalid cursor mode"};
}
void encode(SequenceWriter& out, FocusReporting reporting)
{
    switch (reporting) {
        case FocusReporting::Off: mode(out, 1004, false); return;
        case FocusReporting::On: mode(out, 1004, true); return;
    }
    throw std::invalid_argument{"escape: invalid focus reporting"};
}
void encode(SequenceWriter& out, BracketedPaste paste)
{
    switch (paste) {
        case BracketedPaste::Off: mode(out, 2004, false); return;
        case BracketedPaste::On: mode(out, 2004, true); return;
    }
    throw std::invalid_argument{"escape: invalid bracketed paste"};
}
void encode(SequenceWriter& out, MouseReporting mouse)
{
    unsigned tracking = 0;
    switch (mouse.mode) {
        case MouseMode::Off: break;
        case MouseMode::Basic: tracking = 1000; break;
        case MouseMode::Drag: tracking = 1002; break;
        case MouseMode::Move: tracking = 1003; break;
        default: throw std::invalid_argument{"escape: invalid mouse mode"};
    }
    unsigned encoding = 0;
    switch (mouse.protocol) {
        case MouseProtocol::None:
            if (tracking)
                throw std::invalid_argument{"escape: enabled mouse needs a protocol"};
            break;
        case MouseProtocol::Legacy: break;
        case MouseProtocol::Sgr: encoding = 1006; break;
        case MouseProtocol::Urxvt: encoding = 1015; break;
        default: throw std::invalid_argument{"escape: invalid mouse protocol"};
    }
    out.text(
        "\x1b[?1000l\x1b[?1002l\x1b[?1003l\x1b[?1005l\x1b[?1006l\x1b[?1015l\x1b[?"
        "1016l");
    if (tracking) {
        if (encoding) mode(out, encoding, true);
        mode(out, tracking, true);
    }
}
}  // namespace esc::detail

namespace esc::detail {
void encode_style(SequenceWriter& output,
                  TerminalProfile const* profile,
                  Style const& style)
{
    if (profile)
        encode(output, prepare(*profile, style));
    else
        encode(output, style);
}
void encode_patch(SequenceWriter& output,
                  TerminalProfile const* profile,
                  Style& current,
                  StylePatch const& patch)
{
    validate_attribute(patch);
    auto next = current | patch;
    if (patch.background) {
        if (profile)
            encode(output, prepare(*profile, bg(*patch.background)));
        else
            encode(output, bg(*patch.background));
    }
    if (patch.foreground) {
        if (profile)
            encode(output, prepare(*profile, fg(*patch.foreground)));
        else
            encode(output, fg(*patch.foreground));
    }
    auto enabled = patch.enabled, disabled = patch.disabled;
    if (profile) {
        enabled = prepare(*profile, enabled).value_or(Traits{});
        disabled = prepare(*profile, disabled).value_or(Traits{});
    }
    // SGR shares the bold/dim and underline/double-underline reset codes.
    // Restore known peers from this operation after resetting either group.
    if (disabled.contains(Trait::Bold) || disabled.contains(Trait::Dim)) {
        output.text("\x1b[22m");
        for (auto trait : {Trait::Bold, Trait::Dim})
            if (next.traits.contains(trait)) enabled.insert(trait);
    }
    if (disabled.contains(Trait::Underline) ||
        disabled.contains(Trait::DoubleUnderline)) {
        output.text("\x1b[24m");
        for (auto trait : {Trait::Underline, Trait::DoubleUnderline})
            if (next.traits.contains(trait)) enabled.insert(trait);
    }
    struct Disable {
        Trait trait;
        std::string_view bytes;
    };
    for (auto item :
         {Disable{Trait::Italic, "\x1b[23m"}, Disable{Trait::Blink, "\x1b[25m"},
          Disable{Trait::Inverse, "\x1b[27m"}, Disable{Trait::Invisible, "\x1b[28m"},
          Disable{Trait::CrossedOut, "\x1b[29m"}})
        if (disabled.contains(item.trait)) output.text(item.bytes);
    // Keep double underline's precedence when a later patch enables underline.
    if (enabled.contains(Trait::Underline) &&
        next.traits.contains(Trait::DoubleUnderline))
        enabled.insert(Trait::DoubleUnderline);
    if (profile) enabled = prepare(*profile, enabled).value_or(Traits{});
    encode(output, enabled);
    current = next;
}
}  // namespace esc::detail
