# API reference

Include `<esc/esc.hpp>` for the whole API, or one of the headers below. Public
names are in `esc`. This guide groups the functions by use; generated HTML gives
the declarations, overloads, members, and header comments.

- [Output and composition](#output-and-composition)
- [Styles and colors](#styles-and-colors)
- [Text and glyphs](#text-and-glyphs)
- [Terminal sessions](#terminal-sessions)
- [Input and parsing](#input-and-parsing)
- [Queries and keyboard protocols](#queries-and-keyboard-protocols)
- [Command values](#command-values)
- [Generated reference](#generated-reference)

## Output and composition

Headers: [sequence.hpp](../include/esc/sequence.hpp),
[styled_text.hpp](../include/esc/styled_text.hpp), [io.hpp](../include/esc/io.hpp).

| Function | Use |
| --- | --- |
| `encode(values...)` | Return an owning string of encoded bytes; no I/O |
| `encode(profile, values...)` | Encode after applying the profile's capability policy |
| `append_encoded(string, [profile,] values...)` | Append encoded bytes to reusable storage |
| `encode_to(span<char>, [profile,] values...)` | Encode into caller storage; return `EncodeResult{written, required}` |
| `sequence(values...)` | Own and combine values for later encoding |
| `styled(content, appearance)` | Own text or composed content with a scoped appearance |
| `styled_view(string_view, appearance)` | Borrow UTF-8 or UTF-32 text with a scoped appearance |
| `text \| appearance` | Create owning styled text |
| `styled_text \| appearance` | Return styled text with the additional appearance applied |
| `Output(FILE* = stdout)` | Borrow a byte output stream |
| `Output::write(values...)` | Write raw commands and text; no capability adaptation |
| `Output::flush()` | Flush the borrowed stdio stream |
| `write(values...)` / `flush()` | The stdout shortcuts |

The output functions accept strings/views, UTF-32 strings/views, Unicode scalars,
commands, styles, styled runs, glyph ranges, and nested sequences. Format numbers
with `std::to_string` or your preferred formatter first. They add no spaces,
newlines, terminators, or implicit flushes.

A short span writes nothing and reports the full `required` size. Successful
encoding writes exactly `written` bytes and leaves the rest of the span alone.
Validation precedes storage changes or output. See [borrowing and validation](behavior.md#borrowing-and-validation).

## Styles and colors

Headers: [style.hpp](../include/esc/style.hpp), [trait.hpp](../include/esc/trait.hpp),
[color.hpp](../include/esc/color.hpp).

| Name | Use |
| --- | --- |
| `fg(color)` / `bg(color)` | Select the foreground or background channel |
| `Style{}` | A complete appearance; default colors and no traits |
| `StylePatch{}` | A partial change; an empty patch preserves the appearance |
| `appearance \| appearance` | Compose without changing the operands; the last decision wins |
| `without(traits)` | A patch that disables traits |
| `Traits(Trait)` | Construct a checked trait set; default construction is empty |
| `Traits::contains(trait)` | Test all requested bits; `None` tests whether the set is empty |
| `Traits::insert(trait_or_set)` / `remove(trait_or_set)` | Mutate and return the set |
| `Traits::data()` | Read the underlying checked bit mask |
| `RGB(r, g, b)` | Byte channels, each 0–255 |
| `RGB(hex)` | A checked packed 24-bit value |
| `RGB(HSL)` / `hsl_to_rgb(HSL)` | Convert hue 0–359 and saturation/lightness 0–100 to RGB |
| `rgb_to_hsl(RGB)` | Convert RGB to the integer HSL representation |
| `conventional_system_palette()` | Return sixteen RGB values for color approximation |

`Color` holds `DefaultColor`, `IndexedColor`, or `RGB`. Use `default_color` for the
terminal's configured color, `colors::red` for its named palette slot, or
`IndexedColor{123}` for another palette slot. `colors` includes the sixteen
ordinary and bright names. Palette slots have no fixed RGB value.

Trait constants are `bold`, `dim`, `italic`, `underline`, `blink`, `inverse`,
`invisible`, `crossed_out`, `double_underline`, and `standout`. `Trait` enum names
are also available. `standout` combines bold and inverse. `ResetStyle{}` resets
all SGR state; a `Style` replaces only the modeled appearance.

### Profiles

Header: [capabilities.hpp](../include/esc/capabilities.hpp).

| Function or type | Use |
| --- | --- |
| `snapshot_environment()` | Read TERM, COLORTERM, and NO_COLOR into an `Environment` |
| `resolve_profile(environment, output_is_terminal)` | Return a `ProfileReport` without I/O |
| `TerminalProfile` | Caller-owned color, trait, protocol, and extension assumptions |
| `ProfileReport::decisions` | Per-feature explanations and evidence sources |
| `UnsupportedFeature::feature()` | Identify a capability rejected by the profile |

A default profile is plain. `BestEffort` adapts colors/traits and omits optional
extensions where supported by policy; `Strict` rejects any loss. Geometry and
mode changes require their capabilities under either policy. An explicit profile
bypasses environment resolution. See [colors and profiles](behavior.md#colors-and-profiles).

## Text and glyphs

Headers: [text.hpp](../include/esc/text.hpp), [layout.hpp](../include/esc/layout.hpp),
[glyph.hpp](../include/esc/glyph.hpp).

| Function | Use |
| --- | --- |
| `utf8_to_utf32(utf8)` / `utf32_to_utf8(utf32)` | Strict, owning conversion without normalization |
| `append_utf32(string, utf8)` / `append_utf8(string, utf32)` | Append a validated conversion |
| `encode_utf8(scalar)` | Return `Utf8CodePoint`, holding up to four bytes |
| `Utf8CodePoint::view()` | Borrow the encoded bytes |
| `UnicodeError::offset()` | Read the failing byte/scalar offset |
| `graphemes(utf8_or_utf32)` | Borrow a forward range of extended grapheme clusters |
| `GraphemeView::begin()` / `end()` | Iterate borrowed clusters until the sentinel |
| `next_grapheme(text_or_glyph_span)` | Borrow the first cluster, or an empty view |
| `cell_width(text_or_glyph_span, policy = {})` | Estimate columns; `nullopt` means unknown width |
| `grapheme_width(cluster, policy = {})` | Measure exactly one cluster; multiple clusters throw |
| `glyphs(text, style = {})` | Build an owning `GlyphString` with per-scalar styles |
| `append_glyphs(glyph_string, text, style = {})` | Append styled scalars |
| `append_glyphs_to(iterator, text, style = {})` | Write styled scalars to an output iterator; return the advanced iterator |
| `style_in_place(glyph_range, appearance)` | Mutate each glyph's style |
| `styled(glyph_range, appearance)` / `range \| appearance` | Return an owning styled glyph string |
| `glyph \| appearance` / `character \| appearance` | Return a styled glyph |
| `glyph_string += range` | Append a validated glyph range |
| `left + right` (glyph operands) | Return a concatenated `GlyphString` |

Glyph operands include glyphs, scalar characters, UTF-8/UTF-32 text, and glyph
ranges. A `Glyph` stores one Unicode scalar and a complete `Style`; it doesn't
promise one cell. `WidthPolicy` controls ambiguous, emoji, joined-cluster,
orphan-mark, and unassigned widths, with optional sorted scalar overrides.
Unicode data is pinned to `layout_unicode_version` (17.0.0). See [text layout](text-layout.md).

## Terminal sessions

Header: [terminal.hpp](../include/esc/terminal.hpp).

| Function | Use |
| --- | --- |
| `TerminalOptions::interactive()` | Request immediate input, echo off, alternate screen, and a hidden cursor |
| `Terminal::open(options = {})` | Open a scoped stdin/stdout session; defaults preserve modes |
| `Terminal::open(streams, options = {})` | Borrow streams belonging to the same terminal device |
| `Terminal::profile()` / `profile_report()` | Read the selected profile and its explanations |
| `Terminal::write(values...)` | Draw through that profile; session mode commands are excluded |
| `Terminal::output()` | Access raw output; drawing bytes bypass profile adaptation |
| `Terminal::flush()` | Flush session output |
| `Terminal::set(settings...)` | Change owned echo, input, signals, screen, cursor visibility, mouse, focus, or paste settings |
| `Terminal::area()` | Read the current width and height in cells |
| `Terminal::read()` | Wait for an event or input exhaustion |
| `Terminal::read(milliseconds)` | Read with a steady-clock deadline; zero polls |
| `Terminal::wait_state()` | Get readiness, a borrowed wait source, and the next deadline |
| `Terminal::frame()` | Begin a nested frame; only the outermost frame flushes/ends synchronization |
| `Frame::close()` | End the frame, reporting errors; destruction also attempts cleanup |
| `Frame::synchronized()` | Check whether this active frame uses synchronized output |
| `Terminal::confirm_synchronized_output(timeout = 250ms)` | Query mode 2026 and use synchronization if confirmed |
| `Terminal::close()` | Restore owned state; safe to call again |
| `Terminal::is_open()` | Test whether the session still owns its state |
| `SessionError::rollback_error()` | Read a secondary error encountered restoring a failed transition |

`Terminal` and `Frame` are move-constructible, not copyable. Destructors attempt
cleanup without throwing. `Streams` borrows FILE pointers; Escape doesn't close
them. `ProtocolState` describes the initial mode state to restore. Sessions do
not install signal handlers. See [session ownership](behavior.md#session-ownership).

## Input and parsing

Headers: [io.hpp](../include/esc/io.hpp), [parser.hpp](../include/esc/parser.hpp),
[event.hpp](../include/esc/event.hpp), [key.hpp](../include/esc/key.hpp),
[result.hpp](../include/esc/result.hpp).

| Function | Use |
| --- | --- |
| `Reader(FILE* = stdin, options = {}, protocols = {})` | Borrow byte input and maintain parsing/deadlines |
| `Reader::read()` / `read(milliseconds)` | Read `Event`, `Timeout`, or `EndOfInput` |
| `Reader::configure(protocols)` | Select protocols for new sequence beginnings |
| `Reader::wait_state()` | Expose readiness and a borrowed host wait source |
| `Parser(options = {}, protocols = {})` | Construct bounded incremental parsing state; no clock or I/O |
| `Parser::next(span<const char>)` | Return bytes consumed and at most one event; advance by `consumed` |
| `Parser::pending_timeout()` | Identify an incomplete Escape or sequence timeout |
| `Parser::expire()` | Resolve the current incomplete unit after its deadline |
| `Parser::finish()` | Signal EOF and drain incomplete input; call until empty |
| `Parser::configure(protocols)` | Change protocols for new units; preserve a unit already in progress |
| `Parser::reset()` | Clear input state while keeping the protocol configuration |
| `match(variant, handlers...)` | Exhaustive visitation with normal `std::visit` reference/return semantics |
| `text_scalar(key)` | Return the scalar identity, or `nullopt` for a named/Kitty key |
| `KeyEvent::matches(key, modifiers = {}, action = Press)` | Match exact key identity, modifier bits, and action |

`Event` holds `KeyEvent`, mouse press/release/move/wheel, resize, focus, paste,
terminal reply, or unknown input. Mouse coordinates are zero-based cells;
wheel `dy=-1` is up and `+1` is down. A `MouseMove` reports its held button if any.
`PasteChunk` owns original bytes; flags identify first/last/aborted chunks.
`UnknownInput` owns bounded bytes and an `InputProblem` reason.

`Key` holds a Unicode scalar, `KeyCode`, or a `KittyKey` identifier. A text-only
key event can lack key identity. `KeyAction` distinguishes press, repeat, and
release; legacy input reports presses. `WaitState` combines readiness, an
optional deadline, and a descriptor/Windows handle owned by Escape.

## Queries and keyboard protocols

Headers: [query.hpp](../include/esc/query.hpp), [keyboard.hpp](../include/esc/keyboard.hpp).

| Function | Use |
| --- | --- |
| `encode_query(query)` / `query_to(span, query)` | Pure request encoding, owning or bounded |
| `Terminal::request(query_or_span)` | Send and flush; receive replies later through `read()` |
| `Terminal::query(query, timeout)` | Wait for one matching reply while preserving unrelated input |
| `Terminal::probe(timeout = 250ms)` | Collect keyboard and private-mode observations under one deadline |
| `Terminal::enable_keyboard(flags = Disambiguate, timeout = 250ms)` | Negotiate and confirm Kitty keyboard flags |
| `Terminal::disable_keyboard()` | Restore the captured keyboard flags |
| `Terminal::keyboard_protocol()` | Read the active input grammar |

`Query` and `Reply` cover device attributes, private modes, and Kitty flags.
`QueryResult` holds `Reply`, `Timeout`, `EndOfInput`, `InputPending`, or
`Rejected{reply}`. A timeout leaves support unknown. `InputPending` asks you to
drain preserved events before retrying. A `ProbeReport` does not change the
session profile or enable protocols. See [queries and keyboard input](queries-and-keyboard.md).

## Command values

Commands are values passed to an encoder or writer, rather than functions that
perform I/O. Coordinates are zero-based. Negative absolute coordinates or invalid
enumerators throw; encoding doesn't clamp to the current screen.

| Command | Effect |
| --- | --- |
| `Cursor{x = 0, y = 0}` / `cursor(Point)` | Set the absolute cell position |
| `MoveCursor{dx = 0, dy = 0}` | Move by signed horizontal/vertical offsets |
| `BlankRow{Erase::All}` / `BlankScreen{Erase::All}` | Erase the selected row/screen portion |
| `ResetStyle{}` | Reset all SGR appearance |
| `Bell{}` | Request the terminal's configured bell |
| `PlayNotes{notes, duration = 125ms, volume = 4}` | Request DECPS playback; duration rounds to 1/32-second ticks |
| `CursorStyle` | Default, blinking/steady block, underline, or bar |
| `SaveCursor{}` / `RestoreCursor{}` | Use the terminal's single DEC saved-state slot |
| `ScrollRegion{top, bottom}` / `ResetScrollRegion{}` | Set inclusive margins (`top < bottom`), or reset them |
| `ScrollUp{count = 1}` / `ScrollDown{count = 1}` | Scroll within the terminal's active margins |
| `InsertCharacters{count = 1}` / `DeleteCharacters{count = 1}` | Insert/delete cells at the cursor |
| `InsertLines{count = 1}` / `DeleteLines{count = 1}` | Insert/delete lines at the cursor |
| `WindowTitle{text}` | Set a validated UTF-8 title |
| `BeginHyperlink{uri, id = {}}` / `EndHyperlink{}` | Open/close a validated OSC 8 link |

`Erase` offers `ToEnd`, `ToBeginning`, and `All`. Zero editing/scrolling counts
emit nothing. `Note` includes `Rest` and C5–C7, with sharp names such as `Cs5`.
`NoteDuration{4}` is exactly 125ms. See [extensions](terminal-extensions.md)
for validation, capability requirements, and playback timing.

`ScreenBuffer`, `CursorMode`, `MouseReporting`, `FocusReporting`,
`BracketedPaste`, and `SynchronizedOutput` also encode as raw commands.
Use `Terminal::set` for session-owned modes and `Terminal::frame` for scoped
synchronization; `Terminal::write` rejects those raw mode commands.

`Point{x,y}` supports component-wise `+`/`-` and lexicographic ordering by x, then
y. `Area{width,height}` holds dimensions. Both support value comparisons.

## Generated reference

[Doxygen](https://www.doxygen.nl/manual/starting.html) builds searchable HTML
from the public headers. Install version 1.12 or newer separately; ordinary builds don't need it.

```sh
cmake -S . -B build -DESCAPE_BUILD_DOCS=ON
cmake --build build --target escape.docs
```

Open `build/docs/html/index.html`. The target needs no Graphviz, doesn't fetch
packages, and excludes implementation namespaces. Function descriptions live
with their declarations, so the signatures come from the code you actually build.
