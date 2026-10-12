# Functional API revision

This breaking revision makes output composable, keeps transformations pure, and
uses types to distinguish complete appearances, partial changes, and input
outcomes. The public headers are the signature reference; the session, capability,
Unicode, and protocol contracts in the linked documents continue to apply.

## One output vocabulary

```cpp
using namespace esc;

auto terminal = Terminal::open(TerminalOptions::interactive());
auto warning = fg(XColor::Yellow) | bold;
auto label = styled("Warning 👽", warning);

terminal.write(Cursor{4, 2}, label, "\n");
terminal.flush();
terminal.close();
```

`encode`, `append_encoded`, `encode_to`, `Output::write`, and the free stdout
`write` accept the same values: commands, styles, text, Unicode scalars, styled
runs, glyphs, and recursively composed sequences. `Terminal::write` accepts that
vocabulary with session mode commands excluded, even inside a composition. Use
`terminal.set(...)` for owned/restored screen, cursor visibility, mouse, focus,
and paste modes. Scoped synchronization stays under `terminal.frame()`.

```cpp
auto bytes = encode(Cursor{4, 2}, "Hello", ResetStyle{});
std::string buffer;
buffer.reserve(4096);
append_encoded(buffer, terminal.profile(), Cursor{4, 2}, label);
std::array<char, 256> storage;
auto result = encode_to(storage, terminal.profile(), Cursor{4, 2}, label);
```

A profile precedes the values in each pure encoder. Terminal writes use the
session profile; direct Output writes and unprofiled encoding are raw. There is
no implicit environment read, probing, newline, or flush. Raw byte views preserve
invalid UTF-8 and NUL; UTF-32 values and styled text validate Unicode strictly.
Arrays/pointers have conventional null-terminated semantics; explicit views keep
embedded NUL. Arbitrary numbers and booleans are rejected.

Every mixed pack is validated before any write or buffer modification. Bounded
encoding is all-or-nothing, reports the exact size, and leaves trailing bytes
untouched. Reserved appends and bounded encoding allocate nothing except when
an append's borrowed payload aliases the destination; that case builds a separate
suffix before changing the original. Alias checks include nested compositions.
Output streams fragments using a fixed stack buffer, retaining no pending private
bytes between calls. I/O errors can leave a written prefix; no rollback is promised.
User-defined forward ranges must remain stable throughout the two encoding passes.
General glyph ranges use a temporary suffix during append because their iteration
may depend on the destination. Bounded encoders require opaque/lazy ranges to be
independent of the destination; inspectable contiguous overlaps are rejected.

## Complete styles and partial changes

```cpp
auto warning = fg(XColor::Yellow) | bold;
auto urgent = warning | underline;
auto normal_weight = urgent | without(bold);
auto complete = Style{} | urgent;
```

`Style` contains foreground, background, and a complete `Traits` set. Applying it
replaces all modeled appearance components. `StylePatch` contains optional colors
and enabled/disabled trait sets; absent components preserve existing values.
Pipe composition is pure. The last color or enable/disable decision wins.
Conflicting enabled/disabled fields in a directly constructed patch are invalid.

`fg`, `bg`, Trait, Traits, and `without` are patch attributes. Trait/Traits encoding
adds traits; an empty set/None is a no-op. Full Style encoding clears the modeled
traits and sets its requested traits. `Style{}` resets modeled appearance;
`ResetStyle{}` emits the full SGR reset. Lowercase trait constants are convenient
values, and the corresponding `Trait::Bold` etc. remain available for switches
and explicit set construction. `Traits::insert/remove` and `style_in_place` name
mutation; all pipes return values.

Double underline takes precedence over single underline when both are requested.
Bold/dim and single/double underline share SGR reset codes. `without` clears the
shared wire group and restores peers known within the current encoding operation.
It cannot preserve an unknown peer set by an earlier call or another writer.

`RGB` is the single RGB type, with byte-channel and checked packed-hex constructors.
`RGB{HSL{...}}`, `hsl_to_rgb`, and `rgb_to_hsl` perform validated conversion.
`Color` contains RGB, XColor palette slots, or TermColor::Default. The redundant
TrueColor/TColor, foreground/background, and encoded-color helper spellings are
removed, along with the old Brush and escape-family APIs.

## Styled text, ownership, and nesting

```cpp
auto inner = styled("important", fg(XColor::Red) | bold);
auto message = styled(sequence("An ", inner, " message"), underline);
terminal.write(message);

auto borrowed = styled_view(std::string_view{existing_text}, warning);
```

`styled(text, appearance)` owns its text. UTF-8 stays UTF-8 and does not expand
into a vector of styled scalars. `text | appearance` also creates owning styled
text; later pipes compose the appearance without modifying the input. Rvalues
reuse owned text storage. `sequence(...)` copies/moves children and owns ordinary
string views by copying their text. Typed commands with view fields and explicitly
borrowed children retain their borrowing contract.

`styled_view` explicitly borrows a UTF-8 or UTF-32 view. Its source must outlive
all uses and stay unchanged during encoding; direct owning-string temporaries
are rejected. A string_view explicitly made from a temporary still requires the
caller to satisfy the lifetime contract. Styled bytes are revalidated on encoding,
so mutation of a borrowed source cannot bypass validation.

A scoped run starts from the style known immediately before it in the same
encode/append/write operation. Its patch inherits that appearance; a complete
Style replaces it. Inner runs restore the enclosing style, and the outer run
restores the style preceding it. Each operation starts with a default style
baseline. An isolated styled label finishes at that baseline. This never queries
or guesses a terminal's unknown preexisting SGR state.

`glyphs` explicitly builds a `GlyphString` when per-scalar styling is needed.
`Glyph{symbol, style}` and forward glyph ranges are directly encodable/writable.
Each glyph's complete style is applied, adjacent equal styles are coalesced, and
the preceding appearance is restored afterward. Empty glyph ranges emit nothing.
Glyphs remain Unicode scalars, with no implied one-cell width. Grapheme and cell
measurement APIs retain their explicit width policy.

## Input alternatives and one keyboard event

```cpp
using ReadResult = std::variant<Event, Timeout, EndOfInput>;
using QueryResult = std::variant<Reply, Timeout, EndOfInput, InputPending, Rejected>;
struct ParseResult { std::size_t consumed; std::optional<Event> event; };
```

There is no independent status field that can contradict its payload. Parser
`event` presence indicates whether one event was produced; `consumed` always tells
the caller how far to advance input. Timeout, input exhaustion, input preservation
queue pressure, and negotiation rejection remain distinct outcomes. `Rejected{Reply reply}` preserves the negative
acknowledgment for diagnostics. I/O errors
continue to throw.

```cpp
using namespace std::chrono_literals;
match(terminal.read(100ms),
    [&](Event const& event) { handle(event); },
    [&](Timeout) { tick(); },
    [&](EndOfInput) { running = false; });

if (auto reply = std::get_if<Reply>(&query_result)) {
    // Visit *reply for the typed response.
}
```

`match` is exhaustive visitation with std::visit's reference and return semantics.
It supports mutable and const lvalues and rvalues; handlers must cover all
alternatives and agree on return type.

Both legacy and enhanced keyboard input produce `KeyEvent`: optional key,
modifiers, action, optional alternate identities, and owned associated text.
Legacy action is Press; unreported fields stay empty. Text-only IME events can
lack key identity. `event.matches(U'q')` checks an exact key, empty modifiers,
and Press. Supply modifiers/action explicitly when matching a chord, Repeat, or
Release. Enabling Kitty adds information without changing the Event alternative.

## Migration and verification

Use encode/append_encoded/encode_to instead of escape/append_escape/escape_to,
Terminal::write instead of send, Style instead of Brush, RGB instead of the extra
color wrappers, and without instead of remove_trait. Replace Trait::None used as
a reset with a complete Style or ResetStyle. Visit ReadResult/QueryResult directly;
remove ParseStatus checks and test parser event presence. Replace KeyPress visitors
with KeyEvent visitors and handle optional identity and action explicitly.

The contract suite covers canonical bytes, nested restoration, pure composition,
Unicode validation, overlap, allocation, direct streamed text/glyphs, profile
adaptation, result visitation, legacy/enhanced input, and unchanged lifecycle/error
handling. Package checks compile each header independently, reject invalid output
values and nested session mode bypasses, and build both subproject and installed
consumers. Example and benchmark smoke checks exercise the public interfaces.

Local verification on 2026-10-12 used macOS and AppleClang 15 in Debug mode:
all 15 CTest suites passed, including standalone headers, installed/subproject
consumers, example PTY smoke tests, and benchmark preflights. A second build with
AddressSanitizer and UndefinedBehaviorSanitizer passed all 11 core/POSIX behavior
suites without findings. Leak detection was disabled on macOS. The Windows tests
were migrated but Windows runtime and live emulator validation were not run here.
