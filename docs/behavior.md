# Terminal behavior

Escape encodes values, reads events, and owns the terminal modes a session
changes. It doesn't keep a screen model or infer what your terminal has drawn.

## Colors and profiles

There are three color choices:

- `default_color` uses the terminal's configured foreground or background.
- `colors::red` and `IndexedColor{n}` select palette slots and respect the user's theme.
- `RGB{0xe06c75}` asks for a particular color.

`fg` and `bg` select the channel. `Style` describes a complete appearance;
`StylePatch` describes changes. An absent patch color preserves the channel,
while `fg(default_color)` resets it. Applying a complete style replaces both
colors and the modeled traits. Composing patches with `|` keeps the last decision
for each channel/trait, without changing the inputs.

Raw `encode`, `Output::write`, and free `write` emit what you ask for. Pass a
`TerminalProfile` to an encoder, or use `Terminal::write`, to apply capability
policy. `BestEffort` approximates colors, drops unsupported traits, and omits
optional extensions. `Strict` throws `UnsupportedFeature` if that would lose
information. Cursor geometry and mode changes require support under either policy.
The profile's system palette guides color approximation; it doesn't change the
terminal's actual palette.

A default profile produces plain output. Environment resolution uses TERM and
COLORTERM only when output is a terminal, then applies nonempty NO_COLOR to
suppress colors. It recognizes common xterm, Ghostty, kitty, WezTerm, screen,
tmux, ANSI, and Linux console TERM values. Other nonempty `xterm-*` names get a
conservative baseline. Empty/dumb TERM, redirected output, and other unknown
names remain plain. These are assumptions, not probes. `profile_report()` shows
where each decision came from.

An explicit profile bypasses environment resolution. Use it for terminal features
you have verified, including optional links, titles, scrolling, cursor styles,
and note playback. Sessions don't issue queries or negotiate keyboard protocols
on open. See [queries](queries-and-keyboard.md) for explicit confirmation.

Bold/dim and single/double underline share wire reset codes. `without` restores
peers known in the current encoding operation; it can't preserve a peer set by
another writer or a previous operation. Double underline takes precedence over
single underline when both are requested.

## Borrowing and validation

`styled` and `sequence` own ordinary string content. `styled_view` borrows text;
its source must stay alive and unchanged during encoding. Commands such as
`WindowTitle` and `BeginHyperlink` also borrow their view fields, including when
copied into a sequence. A grapheme range borrows its original text. Moving these
values doesn't extend the lifetime of borrowed storage.

UTF-8/UTF-32 conversions, styled text, glyphs, titles, and hyperlinks validate
Unicode. `UnicodeError::offset()` counts bytes for UTF-8 and scalars for UTF-32.
Plain byte output intentionally preserves arbitrary bytes, including NUL and
invalid UTF-8. Use explicit views to retain embedded NUL; pointer/array strings
have null-terminated semantics.

Every mixed output pack is validated before changing a destination or writing.
`encode_to` is all-or-nothing, adds no terminator, and reports the exact space
needed. It rejects inspectable overlap with borrowed payloads. An append that
borrows from its destination snapshots the suffix before growing the string.
Forward ranges must remain stable during the measurement and writing passes;
opaque/lazy ranges used with bounded encoding must be independent of the destination.

I/O failures can leave a written prefix. Destruction attempts cleanup, but cannot
report errors. Use explicit `flush()`, `Frame::close()`, and `Terminal::close()`
when the application needs to report failures. Negative timeouts, invalid
parameters, and invalid profiles throw before I/O.

## Session ownership

A session borrows input/output FILE streams belonging to the same terminal
device. One session owns that device's modes in the process. Default options
preserve modes; `interactive()` requests immediate input, echo off, alternate
screen, and a hidden cursor. Mouse, focus, and paste are opt-ins.

Use `set()` to change session-owned settings. Each settings pack rejects duplicate
kinds before I/O. Closing restores captured OS input settings and protocol modes
to `restore_modes`, which defaults to normal screen, visible cursor, and reporting
off. Supply the actual inherited protocol state when opening inside another
terminal application. The session doesn't discover it automatically.

Drawing effects such as cursor shape, margins, saved cursor slots, titles, and
open hyperlinks are the application's responsibility. Session cleanup doesn't
undo them. Use matching drawing commands when you change them.

Frames flush on their outermost close. They synchronize when the supplied profile
or an explicit confirmation enables that feature; otherwise BestEffort still
flushes without synchronization. Strict mode rejects an unconfirmed/unsupported
synchronized frame. Nested frames share the outer frame's synchronization.

Failed transitions try to roll back. `SessionError` reports the primary failure
and a secondary `rollback_error()`. Partial protocol writes can make a session
unusable; close still attempts restoration.

Escape doesn't install signal handlers. Applications decide how to handle signals
and suspend/resume. `Signals::Off` requests input characters instead of terminal
generated signals; the application must then handle its own quit keys.

## Input and deadlines

`Parser` has no I/O or clock. It owns a bounded incomplete unit, and `next` returns
how many bytes were consumed plus at most one event. Advance by `consumed`, even
if no event was produced. Protocol changes apply to new units, leaving an already
started sequence under its original grammar.

`Reader` handles I/O and steady-clock deadlines. Zero timeout polls; negative
values throw. Deadlines don't restart on unrelated input or interrupted reads.
Escape-key ambiguity and incomplete sequences have separate parser timeouts.
EOF drains pending bytes before `EndOfInput`. Paste chunks preserve original
bytes, and unknown input remains owned and bounded.

Don't mix Reader with unread stdio buffering, concurrent `fread`/`getchar`, or
another reader of the same stream. Sessions are single-reader objects; serialize
read, query, and keyboard negotiation. Writes also require application-level
serialization when sharing an output stream.

`wait_state()` exposes a borrowed descriptor or Windows handle plus readiness and
a parser/resize deadline. Never close that source or read it directly. A ready
state can mean an event or EOF is already held. See the [event-loop example](demos.md#host-event-loop).

## Windows streams

Native console sessions enable VT output, select UTF-8 output, and use binary
mode for the borrowed output descriptor. Closing restores captured modes, code
pages, and descriptor mode. Ordinary input uses Unicode console records. Explicit
queries prepare a VT input transport; opening a session alone doesn't create it.

The automatic Windows profile is conservative: sixteen colors, basic traits,
alternate screen, and cursor visibility. Richer input and output features need an
explicit profile for the connection. Native console handles are required for
sessions; MSYS/mintty pipe handles use `Reader`/`Output` instead.

Standalone `Output` preserves the borrowed FILE stream's mode. Open Windows byte
streams with `"wb"` or use host-owned `_setmode(..., _O_BINARY)` to avoid CRT newline
translation. See [query transport](queries-and-keyboard.md#windows-transport) for
VT input setup and restoration.
