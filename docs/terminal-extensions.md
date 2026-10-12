# Terminal controls, OSC payloads, frames, and event loops

These additions extend [the interface contract](interface-proposal.md). Public
APIs remain C++20 and do not expose platform headers. Encoding is pure; session
operations explicitly own I/O and state. There is no terminfo dependency.

## Capability selection

`TerminalProfile` adds `cursor_style`, `cursor_save_restore`, `scrolling`,
`editing`, `window_title`, `hyperlinks`, and `synchronized_output`. All default to
false. Environment resolution and the automatic Windows profile leave these
extensions disabled and report that support needs explicit selection. Existing
features retain their earlier evidence. The sound commands add independent `bell`
and `play_notes` flags, both false by default; known terminal profiles enable
basic bells, while DECPS playback requires explicit selection. A report contains
seventeen decisions.

Raw `encode`, `append_encoded`, and `encode_to` encode without checking a profile.
Their profile overloads and `Terminal::write` use the selected capabilities.
BestEffort omits unsupported extension commands; Strict throws
`UnsupportedFeature` with the corresponding `Feature`. Zero-count operations are
no-ops under either policy, including Strict. Invalid values and payloads throw
before fallback, so unsupported commands cannot hide invalid input.

Choose capabilities for the actual connection, for example:

```cpp
esc::TerminalOptions options = esc::TerminalOptions::interactive();
auto profile = esc::resolve_profile(esc::snapshot_environment(), true).profile;
profile.window_title = true; // Caller knows this connection accepts OSC 2.
profile.hyperlinks = true;   // Caller knows this connection accepts OSC 8.
options.profile = profile;
auto terminal = esc::Terminal::open(options);
terminal.write(esc::WindowTitle{"Build results"});
terminal.write(esc::BeginHyperlink{"https://example.org/results", "build"});
terminal.write("Open results");
terminal.write(esc::EndHyperlink{});
terminal.flush();
```

These flags are declarations, not probes. Support for a title does not imply
support for hyperlinks. Sending literal text through `write` still accepts raw
bytes; use typed commands for validated OSC construction.

## Drawing controls

All types below are `DrawingCommand`s and work with string, reusable append,
span, and session encoders. Coordinates are zero-based. `ESC` denotes byte 0x1B.

| Value | Encoding / contract |
| --- | --- |
| `CursorStyle::Default` | `ESC[0 q` |
| BlinkingBlock / SteadyBlock | `ESC[1 q` / `ESC[2 q` |
| BlinkingUnderline / SteadyUnderline | `ESC[3 q` / `ESC[4 q` |
| BlinkingBar / SteadyBar | `ESC[5 q` / `ESC[6 q` |
| `SaveCursor{}` / `RestoreCursor{}` | `ESC7` / `ESC8` (DEC save/restore) |
| `ScrollRegion{top, bottom}` | `ESC[{top+1};{bottom+1}r`; inclusive bounds, `0 <= top < bottom` |
| `ResetScrollRegion{}` | `ESC[r` |
| `ScrollUp{count}` / `ScrollDown{count}` | `ESC[{count}S` / `ESC[{count}T` |
| `InsertCharacters{count}` / `DeleteCharacters{count}` | `ESC[{count}@` / `ESC[{count}P` |
| `InsertLines{count}` / `DeleteLines{count}` | `ESC[{count}L` / `ESC[{count}M` |

Counts are unsigned, default to one, and zero emits nothing. Encoding does not
clamp to a screen size. Row conversion widens before adding one. Invalid cursor
style enumerators and invalid region bounds throw `std::invalid_argument`.

Save/restore uses the terminal's single DEC saved state slot, not an application
stack. It can restore attributes and origin state as well as position. Margin
changes can move the cursor. Scrolling and insertion/deletion follow the
emulator's current margins and erase behavior. Escape does not infer these
states or create a screen model. Session cleanup does not automatically restore
cursor style, margins, saved cursor state, titles, or active hyperlinks; the
application must explicitly manage drawing effects it changes.

The encodings follow [xterm's control sequence reference](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html).

## Titles and hyperlinks

`WindowTitle{std::string_view text}` emits OSC 2. `BeginHyperlink{uri, id}` and
`EndHyperlink{}` emit OSC 8. Both use the seven-bit ST terminator (`ESC\\`), never
BEL. Empty titles are valid. BeginHyperlink requires a nonempty URI; use
EndHyperlink to end a link. An omitted ID emits an empty parameter field.

Title and URI payloads must be valid UTF-8 and contain no C0, DEL, or C1 Unicode
control characters. Invalid UTF-8 throws `UnicodeError`; forbidden controls throw
`std::invalid_argument`. Embedded NUL and ESC are rejected. Optional IDs accept
only ASCII unreserved characters `[A-Za-z0-9._~-]`; parameter delimiters cannot
be injected through an ID. URI bytes are otherwise preserved: there is no scheme
filter, escaping, normalization, or implied URI semantic validation. The emulator
and application decide which destinations are appropriate.

The views are borrowed for each synchronous call. Payload size is bounded by the
caller's storage and the destination's size, not an arbitrary 256-byte limit.
`Terminal::write` validates its entire command pack before any output and streams
OSC fragments directly to `Output`, without building an intermediate string.
I/O failures may leave partial output, as with other writes.

`encode_to` returns the exact required byte count and leaves an undersized span
unchanged. A payload overlapping the region that would be written is rejected
before mutation; disjoint payloads within the same larger allocation are valid.
`append_encoded` supports title/URI/ID views into its destination string, including
when growth relocates it. That aliasing case encodes a separate suffix and
may allocate; normal reserved appends and sufficient spans allocate nothing.
Validation/allocation failures leave the original destination unchanged.

OSC 8 behavior follows [the original hyperlink specification](https://gist.github.com/egmontkob/eb114294efbcd5adb1944c9f3cb5feda).

## Scoped synchronized output

```cpp
// Probe only when the application requests it; ordinary input is preserved.
auto confirmation = terminal.confirm_synchronized_output();
{
    auto frame = terminal.frame();
    terminal.write(esc::Cursor{});
    terminal.write("Updated screen");
    frame.close(); // Checked presentation/flush; destruction also attempts it.
}
```

`frame()` returns a move-only `Frame` with `close()` and `synchronized()`. A session
allocates shared guard state once during open; starting, nesting, moving, and
closing frames allocate nothing. Starting the outer frame flushes prior output,
then sends and flushes DEC mode 2026 On if support is selected and the inherited
baseline is Off. Nested guards add no mode changes. The last live guard to close
sends and flushes Off when the session enabled it. Guards may close in any order;
close is idempotent. Drawing and explicit flushes inside a frame remain allowed.

An explicit `profile.synchronized_output=true` enables frames without probing.
Alternatively, `confirm_synchronized_output(timeout=250ms)` sends ModeQuery 2026
and accepts mutable Set/Reset replies, recording support separately from the
profile. The confirmed mode also becomes the inherited baseline. Unrecognized
or permanent Set/Reset replies return `Rejected{reply}` and
disable synchronization. A timeout preserves earlier evidence. No query runs
automatically on open. Probing inside a frame is rejected before I/O.

Without support, BestEffort supplies a scope that flushes on its final close;
`synchronized()` is false. Strict rejects frame creation. For caller-known
inherited On, set `options.restore_modes.synchronized_output=On`; frames preserve
that state and do not send Off. The caller owns presentation of inherited output.

Frames follow a moved Terminal and may outlive a closed or destroyed session.
Their later close is harmless and `synchronized()` becomes false. Screen-buffer
changes are rejected while any frame is active; requesting the current buffer
is a no-op. Terminal close ends owned synchronization before restoring keyboard,
screen, and OS modes, even if guards remain alive.

A failed begin attempts rollback and reports a `SessionError` including rollback
failure. A failed end marks the session unusable for further operations; checked
Terminal close retries the owned cleanup. Destructors attempt cleanup without
throwing. Destruction cannot promise cleanup after process termination or a
permanently failed output device. Keep frames short: emulator timeouts and
implementation differences mean this is not a general atomic-output guarantee.

Protocol details are based on [the synchronized output specification](https://github.com/contour-terminal/vt-extensions/blob/master/synchronized-output.md).

## Event-loop readiness and deadlines

Both Reader and Terminal expose `wait_state()`:

```cpp
enum class WaitSourceKind { None, FileDescriptor, WindowsHandle };
struct WaitSource { WaitSourceKind kind; std::intptr_t value; };
struct WaitState {
    WaitSource source;
    bool ready;
    std::optional<std::chrono::steady_clock::time_point> deadline;
};
```

A snapshot primes the input with a zero-timeout read and preserves any resulting
event or EOF in bounded storage. `ready` means an event/EOF is held **or** input
can make immediate progress. `read(0ms)` can still return Timeout when the bytes
only extend an incomplete sequence or paste chunk. Refresh the snapshot and
continue while ready; do not sleep merely because one read returned Timeout.
Repeated snapshots do not discard held events. Cached EOF remains observable.

If not ready, wait on the borrowed source until the earliest of its deadline and
the application's own timers. Then read with zero timeout and refresh. Parser
Escape/sequence deadlines are absolute monotonic times and do not restart when
snapshots are taken. Terminal snapshots also include resize sampling deadlines
and any ordinary input queued during queries. A missing deadline means Escape
has no timer pending, provided a native source is available.

| Input transport | Wait source |
| --- | --- |
| POSIX descriptor | FileDescriptor; suitable for poll/select or host registration |
| Windows native console input | WindowsHandle; suitable for WaitForMultipleObjects |
| Windows explicit query/Kitty VT reader | WindowsHandle for the reader's completion event |
| Windows synchronous file/pipe without native readiness | None; deadline no later than a two-millisecond polling interval |

The value is an integer representation of a borrowed descriptor/HANDLE. Do not
close it, read it directly, or change its flags. On Windows, query/keyboard
transport changes can replace the source. Refresh and update host registration
after session operations, moves, and transport changes. Closing/destroying the
owner invalidates the source. All operations on one Reader/Terminal require
external serialization; there is no concurrent reader ownership.

[examples/event-loop.cpp](../examples/event-loop.cpp) is a compiling POSIX and
Windows loop combining input, Escape deadlines, and an application timer. A host
can add its shutdown pipe/socket to poll or a shutdown event to
WaitForMultipleObjects. Input ownership stays with Escape. Native Windows VT
queries preserve pending input until completion; close cancels and joins their
reader before releasing its wait source.

Zero-timeout parsing does not turn synchronous output, canonical echo, regular
file reads, or filesystem/device operations into hard real-time operations.
This API supplies input readiness and timer integration, not a nonblocking output
queue. Application signals and shutdown policy remain owned by the host.

## Verification

Core cases check literal encodings, boundaries, invalid UTF-8 and control payloads,
capability fallbacks, short spans, aliasing, and allocation contracts. POSIX PTYs,
pipes, and files check frame nesting/lifetime, inherited state, explicit probing,
partial writes and cleanup, long OSC streaming, cached input/EOF, absolute parser
and resize deadlines, large partial paste, and combined input/shutdown polling.
Windows cases cover console and VT completion handles, pipe timer fallback,
cached input, and frame mode restoration. See [test status](../tests/contract/README.md)
and [live acceptance checks](../tests/contract/live-terminals.md). Cross-builds
establish compilation/linking; native Windows/Linux and emulator rendering checks
remain separate pending evidence.

## Bell and note playback

`Bell{}` encodes one BEL byte (`0x07`). It requests the terminal's configured bell;
that may be audible, visual, or muted. It does not change styles or modes and
uses the same output buffering as other commands. Flush to deliver it immediately.

```cpp
terminal.write("Invalid input", esc::Bell{});
terminal.flush();
```

`PlayNotes` owns a sequence of DECPS musical notes:

```cpp
using namespace esc;
using namespace std::chrono_literals;

auto chime = PlayNotes{
    .notes = {Note::C5, Note::E5, Note::G5, Note::Rest, Note::C7},
    .duration = 125ms,
    .volume = 4,
};
auto bytes = encode(chime); // Pure encoding; no sound is played here.
```

Notes run chromatically from `C5` through `C7`. Sharps use `Cs5`, `Ds5`, `Fs5`,
`Gs5`, `As5`, and the corresponding octave-6 names. `Rest` is silence. Duration
is per note (including rests), stored in `std::chrono::microseconds`; it must be
an exact multiple of 31,250 microseconds, between zero and 7,968,750 inclusive
(0..255 DECPS ticks). Volume is 0..7; zero requests silent playback. Invalid
parameters throw `std::invalid_argument` before destination mutation or output,
even when playback would be omitted by a profile. Empty melodies emit nothing;
their duration and volume still validate. Melodies longer than 16 notes are split
into consecutive DECPS commands. Use multiple `PlayNotes` values for changes in
duration or volume. Output never sleeps or waits for playback to finish.

The encoding is `ESC [ volume ; ticks ; note [; note ...] , ~`, with note codes
0 for rest and 1..25 for C5..C7. This follows the
[DEC VT520 protocol](https://vt100.net/dec/ek-vt520-rm.pdf), supported by
[Windows Terminal](https://github.com/microsoft/terminal/blob/main/src/terminal/adapter/adaptDispatch.cpp)
and [mintty](https://github.com/mintty/mintty/wiki/CtrlSeqs#audio-support).
Sound output and scheduling remain subject to the terminal's implementation and
user configuration; writing successfully does not acknowledge audible playback.

`TerminalProfile::bell` and `TerminalProfile::play_notes` are independent flags,
both false by default. Bell can be enabled without cursor/color control;
`play_notes=true` requires `control=true`. Under `BestEffort`, an unsupported
command is omitted. Under `Strict`, it throws `UnsupportedFeature` with
`Feature::Bell` or `Feature::PlayNotes`. An empty, valid melody remains a no-op
under either policy. Playback never automatically falls back to a bell.

Known TERM entries enable basic bell support when output is a terminal.
Unrecognized TERM values and redirected output retain the plain fallback.
`NO_COLOR` does not affect bells. DECPS playback always requires an explicit
profile; neither `TERM=xterm` nor Windows VT mode establishes playback support.
For a terminal known to support DECPS, open the session with a caller-supplied
profile:

```cpp
auto profile = resolve_profile(snapshot_environment(), true).profile;
profile.control = true;
profile.bell = true;
profile.play_notes = true;
auto options = TerminalOptions{};
options.profile = profile;
auto terminal = Terminal::open(options);
terminal.write(chime);
terminal.flush();
```

Raw `encode`, `Output::write`, and free `write` always emit the requested command;
only profile-aware output applies capability policy. `Bell` and `PlayNotes` can
also be placed in `sequence` values, and melodies retain their owned notes.
