#pragma once

#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ratio>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

#include <algorithm>
#include <array>
#include <esc/detail/any_of.hpp>
#include <esc/glyph.hpp>
#include <esc/point.hpp>
#include <esc/style.hpp>
#include <esc/styled_text.hpp>
#include <memory>

namespace esc {

struct TerminalProfile;

/// Absolute zero-based cell position; negative coordinates are invalid.
struct Cursor {
    int x = 0;
    int y = 0;
};

/// Signed relative cursor movement.
struct MoveCursor {
    int dx = 0;
    int dy = 0;
};

/// Convert a Point into an absolute cursor command.
[[nodiscard]] constexpr auto cursor(Point p) -> Cursor { return {p.x, p.y}; }

enum class Erase { ToEnd, ToBeginning, All };

/// Erase part or all of the current row. Default: the whole row.
struct BlankRow {
    Erase extent = Erase::All;
};

/// Erase part or all of the screen. Default: the whole screen.
struct BlankScreen {
    Erase extent = Erase::All;
};

/// Reset all SGR appearance.
struct ResetStyle {};

/// Request the terminal's configured audible/visual bell. Does not flush output.
struct Bell {};

/// DECPS note identities: Rest = 0; C5 through C7 = 1 through 25.
enum class Note : std::uint8_t {
    Rest,
    C5,
    Cs5,
    D5,
    Ds5,
    E5,
    F5,
    Fs5,
    G5,
    Gs5,
    A5,
    As5,
    B5,
    C6,
    Cs6,
    D6,
    Ds6,
    E6,
    F6,
    Fs6,
    G6,
    Gs6,
    A6,
    As6,
    B6,
    C7
};

/// Exact DECPS timing: one tick is 1/32 second (31,250 microseconds).
using NoteDuration = std::chrono::duration<int, std::ratio<1, 32>>;

/// Owns the melody. Duration is per note, including rests; no application sleeps.
/// Duration rounds to the nearest tick, ties up; positive values use at least one.
/// Require 0..7,968,750us and volume 0..7. Empty notes emit nothing.
/// Long melodies are encoded as consecutive DECPS commands of up to 16 notes.
struct PlayNotes {
    std::vector<Note> notes;
    std::chrono::microseconds duration = std::chrono::milliseconds{125};
    unsigned volume = 4;
};

/// Default, blinking, or steady block/underline/bar cursor shape.
enum class CursorStyle {
    Default,
    BlinkingBlock,
    SteadyBlock,
    BlinkingUnderline,
    SteadyUnderline,
    BlinkingBar,
    SteadyBar
};

/// Save state in the terminal's single DEC slot, not an application stack.
struct SaveCursor {};

/// Restore state from the terminal's DEC saved-state slot.
struct RestoreCursor {};

/// Set zero-based inclusive margins; require 0 <= top < bottom.
struct ScrollRegion {
    int top;
    int bottom;
};  // zero-based inclusive, top < bottom

/// Reset the scrolling margins to the full screen.
struct ResetScrollRegion {};

/// Scroll up within the current margins; zero count does nothing.
struct ScrollUp {
    unsigned count = 1;
};

/// Scroll down within the current margins; zero count does nothing.
struct ScrollDown {
    unsigned count = 1;
};

/// Insert blank cells at the cursor; zero count does nothing.
struct InsertCharacters {
    unsigned count = 1;
};

/// Delete cells at the cursor; zero count does nothing.
struct DeleteCharacters {
    unsigned count = 1;
};

/// Insert lines at the cursor within current margins.
struct InsertLines {
    unsigned count = 1;
};

/// Delete lines at the cursor within current margins.
struct DeleteLines {
    unsigned count = 1;
};

/// Borrow a UTF-8 title; controls are forbidden and encoding validates it.
struct WindowTitle {
    std::string_view text;
};

/// Borrow a nonempty UTF-8 URI and optional ID for an OSC 8 link.
struct BeginHyperlink {
    std::string_view uri;
    std::string_view id{};
};

/// End the current OSC 8 link.
struct EndHyperlink {};

enum class SynchronizedOutput { Off, On };

enum class ScreenBuffer { Normal, Alternate };

enum class CursorMode { Show, Hide };

enum class MouseMode { Off, Basic, Drag, Move };

enum class MouseProtocol { None, Legacy, Sgr, Urxvt };

struct MouseReporting {
    MouseMode mode;
    MouseProtocol protocol;
};

enum class FocusReporting { Off, On };

enum class BracketedPaste { Off, On };

template <class T>
concept DrawingCommand = detail::AnyOf<std::remove_cvref_t<T>,
                                       Cursor,
                                       MoveCursor,
                                       BlankRow,
                                       BlankScreen,
                                       ResetStyle,
                                       Bell,
                                       PlayNotes,
                                       Trait,
                                       Traits,
                                       ColorFG,
                                       ColorBG,
                                       Style,
                                       StylePatch,
                                       WithoutTraits,
                                       CursorStyle,
                                       SaveCursor,
                                       RestoreCursor,
                                       ScrollRegion,
                                       ResetScrollRegion,
                                       ScrollUp,
                                       ScrollDown,
                                       InsertCharacters,
                                       DeleteCharacters,
                                       InsertLines,
                                       DeleteLines,
                                       WindowTitle,
                                       BeginHyperlink,
                                       EndHyperlink>;

template <class T>
concept Escapable = DrawingCommand<T> || detail::AnyOf<std::remove_cvref_t<T>,
                                                       ScreenBuffer,
                                                       CursorMode,
                                                       MouseReporting,
                                                       FocusReporting,
                                                       BracketedPaste,
                                                       SynchronizedOutput>;

/// Bounded encoding result. A short span has written=0 and full required size.
struct EncodeResult {
    std::size_t written;
    std::size_t required;
};

namespace detail {
/// A null destination measures only. Both passes share the canonical serializer.
/// Command validation occurs during measurement, before caller storage is changed.
/// A writing pass requires the same commands to have been measured first.
class SequenceWriter {
   public:
    explicit SequenceWriter(char* output = nullptr) : output_{output} {}

    using Sink = void (*)(void*, std::string_view);

    SequenceWriter(void* context, Sink sink)
        : output_{nullptr}, context_{context}, sink_{sink}
    {}

    void text(std::string_view bytes);

    void number(std::uint64_t value);

    [[nodiscard]] auto size() const noexcept -> std::size_t { return size_; }

    [[nodiscard]] auto measuring() const noexcept -> bool { return !output_ && !sink_; }

   private:
    void add_size(std::size_t count);

    char* output_;
    void* context_ = nullptr;
    Sink sink_ = nullptr;
    std::size_t size_ = 0;
};

void encode(SequenceWriter&, Cursor);

void encode(SequenceWriter&, MoveCursor);

void encode(SequenceWriter&, BlankRow);

void encode(SequenceWriter&, BlankScreen);

void encode(SequenceWriter&, ResetStyle);

void encode(SequenceWriter&, Bell);

void encode(SequenceWriter&, PlayNotes const&);

void encode(SequenceWriter&, PlayNotes const*);

void encode(SequenceWriter&, Trait);

void encode(SequenceWriter&, Traits);

void encode(SequenceWriter&, ColorFG const&);

void encode(SequenceWriter&, ColorBG const&);

void encode(SequenceWriter&, Style const&);

void encode(SequenceWriter&, ScreenBuffer);

void encode(SequenceWriter&, CursorMode);

void encode(SequenceWriter&, MouseReporting);

void encode(SequenceWriter&, FocusReporting);

void encode(SequenceWriter&, BracketedPaste);

void encode(SequenceWriter&, CursorStyle);

void encode(SequenceWriter&, SaveCursor);

void encode(SequenceWriter&, RestoreCursor);

void encode(SequenceWriter&, ScrollRegion);

void encode(SequenceWriter&, ResetScrollRegion);

void encode(SequenceWriter&, ScrollUp);

void encode(SequenceWriter&, ScrollDown);

void encode(SequenceWriter&, InsertCharacters);

void encode(SequenceWriter&, DeleteCharacters);

void encode(SequenceWriter&, InsertLines);

void encode(SequenceWriter&, DeleteLines);

void encode(SequenceWriter&, WindowTitle);

void encode(SequenceWriter&, BeginHyperlink);

void encode(SequenceWriter&, EndHyperlink);

void encode(SequenceWriter&, SynchronizedOutput);

/// A complete style adapted to the selected profile, using stack storage.
struct PreparedStyle {
    std::optional<ColorBG> background;
    std::optional<ColorFG> foreground;
    std::optional<Traits> traits;
};

void encode(SequenceWriter&, PreparedStyle const&);

template <class Command>
void encode(SequenceWriter& output, std::optional<Command> const& command)
{
    if (command) encode(output, *command);
}

void validate_profile(TerminalProfile const&);

auto prepare(TerminalProfile const&, Cursor const&) -> std::optional<Cursor>;

auto prepare(TerminalProfile const&, MoveCursor const&) -> std::optional<MoveCursor>;

auto prepare(TerminalProfile const&, BlankRow const&) -> std::optional<BlankRow>;

auto prepare(TerminalProfile const&, BlankScreen const&) -> std::optional<BlankScreen>;

auto prepare(TerminalProfile const&, ResetStyle const&) -> std::optional<ResetStyle>;

auto prepare(TerminalProfile const&, Bell const&) -> std::optional<Bell>;

/// Borrow the owned payload for this encoding pass, avoiding melody copies.
auto prepare(TerminalProfile const&, PlayNotes const&) -> PlayNotes const*;

auto prepare(TerminalProfile const&, Traits const&) -> std::optional<Traits>;

auto prepare(TerminalProfile const&, ColorFG const&) -> std::optional<ColorFG>;

auto prepare(TerminalProfile const&, ColorBG const&) -> std::optional<ColorBG>;

auto prepare(TerminalProfile const&, ScreenBuffer const&)
    -> std::optional<ScreenBuffer>;

auto prepare(TerminalProfile const&, CursorMode const&) -> std::optional<CursorMode>;

auto prepare(TerminalProfile const&, MouseReporting const&)
    -> std::optional<MouseReporting>;

auto prepare(TerminalProfile const&, FocusReporting const&)
    -> std::optional<FocusReporting>;

auto prepare(TerminalProfile const&, BracketedPaste const&)
    -> std::optional<BracketedPaste>;

auto prepare(TerminalProfile const&, Trait) -> std::optional<Traits>;

auto prepare(TerminalProfile const&, Style const&) -> PreparedStyle;

auto prepare(TerminalProfile const&, CursorStyle const&) -> std::optional<CursorStyle>;

auto prepare(TerminalProfile const&, SaveCursor const&) -> std::optional<SaveCursor>;

auto prepare(TerminalProfile const&, RestoreCursor const&)
    -> std::optional<RestoreCursor>;

auto prepare(TerminalProfile const&, ScrollRegion const&)
    -> std::optional<ScrollRegion>;

auto prepare(TerminalProfile const&, ResetScrollRegion const&)
    -> std::optional<ResetScrollRegion>;

auto prepare(TerminalProfile const&, ScrollUp const&) -> std::optional<ScrollUp>;

auto prepare(TerminalProfile const&, ScrollDown const&) -> std::optional<ScrollDown>;

auto prepare(TerminalProfile const&, InsertCharacters const&)
    -> std::optional<InsertCharacters>;

auto prepare(TerminalProfile const&, DeleteCharacters const&)
    -> std::optional<DeleteCharacters>;

auto prepare(TerminalProfile const&, InsertLines const&) -> std::optional<InsertLines>;

auto prepare(TerminalProfile const&, DeleteLines const&) -> std::optional<DeleteLines>;

auto prepare(TerminalProfile const&, WindowTitle const&) -> std::optional<WindowTitle>;

auto prepare(TerminalProfile const&, BeginHyperlink const&)
    -> std::optional<BeginHyperlink>;

auto prepare(TerminalProfile const&, EndHyperlink const&)
    -> std::optional<EndHyperlink>;

auto prepare(TerminalProfile const&, SynchronizedOutput const&)
    -> std::optional<SynchronizedOutput>;

/// These are borrowed command payloads; their overlap checks are also used by
/// the recursive encoder for text and nested compositions.
auto overlaps(std::string_view input, char const* storage, std::size_t size) noexcept
    -> bool;

auto payload_overlaps(WindowTitle const&, char const*, std::size_t) noexcept -> bool;

auto payload_overlaps(BeginHyperlink const&, char const*, std::size_t) noexcept -> bool;

auto payload_overlaps(PlayNotes const&, char const*, std::size_t) noexcept -> bool;

void relocate_payload(WindowTitle&, char const*, std::size_t, char const*);

void relocate_payload(BeginHyperlink&, char const*, std::size_t, char const*);

/// Full styles replace all modeled traits. Patches enable/disable selected traits.
void replace_traits(SequenceWriter&, Traits);

void encode_patch(SequenceWriter&, TerminalProfile const*, Style&, StylePatch const&);

void encode_style(SequenceWriter&, TerminalProfile const*, Style const&);

/// A single concept controls all sinks; composition is checked recursively.
template <class T, bool DrawingOnly = false>
inline constexpr bool encodable = [] {
    using V = std::remove_cvref_t<T>;
    if constexpr (IsSequence<V>::value) {
        return []<class... U>(Sequence<U...> const*) {
            return (encodable<U, DrawingOnly> && ...);
        }(static_cast<V const*>(nullptr));
    }
    else if constexpr (IsStyledText<V>::value) {
        return encodable<decltype(std::declval<V>().content), DrawingOnly>;
    }
    else
        return (DrawingOnly ? DrawingCommand<V> : Escapable<V>) || ByteText<V> ||
               ScalarText<V> || AnyOf<V, char, char32_t, Glyph> || GlyphRange<V const>;
}();

template <class T>
auto byte_view(T const& value) -> std::string_view
{
    if constexpr (std::is_pointer_v<std::remove_cvref_t<T>>)
        if (!value) throw std::invalid_argument{"Output: null string"};
    return std::string_view{value};
}

template <class T>
auto scalar_view(T const& value) -> std::u32string_view
{
    if constexpr (std::is_pointer_v<std::remove_cvref_t<T>>)
        if (!value) throw std::invalid_argument{"Output: null string"};
    return std::u32string_view{value};
}

template <class T>
void encode_value(SequenceWriter& output,
                  TerminalProfile const* profile,
                  Style& current,
                  T const& value)
{
    using V = std::remove_cvref_t<T>;
    if constexpr (IsSequence<V>::value) {
        std::apply(
            [&](auto const&... child) {
                (encode_value(output, profile, current, child), ...);
            },
            value.values);
    }
    else if constexpr (IsStyledText<V>::value) {
        validate_attribute(value.appearance);
        auto saved = current;
        current = current | value.appearance;
        encode_style(output, profile, current);
        // Styled byte strings are Unicode text; bare byte strings remain raw.
        if constexpr (ByteText<decltype(value.content)>)
            static_cast<void>(utf8_scalar_count(byte_view(value.content)));
        encode_value(output, profile, current, value.content);
        encode_style(output, profile, saved);
        current = saved;
    }
    else if constexpr (std::same_as<V, Glyph> || GlyphRange<V const>) {
        auto saved = current;
        bool first = true;
        std::size_t offset = 0;
        auto emit = [&](Glyph const& glyph) {
            validate_glyph(glyph, offset++);
            if (first || current != glyph.style)
                encode_style(output, profile, glyph.style);
            current = glyph.style;
            first = false;
            auto bytes = encode_utf8(glyph.symbol);
            output.text(bytes.view());
        };
        if constexpr (std::same_as<V, Glyph>)
            emit(value);
        else
            for (auto const& glyph : value)
                emit(glyph);
        if (!first) encode_style(output, profile, saved);
        current = saved;
    }
    else if constexpr (std::same_as<V, char>)
        output.text({&value, 1});
    else if constexpr (std::same_as<V, char32_t>) {
        auto bytes = encode_utf8(value);
        output.text(bytes.view());
    }
    else if constexpr (ByteText<V>)
        output.text(byte_view(value));
    else if constexpr (ScalarText<V>) {
        auto text = scalar_view(value);
        validate_utf32(text);
        for (auto scalar : text) {
            auto bytes = encode_utf8(scalar);
            output.text(bytes.view());
        }
    }
    else if constexpr (std::same_as<V, Style>) {
        validate_style(value);
        encode_style(output, profile, value);
        current = value;
    }
    else if constexpr (PatchAttribute<V>) {
        encode_patch(output, profile, current, patch(value));
    }
    else {
        if constexpr (std::same_as<V, ResetStyle>) current = {};
        if (profile)
            encode(output, prepare(*profile, value));
        else
            encode(output, value);
    }
}

template <class T>
auto value_overlaps(T const& value, char const* storage, std::size_t size) -> bool
{
    using V = std::remove_cvref_t<T>;
    if constexpr (IsSequence<V>::value)
        return std::apply(
            [&](auto const&... child) {
                return (value_overlaps(child, storage, size) || ...);
            },
            value.values);
    else if constexpr (IsStyledText<V>::value)
        return value_overlaps(value.content, storage, size);
    else if constexpr (ByteText<V>)
        return overlaps(byte_view(value), storage, size);
    else if constexpr (ScalarText<V>) {
        auto view = scalar_view(value);
        return overlaps({reinterpret_cast<char const*>(view.data()),
                         view.size() * sizeof(char32_t)},
                        storage, size);
    }
    else if constexpr (std::same_as<V, WindowTitle> ||
                       std::same_as<V, BeginHyperlink> || std::same_as<V, PlayNotes>)
        return payload_overlaps(value, storage, size);
    else if constexpr (GlyphRange<V const>) {
        if constexpr (std::ranges::contiguous_range<V const>)
            return overlaps({reinterpret_cast<char const*>(std::ranges::data(value)),
                             std::ranges::size(value) * sizeof(Glyph)},
                            storage, size);
        else {
            // Real glyph references can be inspected even in a list/filter view.
            if constexpr (std::is_lvalue_reference_v<
                              std::ranges::range_reference_t<V const>>)
                for (auto const& glyph : value)
                    if (overlaps({reinterpret_cast<char const*>(std::addressof(glyph)),
                                  sizeof(Glyph)},
                                 storage, size))
                        return true;
            return false;
        }
    }
    else
        return false;
}

template <class T>
auto requires_snapshot(T const& value) -> bool
{
    using V = std::remove_cvref_t<T>;
    if constexpr (IsSequence<V>::value)
        return std::apply(
            [](auto const&... child) { return (requires_snapshot(child) || ...); },
            value.values);
    else if constexpr (IsStyledText<V>::value)
        return requires_snapshot(value.content);
    else if constexpr (GlyphRange<V const>)
        return !std::ranges::contiguous_range<V const>;
    else
        return false;
}

template <class... Values>
void encode_values(SequenceWriter& writer,
                   TerminalProfile const* profile,
                   Values const&... values)
{
    Style current;
    (encode_value(writer, profile, current, values), ...);
}

template <class... Values>
void append_values(std::string& output,
                   TerminalProfile const* profile,
                   Values const&... values)
{
    if (profile) validate_profile(*profile);
    SequenceWriter measure;
    encode_values(measure, profile, values...);
    auto required = measure.size(), prefix = output.size();
    if (required > output.max_size() - prefix)
        throw std::length_error{"encode: output exceeds string size limit"};
    if (!required) return;
    if (((requires_snapshot(values) ||
          value_overlaps(values, output.data(), prefix + 1)) ||
         ...)) {
        // Encode before growth: aliases and opaque ranges may depend on the
        // destination.
        std::string suffix(required, '\0');
        SequenceWriter writer{suffix.data()};
        encode_values(writer, profile, values...);
        output.append(suffix);
    }
    else {
        output.resize(prefix + required);
        SequenceWriter writer{output.data() + prefix};
        encode_values(writer, profile, values...);
    }
}

template <class... Values>
auto bounded_values(std::span<char> output,
                    TerminalProfile const* profile,
                    Values const&... values) -> EncodeResult
{
    if (profile) validate_profile(*profile);
    SequenceWriter measure;
    encode_values(measure, profile, values...);
    auto required = measure.size();
    if (required > output.size()) return {0, required};
    if ((value_overlaps(values, output.data(), required) || ...))
        throw std::invalid_argument{"encode_to: output overlaps borrowed payload"};
    SequenceWriter writer{output.data()};
    encode_values(writer, profile, values...);
    return {required, required};
}
}  // namespace detail

template <class T>
concept Encodable = detail::encodable<T>;

template <class T>
concept Drawable = detail::encodable<T, true>;

/// Append raw encoded values. Validate before changing storage; handle aliases.
template <Encodable... Values>
void append_encoded(std::string& output, Values const&... values)
{ detail::append_values(output, nullptr, values...); }

/// Append through a capability profile. Validate before changing storage.
template <Encodable... Values>
void append_encoded(std::string& output,
                    TerminalProfile const& profile,
                    Values const&... values)
{ detail::append_values(output, &profile, values...); }

/// Return raw encoded bytes in an owning string, without I/O.
template <Encodable... Values>
[[nodiscard]] auto encode(Values const&... values) -> std::string
{
    std::string output;
    append_encoded(output, values...);
    return output;
}

/// Return encoded bytes after applying capability policy.
template <Encodable... Values>
[[nodiscard]] auto encode(TerminalProfile const& profile, Values const&... values)
    -> std::string
{
    std::string output;
    append_encoded(output, profile, values...);
    return output;
}

/// All-or-nothing, no terminator, no allocation, untouched on insufficient space.
/// Encode into caller storage without allocation or a terminator.
/// A short span is unchanged; inspectable overlap with payloads is rejected.
template <Encodable... Values>
[[nodiscard]] auto encode_to(std::span<char> output, Values const&... values)
    -> EncodeResult
{ return detail::bounded_values(output, nullptr, values...); }

/// Bounded, all-or-nothing encoding through a capability profile.
template <Encodable... Values>
[[nodiscard]] auto encode_to(std::span<char> output,
                             TerminalProfile const& profile,
                             Values const&... values) -> EncodeResult
{ return detail::bounded_values(output, &profile, values...); }
}  // namespace esc
