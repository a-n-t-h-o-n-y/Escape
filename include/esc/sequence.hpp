#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <tuple>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

#include <esc/brush.hpp>
#include <esc/detail/any_of.hpp>
#include <esc/point.hpp>

namespace esc {

struct TerminalProfile;

// Positions are zero-based; movement deltas are signed.
struct Cursor { int x = 0; int y = 0; };
struct MoveCursor { int dx = 0; int dy = 0; };
[[nodiscard]] constexpr auto cursor(Point p) -> Cursor { return {p.x, p.y}; }

enum class Erase { ToEnd, ToBeginning, All };
struct BlankRow { Erase extent = Erase::All; };
struct BlankScreen { Erase extent = Erase::All; };
struct ResetStyle {};

enum class ScreenBuffer { Normal, Alternate };
enum class CursorMode { Show, Hide };
enum class MouseMode { Off, Basic, Drag, Move };
enum class MouseProtocol { None, Legacy, Sgr, Urxvt };
struct MouseReporting { MouseMode mode; MouseProtocol protocol; };
enum class FocusReporting { Off, On };
enum class BracketedPaste { Off, On };

template<class T>
concept DrawingCommand = detail::AnyOf<std::remove_cvref_t<T>,
    Cursor, MoveCursor, BlankRow, BlankScreen, ResetStyle,
    Trait, Traits, ColorFG, ColorBG, Brush>;

template<class T>
concept Escapable = DrawingCommand<T> || detail::AnyOf<std::remove_cvref_t<T>,
    ScreenBuffer, CursorMode, MouseReporting, FocusReporting, BracketedPaste>;

struct EncodeResult { std::size_t written; std::size_t required; };

namespace detail {
// A null destination measures only. Both passes share the canonical serializer.
// Command validation occurs during measurement, before caller storage is changed.
class SequenceWriter {
public:
    explicit SequenceWriter(char* output = nullptr) : output_{output} {}
    void text(std::string_view bytes);
    void number(std::uint64_t value);
    [[nodiscard]] auto size() const noexcept -> std::size_t { return size_; }
private:
    char* output_;
    std::size_t size_ = 0;
};
void encode(SequenceWriter&, Cursor);
void encode(SequenceWriter&, MoveCursor);
void encode(SequenceWriter&, BlankRow);
void encode(SequenceWriter&, BlankScreen);
void encode(SequenceWriter&, ResetStyle);
void encode(SequenceWriter&, Trait);
void encode(SequenceWriter&, Traits);
void encode(SequenceWriter&, ColorFG const&);
void encode(SequenceWriter&, ColorBG const&);
void encode(SequenceWriter&, Brush const&);
void encode(SequenceWriter&, ScreenBuffer);
void encode(SequenceWriter&, CursorMode);
void encode(SequenceWriter&, MouseReporting);
void encode(SequenceWriter&, FocusReporting);
void encode(SequenceWriter&, BracketedPaste);

// Adapt once, on the stack, so quantization is not repeated by the two passes.
struct PreparedBrush {
    std::optional<ColorBG> background;
    std::optional<ColorFG> foreground;
    std::optional<Traits> traits;
};
void encode(SequenceWriter&, PreparedBrush const&);
template<class Command>
void encode(SequenceWriter& output, std::optional<Command> const& command)
{ if (command) encode(output, *command); }
void validate_profile(TerminalProfile const&);
auto prepare(TerminalProfile const&, Cursor const&) -> std::optional<Cursor>;
auto prepare(TerminalProfile const&, MoveCursor const&) -> std::optional<MoveCursor>;
auto prepare(TerminalProfile const&, BlankRow const&) -> std::optional<BlankRow>;
auto prepare(TerminalProfile const&, BlankScreen const&) -> std::optional<BlankScreen>;
auto prepare(TerminalProfile const&, ResetStyle const&) -> std::optional<ResetStyle>;
auto prepare(TerminalProfile const&, Traits const&) -> std::optional<Traits>;
auto prepare(TerminalProfile const&, ColorFG const&) -> std::optional<ColorFG>;
auto prepare(TerminalProfile const&, ColorBG const&) -> std::optional<ColorBG>;
auto prepare(TerminalProfile const&, ScreenBuffer const&) -> std::optional<ScreenBuffer>;
auto prepare(TerminalProfile const&, CursorMode const&) -> std::optional<CursorMode>;
auto prepare(TerminalProfile const&, MouseReporting const&) -> std::optional<MouseReporting>;
auto prepare(TerminalProfile const&, FocusReporting const&) -> std::optional<FocusReporting>;
auto prepare(TerminalProfile const&, BracketedPaste const&) -> std::optional<BracketedPaste>;
auto prepare(TerminalProfile const&, Trait) -> std::optional<Traits>;
auto prepare(TerminalProfile const&, Brush const&) -> PreparedBrush;
template<Escapable... Commands>
auto prepare_pack(TerminalProfile const& profile, Commands const&... commands)
{
    validate_profile(profile);
    return std::tuple{prepare(profile, commands)...};
}
template<class Tuple>
void encode_pack(SequenceWriter& output, Tuple const& commands)
{
    std::apply([&](auto const&... command) { (encode(output, command), ...); }, commands);
}
} // namespace detail

// Preserve the prefix on validation, size, or allocation failure. A reserved
// destination needs no allocation, and no intermediate strings are constructed.
template<Escapable... Commands>
void append_escape(std::string& output, Commands const&... commands)
{
    detail::SequenceWriter measure;
    (detail::encode(measure, commands), ...);
    auto const required = measure.size();
    auto const prefix = output.size();
    if (required > output.max_size() - prefix)
        throw std::length_error{"escape: encoded output exceeds string size limit"};
    if (required == 0) return;
    output.resize(prefix + required);
    detail::SequenceWriter writer{output.data() + prefix};
    (detail::encode(writer, commands), ...);
}

template<Escapable... Commands>
[[nodiscard]] auto escape(Commands const&... commands) -> std::string
{
    std::string output;
    append_escape(output, commands...);
    return output;
}

// All-or-nothing encoding; no terminator and no heap allocation. Insufficient
// capacity returns the exact required size and leaves every destination byte intact.
template<Escapable... Commands>
[[nodiscard]] auto escape_to(std::span<char> output, Commands const&... commands) -> EncodeResult
{
    detail::SequenceWriter measure;
    (detail::encode(measure, commands), ...);
    auto const required = measure.size();
    if (required > output.size()) return {0, required};
    detail::SequenceWriter writer{output.data()};
    (detail::encode(writer, commands), ...);
    return {required, required};
}

// Explicit capability-aware overloads. The prepared pack is local storage;
// neither validation/adaptation nor bounded/reserved encoding allocates.
template<Escapable... Commands>
void append_escape(std::string& output, TerminalProfile const& profile, Commands const&... commands)
{
    auto const prepared = detail::prepare_pack(profile, commands...);
    detail::SequenceWriter measure;
    detail::encode_pack(measure, prepared);
    auto const required = measure.size();
    auto const prefix = output.size();
    if (required > output.max_size() - prefix)
        throw std::length_error{"escape: encoded output exceeds string size limit"};
    if (required == 0) return;
    output.resize(prefix + required);
    detail::SequenceWriter writer{output.data() + prefix};
    detail::encode_pack(writer, prepared);
}
template<Escapable... Commands>
[[nodiscard]] auto escape(TerminalProfile const& profile, Commands const&... commands) -> std::string
{
    std::string output;
    append_escape(output, profile, commands...);
    return output;
}
template<Escapable... Commands>
[[nodiscard]] auto escape_to(std::span<char> output, TerminalProfile const& profile,
                             Commands const&... commands) -> EncodeResult
{
    auto const prepared = detail::prepare_pack(profile, commands...);
    detail::SequenceWriter measure;
    detail::encode_pack(measure, prepared);
    auto const required = measure.size();
    if (required > output.size()) return {0, required};
    detail::SequenceWriter writer{output.data()};
    detail::encode_pack(writer, prepared);
    return {required, required};
}

// Existing convenience spellings remain pure wrappers.
[[nodiscard]] inline auto clear_traits() -> std::string { return escape(Traits{}); }
[[nodiscard]] inline auto escape_fg(Color color) -> std::string { return escape(fg(color)); }
[[nodiscard]] inline auto escape_bg(Color color) -> std::string { return escape(bg(color)); }

} // namespace esc
