# Escape interface proposal

Status: proposed contract for the `refactor/terminal-api` branch. This document
describes the intended interface, not the interface currently implemented. It is
the starting point for review and behavior tests. No implementation or benchmark
results are implied by the signatures below.

## 1. Design goals and boundaries

Escape is a C++20 library for encoding terminal commands, handling terminal input,
and managing terminal sessions. The design priorities are:

1. Simple, obvious ordinary usage.
2. Pure value transformations wherever possible; explicit ownership of state
   where incremental parsing or operating-system interaction requires it.
3. Readable implementation with deliberate control over allocation and I/O.
4. Broad compatibility through explicit capabilities and documented fallbacks.
5. Practical advanced APIs in their own places, rather than hidden behavior in
   convenience calls.

The library does not implement a widget toolkit, screen diff engine, layout
engine, terminal emulator, or a new wire protocol. It aims to be a modern terminal
control library using existing protocols. A capability claim is a statement about
the selected terminal connection, not a guarantee about every emulator.

The implementation targets Linux, macOS, and Windows 10/11 session backends.
Encoding, text conversion, capability resolution, and input parsing are portable
C++20. Platform implementation and live terminal validation are separate claims.
The public value model must not depend on POSIX or Windows console headers.

## 2. Everyday usage

### A small output-only program

```cpp
#include <esc/esc.hpp>

int main()
{
    esc::write(esc::escape(esc::fg(esc::XColor::Cyan)), "Hello", esc::escape(esc::ResetStyle{}), "\n");
    esc::flush();
}
```

No terminal initialization is necessary to encode commands or write bytes.
`escape` is a pure encoder. This example deliberately requests terminal control
bytes; applications wanting automatic capability handling use a terminal profile.

### An interactive program

```cpp
using namespace std::chrono_literals;

auto terminal = esc::Terminal::open(esc::TerminalOptions::interactive());
terminal.send(esc::Cursor{0, 0}, esc::Brush{.foreground = esc::XColor::Cyan});
terminal.write("Hello");
terminal.flush();

auto result = terminal.read(100ms);
if (result.status == esc::ReadStatus::Event) {
    std::visit(handle_event, *result.event);
}
terminal.close(); // Optional, but reports cleanup errors; destructor is best effort.
```

The terminal owns the session state and parser. It borrows the application's
streams. Construction does not terminate the process or install termination
signal handlers. Mode changes required to open the session are flushed before
`open` returns; ordinary drawing is flushed by the application.

### Reusing a frame buffer

```cpp
std::string frame;
frame.reserve(16 * 1024);

// Each frame:
frame.clear();
esc::append_escape(frame, terminal.profile(), esc::Cursor{0, 0}, brush);
frame.append("Hello");
esc::append_escape(frame, terminal.profile(), esc::ResetStyle{});
terminal.write(frame);
terminal.flush();
```

The convenience and reusable-buffer paths have the same semantics. Buffer reuse
is explicit. No API borrows a hidden scratch string or consults global style state.

## 3. Names, headers, and common conventions

Public types use the existing `PascalCase` convention; functions use `snake_case`.
Keep `fg`, `bg`, `foreground`, and `background` as equivalent helpers. Avoid new
aliases unless they preserve an established, useful spelling.

| Header | Responsibility |
| --- | --- |
| `esc/color.hpp`, `esc/trait.hpp`, `esc/brush.hpp` | Style values |
| `esc/point.hpp`, `esc/area.hpp` | Coordinates and dimensions |
| `esc/sequence.hpp` | Command values and encoding |
| `esc/text.hpp`, `esc/glyph.hpp` | Unicode conversion and styled code points |
| `esc/capabilities.hpp` | Profiles, evidence, and fallback policy |
| `esc/event.hpp`, `esc/parser.hpp` | Input values and incremental parsing |
| `esc/io.hpp` | Byte output and stream wrappers |
| `esc/terminal.hpp` | Session lifetime, settings, reads, and dimensions |
| `esc/esc.hpp` | Convenience umbrella for public headers |

Every header is self-contained. Internal helpers remain in `esc::detail` and are
not compatibility promises. No environment reads, stream changes, or signal
registration occur during static initialization or header inclusion.

Conventions throughout this document:

- An owned return value remains valid independently of later calls.
- An input view is borrowed for the duration of a synchronous call unless
  explicitly stated otherwise.
- Returned views name caller-owned or object-owned storage, with documented
  invalidation. No generated output returns a view into a temporary.
- Invalid public values produce `std::invalid_argument`, not undefined behavior.
- Resource and operating-system failures produce `std::system_error` with an
  operation-specific diagnostic and the original error code.
- Unsupported requests under strict policy produce `UnsupportedFeature`, a
  subclass of `std::runtime_error` with a `Feature feature() const noexcept`
  accessor. `Feature` names `Control`, `Colors`, `Traits`, `AlternateScreen`,
  `CursorVisibility`, `Mouse`, `Focus`, or `BracketedPaste`.
- Allocation exceptions propagate. Pure encoding and conversion calls are
  independent and safe to use concurrently on independent destinations.
- Session and parser instances require external synchronization. A terminal
  input stream has one reader; writers sharing an output stream must coordinate
  whole frames if interleaving is unacceptable.
- `[[nodiscard]]` applies to returned values and status objects; pure operations
  are `constexpr` where C++20 permits it. Do not promise `noexcept` on operations
  that validate data, allocate, or perform I/O.

## 4. Coordinates and style values

### Coordinates

```cpp
struct Point { int x = 0; int y = 0; };
struct Area  { int width = 0; int height = 0; };
struct Cursor { int x = 0; int y = 0; };
struct MoveCursor { int dx = 0; int dy = 0; };
```

`Point` is a general signed offset/position value. Preserve equality and addition/
subtraction. Arithmetic requires representable results; it does not silently
saturate. `Cursor` is a distinct command, not an alias for `Point`: encoding an
arbitrary point should be deliberate. Add `constexpr auto cursor(Point) -> Cursor`.

Absolute cursor and mouse coordinates are zero-based. `Cursor` accepts
nonnegative values and converts to one-based wire coordinates using a wide
intermediate, including `INT_MAX` without signed overflow. Encoding does not
query screen size or clamp to it. Terminal-side clamping is outside the encoder's
contract. This assumes normal origin mode; Escape does not silently change it.

`MoveCursor` encodes vertical movement first, then horizontal. Zero components
emit nothing. Negative deltas use a wide intermediate to handle `INT_MIN`.
The command does not claim to wrap or scroll: actual cursor boundaries depend
on the terminal's current modes.

`Area` permits zero to represent an unknown or empty size. Negative dimensions
are invalid when supplied to an API consuming dimensions. Geometry arithmetic
and text layout beyond these values are outside this refactor.

### Colors

Retain `RGB`, `HSL`, `TrueColor`, `TColor`, `XColor`, `TermColor`, `Color`,
`ColorFG`, `ColorBG`, `fg`, and `bg`. Preserve named indexed colors.

```cpp
enum class TermColor : bool { Default };
struct XColor { std::uint8_t value; /* existing named constants */ };
struct RGB { std::uint8_t red, green, blue; /* constructors below */ };
struct HSL { std::uint16_t hue; std::uint8_t saturation, lightness; };
struct TrueColor { std::uint8_t red, green, blue; /* constructors below */ };
using TColor = TrueColor;
using Color = std::variant<XColor, TrueColor, TermColor>;
struct ColorFG { Color value; };
struct ColorBG { Color value; };
```

The byte fields hold exactly 0..255. Braced construction must reject narrowing;
do not add an integer overload that silently wraps. `RGB` accepts three bytes or
an explicit packed `std::uint32_t` of 0x000000..0xFFFFFF; larger packed values are
invalid. `TrueColor` accepts `RGB`, `HSL`, or an explicit packed RGB integer,
preserving the convenient `TColor{0xF02127}` spelling. HSL ranges are hue 0..359 and saturation/
lightness 0..100. Conversion validates input, normalizes the computed hue into
0..359, and rounds channels to the nearest representable integer, ties upward.
Hue is zero for achromatic RGB. RGB/HSL round trips are approximate and tested
with documented tolerances; exact RGB bytes are preserved by RGB construction.

`XColor` identifies a palette slot, not a universally fixed RGB value. Slots
0..15 are configurable system colors; 16..231 are the conventional 216-color
cube and 232..255 the conventional grayscale ramp when the selected profile
uses the xterm palette. No promise is made about a terminal's customized palette.

All these value types support value equality. Invalid casted enum values and
valueless color variants are rejected by encoding before modifying a destination.

### Traits and brushes

Retain the existing trait names. Define `Standout` as the semantic combination
of `Bold | Inverse`, rather than a separate wire attribute. `Traits` stores a
canonical bitmask with value equality. `None` is the empty set. Construction and
combination reject unknown bits. `contains(None)` means the set is empty;
`contains(Standout)` means both component bits are present.

`Traits` exposes a default constructor, construction from `Trait`, `contains`,
`data`, equality, and pure `operator|` combination. Named mutating `insert` and
`remove` operations remain for explicit mutation and return `Traits&`.
Inserting or removing None is a no-op. `remove_trait(Standout)` removes both
components. If both underline forms are
requested, double underline takes precedence during encoding; the value retains
the requested flags so capability fallback can select single underline.

```cpp
struct Brush {
    Color background = TermColor::Default;
    Color foreground = TermColor::Default;
    Traits traits = {};
};
```

`Brush` is a complete style value. Encoding it sets both colors and replaces the
set of traits modeled by this library. Encoding `fg` or `bg` changes only that
channel. Encoding a `Trait` or `Traits` replaces the modeled traits while
preserving both colors. Trait encoding is not additive. `ResetStyle` requests
the terminal's full SGR reset and can reset attributes outside Escape's model.

No `traits()`, `foreground_color()`, or `background_color()` global getters remain.
Generated output is not evidence of output being written, and written output is
not evidence of the terminal's actual current state.

## 5. Commands and byte encoding

### Command vocabulary

The initial drawing vocabulary is deliberately finite:

```cpp
enum class Erase { ToEnd, ToBeginning, All };
struct BlankRow    { Erase extent = Erase::All; };
struct BlankScreen { Erase extent = Erase::All; };
struct ResetStyle {};
```

Drawing commands are `Cursor`, `MoveCursor`, `BlankRow`, `BlankScreen`, `Trait`,
`Traits`, `ColorFG`, `ColorBG`, `Brush`, and `ResetStyle`. `DrawingCommand<T>`
checks the unqualified type against this closed set. Bare `Color`, integers,
`Point`, strings, and unrelated implicitly convertible types are not commands.

Erase commands include the current position, do not move the cursor, and do not
clear scrollback. `BlankScreen{}` erases the visible display, not its history.
Their effect on blank-cell background is subject to the terminal's erase behavior.

Terminal protocol mode values are also `Escapable`, for advanced byte encoding:

```cpp
enum class ScreenBuffer { Normal, Alternate };
enum class CursorMode { Show, Hide };
enum class MouseMode { Off, Basic, Drag, Move };
enum class MouseProtocol { None, Legacy, Sgr, Urxvt };
struct MouseReporting { MouseMode mode; MouseProtocol protocol; };
enum class FocusReporting { Off, On };
enum class BracketedPaste { Off, On };
```

`Escapable<T>` is the union of drawing commands and these five mode commands
(`ScreenBuffer`, `CursorMode`, `MouseReporting`, `FocusReporting`,
`BracketedPaste`). A bare `MouseMode` lacks the required protocol and is not
encodable. `MouseReporting{Off, None}` is valid; an enabled mode with `None` is
invalid. Echo, canonical input, and signal generation are operating-system
settings and have no escape-string overload.

### Public encoding functions

```cpp
template<Escapable... Commands>
auto escape(Commands const&... commands) -> std::string;

template<Escapable... Commands>
void append_escape(std::string& output, Commands const&... commands);

struct EncodeResult {
    std::size_t written;
    std::size_t required;
};

template<Escapable... Commands>
auto escape_to(std::span<char> output, Commands const&... commands) -> EncodeResult;
```

All three permit zero commands: empty string, no append, or `{0, 0}` respectively.
They support const values, lvalues, and temporaries with the same behavior.
Composition is left to right and equals concatenation of the canonical encoding
of each command. Do not implicitly reorder, merge, or omit repeated commands.

`escape` always returns an owning `std::string`, even for literal sequences.
There is no inconsistent string/view return family. Small-string optimization
is implementation-dependent and is not the allocation guarantee.

`append_escape` preserves the existing prefix. It validates the entire pack,
checks total size, and obtains capacity before changing logical contents. Invalid
input, unsupported requests, overflow, or allocation failure leaves contents
unchanged. There are no intermediate heap-allocated strings; no allocation occurs
when destination capacity suffices. A two-pass size/write implementation is
acceptable; do not encode an entire frame into a second temporary buffer.

`escape_to` writes at offset zero without a null terminator. It validates first.
On insufficient capacity it returns `{0, required}` and leaves every byte
unchanged. On success it returns `{required, required}` and preserves trailing
bytes. A zero-length encoding succeeds in an empty span. Invalid commands throw
`std::invalid_argument`; size overflow throws `std::length_error` before writes.
It performs no heap allocations on successful valid input or capacity failure.

```cpp
std::array<char, 128> storage;
auto result = esc::escape_to(storage, esc::Cursor{4, 2}, brush);
if (result.written == result.required) {
    esc::write(std::string_view{storage.data(), result.written});
}
```

No public fixed-capacity sequence return type is introduced in this proposal.
The bounded destination API supplies the allocation-free path without a capacity
constant spreading through caller code. A future owning small-buffer value must
justify itself with actual usage and measurements.

### Canonical encodings

Use seven-bit introducers (`ESC [`), decimal numbers without leading zeros,
and explicit parameters. Encoding is locale-independent. Baseline wire semantics
follow the [xterm control reference](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html);
the API and serialization choices below are Escape's proposed contract.

| Command | Canonical bytes (`ESC` means byte 0x1B) |
| --- | --- |
| `Cursor{x, y}` | `ESC [ {y+1} ; {x+1} H` |
| `MoveCursor{dx, dy}` | Nonzero vertical component using `A`/`B`, then horizontal using `D`/`C` |
| `BlankRow{extent}` | `ESC [ {0/1/2} K` for end/beginning/all |
| `BlankScreen{extent}` | `ESC [ {0/1/2} J` for end/beginning/all |
| `fg(Default)` / `bg(Default)` | `ESC [ 39 m` / `ESC [ 49 m` |
| Indexed color 0..7 | `ESC [ {30+i/40+i} m` |
| Indexed color 8..15 | `ESC [ {90+i-8/100+i-8} m` |
| Indexed color 16..255 | `ESC [ {38/48} ; 5 ; {i} m` |
| True color | `ESC [ {38/48} ; 2 ; {r} ; {g} ; {b} m` |
| `ResetStyle{}` | `ESC [ 0 m` |

Trait replacement emits `ESC[22;23;24;25;27;28;29` followed by the enabled
parameters and `m`. Parameters are emitted once, in this order: Bold=1, Dim=2,
Italic=3, Underline=4 or DoubleUnderline=21, Blink=5, Inverse=7, Invisible=8,
CrossedOut=9. An empty set emits only the clearing parameters. This clears the
modeled traits, not every possible SGR attribute. `Brush` concatenates background,
foreground, and trait encodings in that order.

Mode commands serialize each private-mode change as its own sequence. Screen
uses 1049, visibility 25, focus 1004, and paste 2004. Mouse replacement disables
tracking modes 1000,1002,1003 and encodings 1005,1006,1015,1016 in that order,
then enables the selected encoding (1006 for Sgr, 1015 for Urxvt; none for Legacy),
then tracking (1000 Basic,1002 Drag,1003 Move). Off emits only the disabling
sequences. This operation assumes ownership of mouse-reporting configuration;
it cannot be mixed with another mouse-mode controller.

Raw encoding never checks environment or capabilities. For example, requesting
true color emits true-color bytes regardless of which terminal might receive them.

Retain `clear_traits()` as a pure compatibility wrapper for `escape(Traits{})`.
Retain `escape_fg` and `escape_bg` as thin wrappers for `escape(fg(...))` and
`escape(bg(...))`; they have no independent state or serialization rules.

## 6. Capability-aware encoding

### Explicit profiles

```cpp
enum class ColorSupport { None, Basic8, Basic16, Indexed256, TrueColor };
enum class FallbackPolicy { BestEffort, Strict };

struct TerminalProfile {
    bool control = false;
    ColorSupport colors = ColorSupport::None;
    Traits traits = {};
    bool alternate_screen = false;
    bool cursor_visibility = false;
    MouseProtocol mouse_protocol = MouseProtocol::None;
    MouseMode maximum_mouse_mode = MouseMode::Off;
    bool focus_reporting = false;
    bool bracketed_paste = false;
    FallbackPolicy fallback = FallbackPolicy::BestEffort;
    std::array<RGB, 16> system_palette = conventional_system_palette();
};
```

`conventional_system_palette()` returns an array by value, is `constexpr`, and
uses these RGB values in slot order:
`000000,800000,008000,808000,000080,800080,008080,C0C0C0,808080,FF0000,00FF00,FFFF00,0000FF,FF00FF,00FFFF,FFFFFF`.
The palette supplies approximation inputs; it does not change terminal colors.
A caller knowing a customized system palette can supply it in the profile.

`TerminalProfile{}` is a plain-output profile with no assumed control support.
Indexed256 and TrueColor profiles assume the conventional xterm cube and ramp;
other indexed layouts, including 88-color terminals, need a later adapter and
must not be mislabeled Indexed256. Validate enum values and these invariants:
control=false implies no color/trait/protocol features; a None mouse protocol
requires maximum_mouse_mode=Off; a non-None protocol requires at least Basic.

Capability-aware counterparts take the profile explicitly:

```cpp
template<Escapable... Commands>
auto escape(TerminalProfile const& profile, Commands const&... commands) -> std::string;

template<Escapable... Commands>
void append_escape(std::string& output, TerminalProfile const& profile,
                   Commands const&... commands);

template<Escapable... Commands>
auto escape_to(std::span<char> output, TerminalProfile const& profile,
               Commands const&... commands) -> EncodeResult;
```

These share all ownership, ordering, capacity, and exception guarantees with raw
encoding. The profile is borrowed during the call. No mutable encoder object,
virtual sink, environment lookup, terminal query, or allocation-backed cache is
needed in a rendering loop. Profile-aware encoding may deliberately produce
different bytes from raw encoding.

### Fallback rules

| Request | BestEffort | Strict |
| --- | --- | --- |
| Supported feature | Encode | Encode |
| Style on a plain-output profile | Omit style bytes, including ResetStyle | Reject nonempty traits or nondefault colors; default styles/reset emit nothing |
| Cursor movement or erase without control support | Throw UnsupportedFeature | Same |
| Unsupported nonessential trait | Remove it | Throw |
| Double underline unsupported, single underline supported | Use single underline | Throw |
| Colors beyond the selected depth | Approximate as defined below | Throw |
| Protocol mode feature unsupported | Throw | Throw |
| Mouse protocol differs from the profile's supported protocol | Use the supported protocol if requested tracking level is supported | Throw |
| Mouse tracking level exceeds supported maximum | Throw; no silent loss of motion events | Throw |

Mouse protocol mismatch rules apply only to enabled reporting. When the mouse
feature is supported, Off disables the owned modes without selecting an encoding.

Default colors are not an approximation and never become black or white. Within
a control profile without color support, default color commands emit nothing;
nondefault colors are dropped under BestEffort and rejected under Strict.
Trait replacement emits the usual clearing sequence when traits are supported;
when no traits are supported it emits nothing under BestEffort. Strict rejects
requested traits absent from the supported set. Clearing parameters for absent
traits may be harmlessly ignored by the selected profile; profiles with known
conflicting interpretations require an adapter and are outside the baseline.

Quantization is pure, deterministic, independent of locale, and never uses bold
as a substitute for a bright color. Choose the candidate with smallest squared
Euclidean distance in encoded RGB channels; ties choose the lowest palette index.
Basic8/16 candidates use the supplied system_palette. Indexed256 candidates use
all 256 entries, with the supplied first 16 and the conventional remaining slots.
The cube uses levels 0,95,135,175,215,255 in RGB index order; grayscale slot i
uses channel value 8+10*(i-232). This metric is a specified inexpensive approximation,
not a perceptual color-matching claim.

An already-supported indexed slot is preserved. For Basic8, bright slots 8..15
fold to 0..7 under BestEffort; Strict rejects them. Unsupported higher indexed
slots are converted using the conventional cube/ramp, then quantized. TrueColor
is retained only with TrueColor support. Approximating to None drops the color
command. Profiles cannot infer the actual display color of an unknown custom slot.

No stateful style deduplication occurs. A future renderer may compare explicit
style values, but it must account for raw writes and failed writes. Such a renderer
is outside this first interface contract.

### Environment resolution

```cpp
struct Environment {
    std::string term;
    std::string colorterm;
    bool no_color = false;
};
enum class EvidenceSource { Default, Environment, KnownTerminal, Explicit };
struct CapabilityDecision {
    Feature feature;
    EvidenceSource source;
    std::string explanation;
};
struct ProfileReport {
    TerminalProfile profile;
    std::vector<CapabilityDecision> decisions;
};

auto snapshot_environment() -> Environment;
auto resolve_profile(Environment const&, bool output_is_terminal) -> ProfileReport;
```

The snapshot owns its strings, reads current environment when called, and sets
no_color only when NO_COLOR is present and nonempty. Resolution is pure. It uses
an explicit, versioned table of known terminal profiles with fixture tests.
Unknown or empty TERM and TERM=dumb resolve to plain output. Do not infer true
color from the substring `256color`. Known profiles may supply Indexed256;
COLORTERM=truecolor or 24bit can raise color support to TrueColor only for a
recognized control-capable terminal. Matching is case-sensitive and exact.

Redirected output resolves to plain output. NO_COLOR removes color support but
does not remove cursor control or traits. Each decision records evidence and
distinguishes a heuristic from a user assertion. Table entries require a cited
protocol basis or a captured terminal fixture; table contents are data to be
reviewed separately, not an unsupported compatibility claim in this document.

An explicit profile supplied to Terminal::open takes precedence over resolution,
including NO_COLOR; that is a deliberate application choice. The library never
looks through SSH or a multiplexer to assume support greater than the immediate
connection reports. Active probing and runtime terminfo expansion are deferred.
There is no hidden probe on construction, and unknown terminals remain usable
through an explicit profile. A later probe API must define deadlines, reply
demultiplexing, and preservation of ordinary input before it is added.

## 7. Text and glyphs

### Unicode conversion

Promote a small, explicit public conversion API instead of asking users to call
detail helpers. Byte strings are UTF-8 only at APIs named for UTF-8; byte output
itself accepts arbitrary bytes. Conversions do not normalize or sanitize text.

```cpp
class UnicodeError : public std::runtime_error {
public:
    auto offset() const noexcept -> std::size_t;
};

auto utf8_to_utf32(std::string_view input) -> std::u32string;
void append_utf32(std::u32string& output, std::string_view utf8);
auto utf32_to_utf8(std::u32string_view input) -> std::string;
void append_utf8(std::string& output, std::u32string_view utf32);

struct Utf8CodePoint {
    std::array<char, 4> bytes;
    std::uint8_t size;
    auto view() const noexcept -> std::string_view;
};
auto encode_utf8(char32_t scalar) -> Utf8CodePoint;
```

All conversions are strict: reject malformed or truncated UTF-8, overlong forms,
surrogates, and values above U+10FFFF. U+0000 and Unicode noncharacters are valid
scalar values and are preserved. See the [Unicode conformance and encoding
requirements](https://www.unicode.org/versions/Unicode17.0.0/core-spec/chapter-3/).
UnicodeError.offset identifies the start of the first ill-formed UTF-8 subsequence
(including a truncated prefix), or the first invalid UTF-32 element in input.
For encode_utf8 it is zero. UTF-8 indexing uses size_t or checked wider arithmetic;
strings larger than INT32_MAX are not truncated to an ICU index type.

Append conversion validates before modifying output and provides the strong
content guarantee. Embedded nulls survive round trips. `Utf8CodePoint` owns its
bytes, has size 1..4, and is not implicitly convertible to a view. Its view
is valid only while that object lives; unused array bytes are zero, with no
promise of a terminator after four encoded bytes. Encoding one scalar allocates
nothing. `std::u8string` overloads, replacement policies, and streaming conversion
iterators are deferred until there is a concrete need.

### Styled code points

```cpp
struct Glyph { char32_t symbol = U' '; Brush brush = {}; };
using GlyphString = std::vector<Glyph>;

auto glyphs(std::string_view utf8, Brush brush = {}) -> GlyphString;
auto glyphs(std::u32string_view utf32, Brush brush = {}) -> GlyphString;
void append_glyphs(GlyphString&, std::string_view utf8, Brush brush = {});
void append_glyphs(GlyphString&, std::u32string_view utf32, Brush brush = {});
```

Glyph is a styled Unicode scalar, not a promise of one terminal cell. Construction
as an aggregate is cheap; APIs consuming glyphs validate their symbols with UnicodeError before mutation. Combining
marks and wide characters remain separate scalars. Grapheme segmentation, shaping,
emoji width, normalization, and terminal cell measurement are outside this refactor.
Do not expose a misleading universal width() function.

The existing GlyphString concept becomes `GlyphRange`: a forward range of Glyph.
`GlyphString` now names the ordinary owning vector. Keep efficient customization
through ranges and a named iterator-based `append_glyphs_to(output_iterator,
input_view, brush={})` for both UTF-8 and UTF-32. It returns the advanced iterator,
validates all input first, and may leave a partial destination only if insertion
itself throws. There is no implicit allocation by the iterator API.

Attributes are ColorFG, ColorBG, Trait, Traits, RemoveTrait, and Brush. Attribute
application replaces the selected color, adds/removes traits, or replaces the
complete brush. This differs deliberately from encoding Traits, which replaces
the terminal's modeled traits. `Brush` and `Glyph` pipe operations return values.
`U'x' | fg(...)` produces a Glyph; narrow character pipes accept ASCII only,
including signed/unsigned char ASCII, and reject bytes above 127. Isolated
char8_t/char16_t code units are not accepted as Unicode scalars. Use strings for
UTF-8 and char32_t for individual scalars.

```cpp
auto styled(GlyphRange auto const& input, Attribute auto attribute) -> GlyphString;
void style_in_place(GlyphRange auto& input, Attribute auto attribute);
```

`styled` always owns a new result and leaves its input unchanged. `style_in_place`
requires mutable glyph references, performs no allocation, validates the
attribute and input scalars before applying it, and names the mutation. GlyphString pipes return a
value even for an lvalue; they never silently modify an lvalue range. Rvalues may
reuse owned storage. String-view pipes decode strictly and return GlyphString.

Retain concatenation for GlyphString, Glyph, ASCII char, char32_t, UTF-8 views,
and UTF-32 views: `+` returns an owned vector in left-to-right order; `+=` appends
and returns GlyphString&. Additionally, `operator+=(GlyphString&, GlyphRange auto const&)`
accepts borrowed glyph ranges and returns GlyphString&. Self-append duplicates the original contents exactly
once. Borrowed glyph ranges aliasing the destination must likewise be handled
before reallocation. Do not add operators to arbitrary standard containers.
Named append APIs provide the generic path. Allocating operations validate text
and attributes before committing changes to an existing vector.

## 8. Input event model

Separate Unicode scalars, named keys, and modifiers. Do not pack modifier bits
into a Unicode-valued enum or infer physical keys from text the terminal cannot
distinguish.

```cpp
enum class KeyCode {
    Escape, Enter, Tab, Backspace,
    ArrowUp, ArrowDown, ArrowLeft, ArrowRight,
    Home, End, Insert, Delete, PageUp, PageDown,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    F13, F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24,
    KeypadEnter, Keypad0, Keypad1, Keypad2, Keypad3, Keypad4,
    Keypad5, Keypad6, Keypad7, Keypad8, Keypad9,
    KeypadDecimal, KeypadPlus, KeypadMinus, KeypadMultiply, KeypadDivide
};
using Key = std::variant<char32_t, KeyCode>;
struct Modifiers { bool shift=false, ctrl=false, alt=false, meta=false; };
struct KeyPress { Key key; Modifiers modifiers={}; };

enum class MouseButton { Left, Middle, Right, Button4, Button5,
                         Button6, Button7, Button8, Button9, Button10, Button11 };
struct MousePress { Point at; MouseButton button; Modifiers modifiers={}; };
struct MouseRelease { Point at; std::optional<MouseButton> button; Modifiers modifiers={}; };
struct MouseMove { Point at; std::optional<MouseButton> held_button; Modifiers modifiers={}; };
struct MouseWheel { Point at; int dx=0; int dy=0; Modifiers modifiers={}; };
struct Resize { Area size; };
struct Focus { bool focused; };
struct PasteChunk { std::string bytes; bool first; bool last; bool aborted=false; };

enum class InputProblem { Unsupported, Malformed, TooLong, Truncated, InvalidUtf8 };
struct UnknownInput { std::string bytes; InputProblem problem; bool final=true; };

using Event = std::variant<KeyPress, MousePress, MouseRelease, MouseMove,
                           MouseWheel, Resize, Focus, PasteChunk, UnknownInput>;
```

All event types support value equality. Positions are zero-based cell coordinates,
not pixels. Wheel dx is positive right, dy positive down; conventional vertical
wheel ticks are +/-1. Release.button can be empty for protocols that do not
identify the released button. Motion reports only the held button represented
by the protocol, not a reconstructed full physical button state.

For legacy input, ASCII characters and valid UTF-8 produce KeyPress with a scalar.
TAB produces KeyCode::Tab, CR produces Enter, bytes BS and DEL produce Backspace,
and standalone ESC produces Escape. Other C0 bytes, including LF and NUL, remain
literal scalar values with no invented modifier flags. Legacy uppercase text
does not imply Shift; Ctrl-I cannot be distinguished from Tab. Applications
needing physical identity require a future explicitly enabled keyboard protocol.
Legacy autorepeat produces more presses and cannot be identified as repeat.

Named key values are unique, independent of Unicode code points. Named keys do
not implicitly convert to characters. A convenience `text_scalar(Key const&) ->
std::optional<char32_t>` returns the scalar alternative, including control scalars;
it does not guess text for named keys or remove modifiers.

PasteChunk contains original bytes, not validated text or key events. Chunking
may split a UTF-8 character; consumers needing text validate/reassemble it.
Bracketed-paste delimiters are not included. Empty paste produces one empty event
with first=true,last=true. EOF during paste yields a final aborted chunk. The
delimiter protocol cannot represent its exact ending delimiter inside payload;
Escape cannot remove that wire ambiguity.

UnknownInput preserves unrecognized or malformed bytes rather than inventing a
key or silently dropping them. Large units arrive as consecutive bounded chunks;
final=false means another chunk of that same unit follows. A final empty chunk
is allowed to close an already-emitted unit. Unknown sequences are never written
back to the terminal by the library.

Linux console scancode access, elevated keyboard modes, release events, physical
modifier keys, and enhanced keyboard protocols are deferred. Remove the current
KeyMode::Raw path from the portable interface rather than implying emulator
support for Linux console behavior.

## 9. Incremental parser

The parser owns its incomplete input, has no I/O or clock access, and accepts
arbitrarily fragmented reads. Explicit state is appropriate here; avoid copying
a parser state and a large input queue on every key merely to appear functional.
Semantic helpers inside the parser should remain pure.

```cpp
struct InputProtocols {
    MouseProtocol mouse = MouseProtocol::None;
    bool focus = false;
    bool bracketed_paste = false;
};
struct ParserOptions {
    std::chrono::milliseconds escape_timeout{25};
    std::chrono::milliseconds sequence_timeout{250};
    std::size_t sequence_limit = 256;
    std::size_t paste_chunk_size = 4096;
};
enum class ParseStatus { Event, NeedInput };
struct ParseResult {
    std::size_t consumed;
    ParseStatus status;
    std::optional<Event> event;
};
enum class PendingTimeout { Escape, Sequence };

class Parser {
public:
    explicit Parser(ParserOptions options={}, InputProtocols protocols={});
    auto next(std::span<char const> input) -> ParseResult;
    auto pending_timeout() const noexcept -> std::optional<PendingTimeout>;
    auto expire() -> std::optional<Event>;
    auto finish() -> std::optional<Event>;
    void configure(InputProtocols protocols);
    void reset() noexcept;
};
```

Limits are validated at construction: sequence_limit is 16..4096 bytes,
paste_chunk_size is 1..4096 bytes, and timeouts are nonnegative. Parser may
allocate its bounded storage once at construction; ordinary text, named-key,
and mouse parsing then allocate nothing. Owned paste/unknown events may allocate.
Do not impose an unbounded internal event queue or retain an entire large paste.

`next` returns at most one event. consumed counts bytes taken from the supplied
span, never bytes already buffered. Event status has a populated event; NeedInput
has none and has consumed all supplied bytes. Empty input can drain a pending
event. An Event with consumed=0 is valid when releasing buffered work, but repeated
calls must make progress and eventually NeedInput. Caller advances the span by
consumed and calls again. Only consumed bytes become owned by Parser; the input
span can otherwise be destroyed immediately after the call.

`configure` validates the protocol set and applies it to subsequent sequence
beginnings. An in-progress sequence retains the protocol interpretation under
which it began. It does not discard input. `reset` deliberately discards partial
input and EOF state while preserving options and configured protocols.

### Parsing contract

- Recognize normal CSI arrows/home/end and SS3 application cursor/keypad forms.
- Recognize the existing supported function-key tilde forms and SS3 F1..F4.
  Numeric tilde identifiers map 1/7 Home, 2 Insert, 3 Delete, 4/8 End,
  5 PageUp, 6 PageDown, 11..15 F1..F5, 17..21 F6..F10, 23..24 F11..F12,
  25..26 F13..F14, 28..29 F15..F16, and 31..34 F17..F20.
  F21..F24 are reserved named values until an enabled protocol supplies them.
- CSI Z is Shift+Tab. Modifier parameters 1..16 use modifier-1 with bit values
  Shift=1, Alt=2, Ctrl=4, Meta=8. Reject out-of-range parameters; do not guess.
- ESC followed by a complete scalar represents an Alt-modified scalar or named
  C0 key under the mappings above. Two ESC bytes resolve the first as Escape
  and start a new pending ESC. Recognized CSI/SS3 prefixes take precedence over
  Alt text; consequently legacy Alt+[ and Alt+O are ambiguous.
- Recognize mouse input only for the configured protocol: legacy X10 byte
  coordinates, SGR cell coordinates, or urxvt decimal coordinates. Validate
  positive wire positions, button flags, integer overflow, and event kind.
  Unknown flag combinations produce UnknownInput, not a guessed button.
- Recognize focus reports only when enabled. Recognize bracketed paste only
  when enabled; inside a paste, all payload is data until the ending delimiter.
- Complete unsupported CSI/SS3 units and OSC/DCS/APC/PM string units produce
  UnknownInput. Do not turn a terminal reply's payload into ordinary keys.
  String units end at ST; OSC additionally accepts BEL. Eight-bit C1 control
  introducers are not enabled in this UTF-8 baseline.
- An invalid UTF-8 prefix produces UnknownInput::InvalidUtf8. Preserve its
  bytes and leave a following valid scalar available for the next event.
- Handle splitting at every byte boundary, including within UTF-8, introducers,
  parameter fields, paste delimiters, and string terminators.
- Numeric parsing consumes the entire intended parameter and checks bounds.
  Use views/indices; parsing a parameter does not require a string allocation.
- On sequence-limit overflow emit bounded UnknownInput::TooLong chunks while
  remaining in that unit until its terminator, timeout, or EOF. Do not interpret
  the remaining payload as new keys merely because a buffer filled.

### Time and end of input

Parser reports which timeout its incomplete state needs; the reader supplies
time. Escape timeout starts on the initial ESC. Once recognized as a CSI/SS3 or
string unit, the sequence timeout is an absolute deadline from that unit's
introducer, not extended indefinitely by trickling bytes. Standalone ESC expires
to Escape. Incomplete recognized units expire to UnknownInput::Truncated.
An incomplete Alt UTF-8 prefix has a sequence deadline after ESC; an ordinary
UTF-8 prefix has no timing deadline and is resolved only by more input or EOF.
Paste payload has no escape timeout; EOF is its truncation boundary.

`expire` resolves the pending deadline state and otherwise returns nullopt.
It does not read a clock, so callers must invoke it only when that deadline is
due. `finish` enters EOF state and drains pending events, returning nullopt
once drained; repeated calls after draining return nullopt. A lone ESC at EOF resolves as KeyCode::Escape. It reports truncated
ordinary UTF-8 or recognized sequence input and closes an unfinished paste as aborted.
Calling next after finish is an invalid operation until reset.

Timeout defaults are policy choices, not claims about every connection. Applications
on slow remote links can increase them. Short read deadlines do not force parser
expiry or discard partially received input.

## 10. Output

```cpp
class Output {
public:
    explicit Output(std::FILE* stream=stdout);
    void write(char byte);
    void write(char32_t scalar);
    void write(std::string_view bytes);
    void write(std::u32string_view text);
    void write(char const* null_terminated);
    template<Writable... Values> void write(Values const&... values);
    void flush();
};

template<Writable... Values> void write(Values const&... values); // stdout
void flush();                                                   // stdout
```

`Writable` is restricted to char, char32_t, string/string_view, u32string/
u32string_view, and null-terminated char pointers/arrays or UTF-32 literal arrays. UTF-32
arrays use null-terminated semantics; an explicit u32string_view preserves embedded
nulls. No arbitrary integer
or bool becomes a character through implicit convertibility. String and u32string
arguments use their full sizes via views. Pointer/array overloads have conventional
C-string semantics; use an explicit view to include embedded nulls in an array.
Zero arguments are a no-op.

Output borrows a non-null FILE*, never closes it, and adds no private output
buffer. It is a lightweight copyable stream reference. Use the application's
existing stdio buffering; do not call setvbuf behind its back. On platforms with
text/binary stream translation, byte-exact use requires a binary stream supplied
by the application. No global terminal initialization is necessary.

write(view) writes exactly size() bytes, including embedded nulls, using length-
based block output. write(string) has identical semantics. char writes one raw
byte; char32_t and u32 views strictly encode Unicode through small bounded
chunks, without an entire intermediate string. write(nullptr) is invalid.
Validate all UTF-32 arguments before beginning a variadic write. Arguments are
written in order with no separators, newline insertion, or automatic flush.

A successful write means bytes were accepted by the C stream, possibly buffered;
it does not prove terminal display. flush checks buffered output errors. Handle
short transfers and errors, and throw system_error on failure; an I/O failure
can leave a prefix written and cannot be rolled back. This API does not promise
frame atomicity, one syscall per write, or safe replay after an error.

The byte guarantee is at the stream boundary. Existing OS terminal output
processing can transform text (for example, LF into CR/LF). Session opening
preserves that processing; an application requiring raw transport controls it
explicitly outside this initial interface. Do not confuse encoder byte tests
with the terminal line discipline's text-processing policy.

Do not interpret text as a format string, validate UTF-8 in raw byte writes, or
strip control bytes. Sanitizing untrusted display text is an application concern;
a separately named sanitizer could be added later without changing byte output.

## 11. Readers and terminal sessions

### Reader

```cpp
enum class ReadStatus { Event, Timeout, EndOfInput };
struct ReadResult {
    ReadStatus status;
    std::optional<Event> event;
};

class Reader {
public:
    explicit Reader(std::FILE* input=stdin, ParserOptions options={},
                    InputProtocols protocols={});
    auto read() -> ReadResult;
    auto read(std::chrono::milliseconds timeout) -> ReadResult;
    void configure(InputProtocols protocols);
};
```

Reader owns its parser, pending bytes, and deadlines; it borrows a non-null input
stream. It is movable and not copyable. The POSIX backend reads/polls the stream's
descriptor, so callers must not have unread stdio-buffered input or concurrently
use fread/getchar/iostreams on that input. This is a documented ownership rule,
not an attempt to change the application's stdio buffer settings. Reader also
works with pipes and files on the supported backends; Terminal adds TTY settings.

read() waits indefinitely for an event or EOF. Timed read rejects negative
durations; zero is a poll. Deadlines use a monotonic clock and include parsing,
not just the first byte. Buffered events are returned before Timeout or EOF.
Caller timeouts do not reset the parser, shorten its own ambiguity deadlines,
or force an incomplete sequence into a key event. Repeated timed reads preserve
all pending input. Expired parser deadlines are serviced before blocking again.

ReadStatus::Event has an event; the other statuses have none. EOF drains Parser
via finish before EndOfInput, which is then returned persistently. EINTR retries
with the original caller deadline; no deadline restarts. Other I/O errors throw
system_error. A hangup drains available bytes before final EOF. No implicit
output flush happens when reading. Reader performs bounded reads rather than
one syscall per byte, and queue capacity is bounded. configure has the parser's
sequence-boundary semantics and does not drop unread bytes.

### Session options

```cpp
enum class Echo { On, Off };
enum class InputBuffer { Canonical, Immediate };
enum class Signals { On, Off };

struct Streams { std::FILE* input=stdin; std::FILE* output=stdout; };
struct ProtocolState {
    ScreenBuffer screen_buffer = ScreenBuffer::Normal;
    CursorMode cursor = CursorMode::Show;
    MouseReporting mouse = {MouseMode::Off, MouseProtocol::None};
    FocusReporting focus = FocusReporting::Off;
    BracketedPaste paste = BracketedPaste::Off;
};
struct TerminalOptions {
    std::optional<Echo> echo;
    std::optional<InputBuffer> input_buffer;
    std::optional<Signals> signals;
    std::optional<ScreenBuffer> screen_buffer;
    std::optional<CursorMode> cursor;
    std::optional<MouseMode> mouse;
    std::optional<FocusReporting> focus;
    std::optional<BracketedPaste> paste;
    std::optional<TerminalProfile> profile;
    ProtocolState restore_modes = {};
    ParserOptions parser = {};
    std::optional<std::chrono::milliseconds> resize_interval = std::chrono::milliseconds{100};
    static auto interactive() -> TerminalOptions;
};
```

An absent setting preserves it. TerminalOptions{} captures a session without
deliberately changing input or protocol modes. interactive() requests immediate
input, echo off, the alternate screen, and a hidden cursor. It preserves signal
generation and leaves mouse, focus, and bracketed paste unchanged: optional
input features should be consciously enabled rather than required for opening
a basic interactive application. Parser timeout defaults apply in both cases.

The application supplies the known/assumed initial protocol state in restore_modes.
Default values describe a conventional normal-screen terminal. This state is
also the assumed state for modes left unchanged, so a caller inheriting enabled
mouse/paste/focus reporting supplies it to configure input interpretation correctly.
Do not pretend these are queried values. The selected profile must support any
enabled inherited input protocols. resize_interval=nullopt disables size polling;
otherwise it must be positive.

### Terminal interface

```cpp
class Terminal {
public:
    static auto open(TerminalOptions options={}) -> Terminal;
    static auto open(Streams streams, TerminalOptions options={}) -> Terminal;

    Terminal(Terminal&&) noexcept;
    auto operator=(Terminal&&) -> Terminal& = delete;
    Terminal(Terminal const&) = delete;
    auto operator=(Terminal const&) -> Terminal& = delete;
    ~Terminal() noexcept;

    auto profile() const -> TerminalProfile const&;
    auto profile_report() const -> ProfileReport const&;
    auto output() -> Output&;
    template<Writable... Values> void write(Values const&... values);
    template<DrawingCommand... Commands> void send(Commands const&... commands);
    void flush();
    template<Setting... Settings> void set(Settings const&... settings);
    auto area() const -> Area;
    auto read() -> ReadResult;
    auto read(std::chrono::milliseconds timeout) -> ReadResult;
    void close();
    auto is_open() const noexcept -> bool;
};
```

The session borrows both streams; they must stay open until cleanup completes.
The POSIX backend requires both to refer to the same terminal device and supports
distinct FILE* wrappers for it. Reject redirected or mismatched streams before
changing anything. Redirected/output-only programs use Output and profiles;
they do not need a dummy interactive session. Do not silently open /dev/tty.

Only one Escape session may own modes for a terminal device in this process.
Reject a second overlapping session with system_error(device_or_resource_busy),
including when different descriptors refer to the same device. A small process
registry for ownership is permitted; global rendering style, parser state, or
environment caches are not. There is no guarantee against unrelated processes
controlling the same terminal. Terminal is move-constructible; moving transfers
cleanup responsibility and leaves the source closed. Assignment is deleted to
avoid burying cleanup failure in replacement of an existing session.

All methods other than is_open, close, and destruction require an open session;
otherwise throw logic_error. Returned profile and output references last until
move, close, or destruction. profile is immutable during an open session; changing
the target capability assumptions requires a new session.

### Opening and changing settings

Open validates all options and profile before side effects, captures the operating-
system input settings, allocates required session/parser storage, reserves ownership,
then applies changes. Failure releases ownership and attempts to undo every change
already made, preserving the original exception. If rollback itself fails, the
thrown SessionError reports both failures; it derives from system_error and
exposes `rollback_error() -> std::error_code`. SessionError's primary code describes
the failing original operation. UnsupportedFeature/invalid_argument validation
failures occur before changes, so they do not require rollback.

On POSIX, derive desired termios from the captured settings and active explicit
options, rather than repeatedly editing already-edited flags:

- Echo::On sets ECHO; Echo::Off clears ECHO and ECHONL.
- InputBuffer::Canonical sets ICANON and retains the captured translation/flow
  settings unless another explicit option requires a change.
- Immediate clears ICANON, ICRNL, INLCR, IGNCR, ISTRIP, IXON, IXOFF, and IEXTEN,
  and sets VMIN=0,VTIME=0 because readiness/deadlines belong to Reader. Byte reads
  with no data are not mistaken for EOF in this mode.
- Signals::On sets ISIG; Signals::Off clears ISIG, IEXTEN, IXON, and IXOFF.
- Preserve other captured flags, including output processing, baud rate, and
  unrelated control characters. Apply with TCSANOW; never discard pending input
  through TCSAFLUSH or flush terminal input as a convenience.

Setting is the closed set Echo, InputBuffer, Signals, ScreenBuffer, CursorMode,
MouseMode, FocusReporting, and BracketedPaste. Zero settings are a no-op.
Duplicate setting types in one call are invalid; all values are validated first.
set recomputes the desired OS settings, applies them once, then protocol changes
in screen/cursor/mouse/focus/paste order, and flushes. Unchanged protocol settings
emit nothing, so setting Alternate twice does not clear it twice. Mode operations
always require capability support; BestEffort never silently loses required
interaction semantics. Explicit mouse Off is harmless when no mouse support
exists and its tracked state is already Off; equivalent disabling no-ops apply
to focus/paste and the normal/show baseline. An unsupported effective change throws.

After successful setting changes, update the owned mode state and reader protocol
configuration. On failure attempt rollback of the just-started transition. A
partial I/O failure can make actual protocol state uncertain; mark the session
unusable except for close/is_open and throw SessionError. close still attempts
restoration for all possibly changed modes. OS failures must be checked, not
ignored. An unusable session remains is_open()==true until cleanup is attempted;
other operations throw logic_error. set makes no promise of atomic terminal observation across OS and byte
operations.

send accepts drawing commands only and encodes using profile(), validates the
entire command pack before output, and uses bounded chunks with no intermediate
heap string. It does not deduplicate style or flush. Zero commands is a no-op.
write forwards the byte/text output contract, without capability filtering.
output exposes the underlying Output for integration; raw protocol bytes written
through it are the caller's responsibility and cannot update session knowledge.

### Dimensions and resize

area() uses a single checked OS size query on the output terminal. It returns
width/height together without mixing two different descriptor snapshots. Zero
dimensions are permitted when the OS cannot provide a useful size; OS errors
throw system_error. Width/height convenience wrappers can call area(), but code
needing both should query once.

Terminal::read adds resize events to Reader's input events. Capture the initial
size at open without emitting an event. If enabled, sample size at resize_interval
while waiting and before blocking when a sample is due. Return a Resize only when
the observed size differs from the last reported size. Pending input events take
precedence, so resize must not reorder input bytes. Transient sizes between polls
may be coalesced; there is no promise to deliver every physical resize. A read
timeout of zero performs no sleeping and may sample a due resize. No SIGWINCH
handler is installed. Applications needing immediate integration with an event
loop can add a later explicit wakeup adapter; polling policy stays configurable.

### Closing and restoration

close is idempotent. Attempt every applicable cleanup action, even if an earlier
one fails: flush queued drawing, restore changed protocol modes in reverse order,
flush restoration output, restore the exact captured OS settings, release owned
resources and session ownership. Leave borrowed streams open. Mark closed after
these attempts and throw the first cleanup system_error if there was a failure.
Destructor performs the same best-effort cleanup without throwing; callers who
need confirmation call close explicitly. Pending output failure does not prevent
restoration attempts or OS cleanup.

Restore protocol modes to restore_modes only when this session changed or may
have changed them. Do not blindly enable autowrap, reset styles, or show the
cursor for a passive session. Escape does not change autowrap in this proposal.
For an alternate-screen session, leave/restore the buffer state as configured;
normal-screen drawing cannot be undone. Style and screen contents changed by
drawing commands are not part of automatic restoration.

Captured OS settings are restored. On macOS, a checked FIONREAD query completes
the kernel's pending canonical-mode reprocessing without consuming or flushing
input, so the transient PENDIN flag does not remain set after restoration. Protocol-mode
restoration depends on the supplied initial state and terminal behavior. It is
not a blanket guarantee of restoring an unknown preexisting emulator state.
No library destructor can recover from SIGKILL, process crashes without stack
unwinding, disconnected terminals, or an unavailable output stream. This is a
limit of the lifecycle contract, not a reason to silently install global handlers.

Termination/suspend signal handlers and signal dispositions belong to the host
application in this version. Escape does not call exit, swallow exceptions,
replace SIGINT/SIGTERM handlers, close borrowed descriptors, or change process-
wide stdio buffering. Applications needing graceful signal cleanup route their
shutdown into ordinary control flow before destroying the session.
Use bounded reads if the host checks a shutdown flag between iterations; an
indefinite read does not treat an interrupted syscall as an implicit quit request.

### Windows session and reader backend

Windows sessions require readable console-input and writable console-screen-buffer
handles, including distinct FILE* wrappers. Windows attaches a process to one
console; its screen buffers share input settings and code pages. One session
owns that console in the process, even for different screen buffers. Reject
redirected or wrong-direction handles before changing modes. Native console
handles used by Windows Terminal/ConPTY are supported; pipe-only MSYS/mintty
connections use standalone Reader/Output and are not interactive sessions.

Capture input/output console modes and code pages before acquiring ownership.
After acquisition, flush prior output and capture/change the output CRT descriptor
mode with `_setmode(..., _O_BINARY)`. Enable processed and VT output, select UTF-8
output, and disable Quick Edit while preserving unrelated flags. VT output mode
must succeed; unsupported older console hosts fail open with checked rollback.
Restore modes, both code pages, and the original descriptor mode at close. No
control handler is installed, input discarded, or console allocated by the library.
The host must not change these shared settings during a session.

Echo maps to ENABLE_ECHO_INPUT, Canonical/Immediate to ENABLE_LINE_INPUT,
and Signals to ENABLE_PROCESSED_INPUT. Windows requires echo to be disabled for
immediate input; an effective Echo::On + Immediate combination is invalid. The
interactive preset already selects Echo::Off. Native console-record reading
avoids a blocking ReadConsole call after readiness for a non-character record.
Reader maintains bounded canonical editing storage (4096 UTF-8 bytes), handles
Backspace and Enter, and delivers a completed line through the same parser.
Overflow reports system_error(value_too_large). Session echo uses the borrowed
session output buffer; standalone Reader echo opens and closes the attached
console's active output buffer. It does not implement shell line editing/history.

KEY_EVENT records normalize Unicode (including UTF-16 surrogate pairs), repeats,
Alt text, AltGr text, arrows/navigation, and F1..F24 into the shared byte parser.
F21..F24 use a private transport grammar enabled only for native console Readers;
ordinary Parser instances and redirected byte input keep those keys reserved.
Key releases and unrelated native records do not become key presses or extend a
read deadline. Malformed native UTF-16 units become replacement scalars. Mouse
records normalize to the configured SGR, legacy, or urxvt grammar; focus records
are delivered only when configured. Session mouse tracking filters motion to
Basic/Drag/Move and toggles native mouse-input capture. Resize queries measure
the visible viewport, not the scrollback buffer. Resize sampling uses the shared
session scheduler. Native records cannot identify bracketed paste themselves;
paste boundaries require actual sequences from a connection that supports them.

The automatic session profile uses a conservative Windows VT baseline: control,
sixteen colors, bold/underline/inverse, alternate screen, cursor visibility.
Mouse/focus/paste default off and richer capabilities need an explicit profile.
The backend uses the current console color table when available. Evidence records
the Windows backend assumption; NO_COLOR disables colors only.
The pure TERM resolver's table is unchanged. A backend profile is not a live
emulator verification result.

Redirected disk and pipe input remains byte-oriented, with the same parser,
EOF handling, and monotonic deadlines. Synchronous pipes use PeekNamedPipe and
bounded 2 ms waits because they expose no reliable waitable readability event.
Readers must have exclusive access to their stream/handle. Standalone Output
retains borrowed stdio buffering and descriptor mode; Windows callers provide a
binary byte stream to prevent CRT newline translation. Session output sets and
restores binary mode explicitly.

## 12. Compatibility scope

Three compatibility levels must be reported separately:

1. **Encoder correctness:** output bytes match the chosen protocol contract.
2. **Parser correctness:** supplied byte fixtures produce the specified events.
3. **Terminal validation:** those commands and input modes work on identified
   emulator/version/platform/connection combinations.

Passing unit tests establishes the first two, not the third. Maintain a checked-in
matrix recording version, TERM/COLORTERM, multiplexer/SSH path, selected profile,
tested features, known limits, and fixture provenance. Matrix rows start as
untested; do not claim all terminals work because they accept some ANSI commands.

The validation set should include xterm, macOS Terminal, a VTE terminal, Konsole,
kitty, Alacritty, WezTerm, tmux, screen, an SSH connection, a Linux console,
Windows Terminal, and the Windows console host.
These are test targets, not support claims. Add older/limited terminals when
their behavior motivates a concrete profile or adapter. A TERM value names the
connection's advertised behavior; do not turn emulator-brand detection into an
unbounded stack of special cases in the encoder.

The baseline uses existing seven-bit CSI/SGR and explicitly selected optional
protocols. Do not assume true color, mouse support, paste reporting, focus
reporting, key release, uniform Unicode width, or accurate environment hints.
The architecture leaves room for adapters without forcing terminfo, probes, or
an emulator dependency on ordinary byte generation.

Deferred additions require their own reviewed contracts: active queries,
terminfo-backed arbitrary terminals, enhanced keyboard protocols, synchronized
output, hyperlinks/titles/clipboard, graphics, event-loop or
nonblocking output adapters, and style diff rendering. They must not be smuggled
into current functions through hidden environment behavior or magic flags.

## 13. Build and dependency interface

Retain the local CMake target `escape` and add an exported `Escape::escape` target.
The installed public headers require C++20. Public includes do not expose zzz
coroutines, ICU types, termios, or platform console headers. The implementation
requires no third-party library; the installed CMake target supplies required
platform linkage for static consumers.

Provide `ESCAPE_BUILD_TESTS` and `ESCAPE_BUILD_EXAMPLES`, enabled when building
Escape as the top-level project and disabled by default for add_subdirectory
consumers. Register behavior tests with CTest. Do not fetch/build unrelated
dependency tests as a side effect of linking Escape. Installation exports headers,
the library, version metadata, and a package configuration supporting
`find_package(Escape CONFIG REQUIRED)`.

An opt-in standalone benchmark suite measures encoding, reused buffers, parsing,
and I/O. Allocation contracts are checked separately by behavior tests.
Compiler warning/sanitizer checks remain part of development verification, not
flags forcibly imposed on downstream consumers.

## 14. Migration from the current checkout

Baseline reviewed: commit `4262aa2` on main. This proposal is a source-breaking
interface revision; do not accumulate a second global session system merely to
keep obsolete initialization calls working. Release/version policy is separate
from the interface contract and must acknowledge these breaks.

| Current interface/behavior | Proposed replacement |
| --- | --- |
| `escape(...) -> std::string` | Same convenient return, now pure |
| Variadic concatenation of temporary strings | Shared size/encode path; append and bounded output |
| `Cursor` alias for `Point` | Distinct Cursor command; cursor(point) helper |
| Global traits/color getters | Remove; callers own their desired style values |
| `set(...)` global side effects | `terminal.set(...)` with owned lifetime |
| initialize/uninitialize functions | Terminal::open and RAII/close |
| Implicit process exit/signal handlers/buffering changes | Checked errors and explicit host lifecycle |
| Global `read()` / integer timeout | Reader or Terminal; chrono timeout and EOF distinction |
| Unicode-valued Key enum with packed Mod bits | Key scalar-or-name variant plus Modifiers |
| Inferred previous button on ambiguous mouse release | optional released button; no invented identity |
| KeyMode::Raw Linux-console path | Deferred platform-specific adapter |
| GlyphString range concept | GlyphRange concept and owning GlyphString alias |
| Lvalue container pipes mutate | Value-returning pipes; named style_in_place |
| Broad integral/character conversions | Explicit byte/scalar/ASCII contracts |
| Detail Unicode helpers and public zzz leakage | Public text functions with strict UnicodeError |
| string writes use fputs; views use per-byte output | Consistent sized block output |
| TERM/color functions with static snapshots | Owning Environment and explicit ProfileReport |
| Two independent size queries | area() obtains one width/height snapshot |

Update Pong and termcaps to demonstrate the ordinary API, and one focused
example to demonstrate frame-buffer reuse. Existing operator tests must change
where old mutation behavior conflicts with this functional contract; passing
old tests is not the definition of correctness. Keep convenient helpers only
when they can be stateless wrappers with the same guarantees.

## 15. Behavior-test contracts

The following groups name intended outcomes. Tests are added as each interface
slice is implemented. A short-lived behavioral failure is acceptable; keep the
working slice compiling so a failure can identify a violated contract.
Expected command/event fixtures should be independent of the function invoked.
Avoid golden tests generated by the same code under test.

| Group | Required behavior |
| --- | --- |
| ENC-01 | Exact canonical bytes for every command/color/trait; no hidden state |
| ENC-02 | Zero/one/many commands; left-to-right composition; repeated commands retained |
| ENC-03 | Trait replacement preserves colors; brush replaces modeled style; full reset distinct |
| ENC-04 | Coordinate extremes avoid overflow; invalid casted values rejected before mutation |
| BUF-01 | Append preserves prefix and strong content guarantee, including a late invalid command |
| BUF-02 | Exact-fit/oversized/undersized/empty spans; no terminator; untouched trailing bytes |
| BUF-03 | Return values own storage; views have the declared lifetime; no scratch-buffer aliasing |
| CAP-01 | Every fallback/strict rule, quantization ties, indexed preservation, custom system palette |
| CAP-02 | Explicit profile validation; unknown/dumb/redirected defaults; no hidden probing |
| CAP-03 | Environment snapshot ownership, precedence, NO_COLOR, 256color not implying true color |
| TXT-01 | Every UTF boundary, nulls, noncharacters, invalid forms, first-error offset |
| TXT-02 | Append conversion leaves destination unchanged on invalid input |
| GLY-01 | Scalar preservation and styling; pure lvalue pipes; explicit mutation; order and self-append |
| COL-01 | RGB/HSL boundaries, negative computed hue normalization, validation, rounding tolerances |
| OUT-01 | String/view identical bytes including nulls; raw byte versus Unicode scalar distinction |
| OUT-02 | Ordered variadic writes; no automatic flush; partial transfer and delayed flush failures |
| PAR-01 | All supported keys/modifiers and mouse kinds; no invented physical-key/release identity |
| PAR-02 | Every split boundary and concatenated event stream; consumed counts and forward progress |
| PAR-03 | Caller timeout versus Escape/sequence expiry; EOF; Alt ambiguity; malformed recovery |
| PAR-04 | Protocol-disabled units, unknown replies, overlong chunks, bounded memory, no dropped bytes |
| PAR-05 | Paste payload preservation, empty/huge/truncated paste, split delimiters, chunk flags |
| IO-01 | Reader queues events, persists state across timeouts, drains hangups/EOF, retries EINTR |
| SES-01 | Passive/interactive defaults; checked mode application; no unrelated flag destruction |
| SES-02 | Single ownership, moves, duplicate/invalid settings, closed-object behavior |
| SES-03 | Partial-open/set rollback, failure reporting, unusable state, cleanup attempts despite errors |
| SES-04 | Exact captured OS restore; only owned protocol changes restored; no process/stdio takeover |
| SES-05 | Size query consistency and resize polling/coalescing without reordering input |
| LIVE-01 | Matrix of actual terminal semantics and restoration; fixtures record provenance |
| PKG-01 | Self-contained headers, add_subdirectory and installed static consumer builds |

Allocation guarantees are correctness checks rather than throughput benchmarks:
verify no allocation in successful bounded encoding, append with sufficient
capacity, single-scalar UTF-8 encoding, or normal key/mouse parsing after setup.
Measure only the operation under test; exclude fixture setup, returned owning
strings, terminal opening, and error-message allocation. Do not assert wall-clock
thresholds or freeze a particular internal data structure.

Use pseudoterminal integration tests for OS flags, fragmentation, deadlines,
EOF, and cleanup. Use a controllable internal syscall/stream seam for short
transfers and failures; do not make public callers implement a virtual backend
just so tests can inject faults. Real emulator checks remain distinct from PTY
checks: a PTY transports bytes but does not render them.

## 16. Implementation order

1. Review this contract through simple and efficient usage examples, concentrating
   on the decisions in section 17 before implementation.
2. Implement pure sequence values and the three encoding paths, with their tests.
3. Implement consistent output and public strict Unicode conversion.
4. Refactor glyph operations into value transformations and named mutation.
5. Introduce the event model and parser independent of live terminal I/O.
6. Implement Reader and scoped Terminal with PTY/failure tests.
7. Implement profiles/fallback resolution and connect session send to profiles.
   A raw encoder and explicitly supplied profile can be used during earlier work;
   no temporary global capability API is needed.
8. Migrate examples, complete package exports, and build the live-terminal matrix.

The capability contracts are designed now even though their implementation comes
later. This prevents return-type work from baking discovery, global state, or
fallthrough behavior into the encoder. Benchmarks follow once a working interface
exists and there is a concrete performance question.

## 17. Proposed decisions to review

These are concrete defaults in this proposal, not unresolved holes in its behavior.
Changing one means updating the contract and its tests before implementation.

- Keep std::string returns; expose append_escape and all-or-nothing escape_to
  instead of introducing an owning small-buffer string type immediately.
- Make Cursor distinct from Point, and keep byte commands separate from text.
- Preserve trait-replacement semantics but remove generation-time global state.
- Make range styling pure; put mutation behind a clearly named function.
- Use explicit profiles with a BestEffort style policy and strict interaction modes.
- Return semantic key/modifier values, expose unknown bytes, and distinguish EOF
  from a timed read with no event.
- Use scoped sessions without automatic signal-handler installation; distinguish
  captured OS state from assumed protocol state.
- Defer enhanced keyboard protocols, active probes, terminfo adapters, and layout
  without hiding their absence.

The primary tradeoffs are explicit: strong append/span guarantees require a size/
validation pass; pure lvalue styling can copy; conservative profiles may require
an application override; resize polling trades immediacy for avoiding signal
ownership. Named in-place/buffer APIs address the first two without changing the
ordinary interface. Test and measure these costs before adding new abstractions.

## Appendix A. Input grammar details

For ordinary CSI keys, accept no parameters or `1`, and optionally `1;m` with
modifier m in 1..16. Finals A/B/C/D map Up/Down/Right/Left, H/F Home/End, and
P/Q/R/S F1..F4. CSI Z accepts no parameters and means Shift+Tab. Tilde forms
accept exactly `n` or `n;m` with the identifiers in section 9. Reject additional
parameters, private prefixes, colon subparameters, negative numbers, and decimal
overflow as unknown/malformed input. Leading zeros are accepted. Omitted parameter
lists are permitted only for ordinary-key forms. Empty fields inside a parameter
list are not accepted; do not invent missing tilde identifiers or modifiers.

SS3 has no parameters in this baseline. A/B/C/D/H/F map the same navigation
keys; P/Q/R/S map F1..F4. M is KeypadEnter, p..y are Keypad0..9, n decimal,
k plus, m minus, j multiply, and o divide. Unsupported variants remain unknown.
CSI R with shape `1;m` is interpreted as modified F3 in the absence of an active
query mechanism. A cursor-position reply with identical bytes is indistinguishable;
there is no hidden query outstanding. Other CSI cursor reports are unknown units.

Mouse button fields use Shift=4,Alt=8,Ctrl=16, motion=32, wheel=64, extended=128.
After removing modifier/motion flags, base 0/1/2 identifies left/middle/right;
base 3 identifies an unspecified legacy release or motion with no held button.
Wheel bases 64..67 mean up/down/left/right; extended 128..131 mean buttons 8..11.
Button4..7 are reserved semantic names and are not synthesized from wheel events.
Other combinations are unknown. Pixel-coordinate mouse reporting is not accepted.

SGR is CSI `<b;x;yM` for presses/moves/wheel and `<b;x;ym` for release. x,y must
be positive and subtract to representable int coordinates. Motion+wheel is
invalid; a lowercase wheel/motion form is invalid. A plain uppercase base-3
SGR report is invalid; a lowercase base-3 release has an unknown button. Legacy
X10 framing is CSI M plus exactly three bytes, with 32 subtracted from each;
coordinates must be >=1 afterward. Urxvt framing is CSI `b;x;yM`, subtracting
32 from b only. Legacy/urxvt base-3 reports are unspecified releases. Byte-format
coordinate range limits are preserved, not guessed around.

Focus reports are CSI I/O without parameters. Paste starts at CSI 200~ and ends
at CSI 201~; the end marker is recognized incrementally only inside enabled paste.
Generic CSI units end at their final byte (0x40..0x7E), after valid parameter and
intermediate byte classes; malformed introducers/fields become UnknownInput.
Opaque string units accumulate/chunk until their terminator. On cancellation
bytes CAN/SUB, close the current unit as Malformed including that byte. ESC
followed by a new introducer inside an unfinished CSI closes the preceding unit
as Truncated and begins the new one; OSC/DCS payload ESC only ends the unit when
followed by the string terminator's backslash. These recovery rules must preserve
byte order and be independent of read fragmentation.

## Appendix B. Reference and evidence policy

Protocol references guide semantics; this document's choices of ownership, API
names, error policy, canonical serialization, and defaults are original design
decisions. Consult [xterm's maintained control reference](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html)
for the cited CSI/SGR/private-mode conventions and [Unicode's conformance chapter](https://www.unicode.org/versions/Unicode17.0.0/core-spec/chapter-3/)
for scalar/encoding validity. These were consulted on 2026-10-11. They do not
establish support on an arbitrary terminal or supersede a captured compatibility
fixture. Fixture updates must explain a protocol correction or a deliberate
contract change; never regenerate expectations merely to make a failing test pass.

## Appendix C. Initial environment-resolution data

The initial resolution table below makes the proposed resolver deterministic.
It is an advertised-protocol heuristic, not a statement that emulator behavior
has been verified. Versioned live fixtures can justify subsequent table changes.
Profiles always start with the conventional system palette and BestEffort policy.
Names not listed are unknown; do not match arbitrary prefixes or suffixes.

| Exact TERM values | Colors before COLORTERM/NO_COLOR | Trait set | Alternate / visibility | Mouse | Focus / paste |
| --- | --- | --- | --- | --- | --- |
| `ansi` | Basic8 | Basic | false / false | None / Off | false / false |
| `xterm`, `xterm-color` | Basic8 | Full | true / true | Sgr / Move | true / true |
| `xterm-256color` | Indexed256 | Full | true / true | Sgr / Move | true / true |
| `screen`, `tmux` | Basic8 | Basic | true / true | None / Off | false / false |
| `screen-256color`, `tmux-256color` | Indexed256 | Basic | true / true | None / Off | false / false |
| `linux` | Basic16 | Basic | false / true | None / Off | false / false |

All table entries have control=true. Basic means Bold, Dim, Underline, Blink,
Inverse, and Invisible. Full adds Italic, CrossedOut, and DoubleUnderline;
Standout is already represented by Bold+Inverse. Table limitations for mux/console
input are deliberate conservative defaults; caller-supplied profiles can enable
verified protocols. Do not infer a terminal behind the multiplexer.

Resolution order is: plain default; if output is a terminal, apply an exact known
TERM entry; for a recognized entry apply COLORTERM=truecolor/24bit; finally apply
nonempty NO_COLOR. Redirected output never applies the table or COLORTERM.
Report one final CapabilityDecision per Feature in the order listed in section 3,
with the strongest applied evidence source and an explanation. Default/unknown
results explain the plain fallback; NO_COLOR explains color suppression. Diagnostic
wording is not a stable string API. Explicit terminal profiles bypass this
resolver and report Explicit for each feature.

Windows backend references: Microsoft's [console modes](https://learn.microsoft.com/en-us/windows/console/setconsolemode),
[low-level input](https://learn.microsoft.com/en-us/windows/console/low-level-console-input-functions),
and [VT sequences](https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences).
These define API behavior; they do not replace the live acceptance matrix.
