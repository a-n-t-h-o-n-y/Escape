# Escape 🕳️

Escape is a C++20 library for terminal byte encoding, Unicode text, incremental
input parsing, and scoped terminal sessions for Linux, macOS, and Windows. The refactor on this branch
is a breaking interface revision; start with the
[functional API contract](docs/functional-api.md) and the
[session/protocol contract](docs/interface-proposal.md).

## Encoding

Raw encoding is pure and does not read the environment or retain drawing state:

```cpp
#include <esc/esc.hpp>

auto warning = esc::fg(esc::colors::yellow) | esc::bold;
auto label = esc::styled("Warning 👽", warning);
auto bytes = esc::encode(esc::Cursor{0, 0}, label);
std::string frame;
frame.reserve(4096);
esc::append_encoded(frame, esc::Cursor{4, 2}, label, "\n");

std::array<char, 128> buffer;
auto result = esc::encode_to(buffer, esc::Cursor{4, 2});
// result.required reports the complete size. An undersized span is unchanged.
```

Pass a `TerminalProfile` before commands for capability-aware encoding.
Best effort adapts colors and traits; strict mode rejects losses. Unsupported
geometry and protocol modes throw. Profiles and caller-supplied palettes are
ordinary values. `resolve_profile(environment, output_is_terminal)` is pure;
`snapshot_environment()` explicitly reads current environment values. Exact
TERM matches use the documented heuristic table and return per-feature evidence.

## Sessions and input

```cpp
auto terminal = esc::Terminal::open(esc::TerminalOptions::interactive());
terminal.write(esc::BlankScreen{}, esc::Cursor{},
               esc::styled("Hello 👽", esc::fg(esc::colors::cyan) | esc::bold));
terminal.flush();

auto result = terminal.read(std::chrono::milliseconds{100});
esc::match(result,
    [](esc::Event const& event) { /* Visit keys, mouse, resize, focus, paste, replies. */ },
    [](esc::Timeout) { /* No event before the deadline. */ },
    [](esc::EndOfInput) { /* Input is exhausted. */ });
terminal.close(); // Reports cleanup errors; destruction also attempts cleanup.
```

Sessions borrow input/output streams referring to the same terminal device.
Only one session owns a device's modes in this process. Passive options preserve
modes; interactive options request immediate input, echo off, alternate screen,
and a hidden cursor. Mouse, focus, and paste are explicit opt-ins. Closing restores
captured input settings and changed protocol modes to the supplied initial state.
The host application owns signal handling.

On Windows, opening a session enables VT output, selects UTF-8 output, and puts
its borrowed output descriptor in binary mode; closing restores the captured
console modes, code pages, and descriptor mode. Input uses Unicode console
records with key repeats, deadlines, and canonical line editing. The automatic
profile assumes only sixteen colors, bold/underline/inverse, alternate screen,
and cursor visibility. Mouse, focus, paste, and richer styles need an explicit
profile for the connection. Windows Terminal and the Windows console host are
manual validation targets. Console sessions require native console handles;
redirected files and pipes use `Reader`/`Output` directly. MSYS/mintty pipe handles
are not native console sessions.

For standalone `Output` on Windows, open byte streams in binary mode (`"wb"`,
or host-owned `_setmode(_fileno(stream), _O_BINARY)`) to prevent CRT newline
translation. `Output` itself preserves the borrowed stream's mode.

`Output` also writes directly to a borrowed FILE* without a terminal session.
Raw byte views preserve their full length, including nulls. UTF-32 output is
validated before a variadic write and encoded in bounded chunks. No output API
adds separators, newlines, private buffering, or automatic flushing.

`Parser` owns bounded incomplete input and performs no I/O or clock access.
`Reader` combines it with descriptor reads, monotonic deadlines, and explicit
Event/Timeout/EndOfInput variant results. A Reader's input must not have unread stdio
buffering or concurrent fread/getchar/iostreams users. Parser protocol changes
apply only to new sequence beginnings. Unknown input stays owned and bounded;
paste preserves original bytes. Ordinary keys and mouse events allocate nothing
after parser construction.

Scoped frames, cursor styles, scrolling/editing controls, validated titles and
hyperlinks, and host event-loop readiness are implemented. Extensions require
explicit capability selection or an explicit synchronization probe. See
[their contract and usage](docs/terminal-extensions.md) and the compiling
[event-loop example](examples/event-loop.cpp).

## Unicode and glyphs

```cpp
auto scalars = esc::utf8_to_utf32("Hello 👽");
auto text = esc::styled("Hello 👽", esc::fg(esc::colors::cyan));
auto highlighted = text | esc::bold; // text is unchanged
esc::write(highlighted);

auto glyphs = esc::glyphs("Hello 👽"); // Explicit per-scalar storage.
esc::style_in_place(glyphs, esc::bold);
esc::write(glyphs);
```

Conversions are strict. `UnicodeError::offset()` identifies invalid input.
`GlyphString` is a vector of styled Unicode scalars; a glyph does not promise
one screen cell. Pipes return values; named append and mutation APIs support
reusable storage. Borrowed grapheme views and explicit cell-width policies are available through
`graphemes`, `cell_width`, and `grapheme_width`; see [text layout](docs/text-layout.md).

## Build and install

Requires a C++20 compiler and CMake 3.16+. Session backends target Linux, macOS,
and Windows 10/11 consoles with VT output support. Windows builds support MSVC;
the public headers contain no platform console types.
Terminfo is not supported. Capability selection uses profiles, environment
heuristics, and explicit queries. No third-party library is fetched or required.
Python 3 enables package/header
verification tests.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
cmake --build build --config Release --target termcaps escape.examples.pong
```

`ESCAPE_BUILD_TESTS` and `ESCAPE_BUILD_EXAMPLES` default on for a top-level
build and off for add_subdirectory consumers. `ESCAPE_BUILD_CONTRACT_TESTS`
remains an opt-in compatibility switch. Fault injection is private to test builds.

```sh
cmake -S . -B build-install -DESCAPE_BUILD_TESTS=OFF -DESCAPE_BUILD_EXAMPLES=OFF \
    -DCMAKE_INSTALL_PREFIX=/your/prefix
cmake --build build-install --config Release
cmake --install build-install --config Release
```

Consumers use `Escape::escape` after either `add_subdirectory(Escape)` or
`find_package(Escape CONFIG REQUIRED)`. The development package version is
0.3.0; this branch has not been published as a release.

See [contract tests](tests/contract/README.md) for verification and
[the acceptance matrix](tests/contract/live-terminals.md) for real terminal
validation. Synthetic fixtures and PTYs check library behavior, not emulator
rendering or every terminal's advertised capabilities. CI builds and runs behavior
tests on Linux, macOS, and Windows; a configured CI job is not a recorded passing
run. Terminal-specific width validation remains future work.
Explicit queries, capability observations, and negotiated Kitty keyboard input are
implemented; see [their interface and usage](docs/queries-and-keyboard.md).

See the [implementation review](docs/implementation-review.md) for corrected
Windows input/echo behavior and the measured paste parsing optimization.
See the [performance review](docs/performance-review.md) for subsequent encoding,
palette, Unicode, layout, and reader optimizations and their measurements.

[TermOx](https://github.com/a-n-t-h-o-n-y/TermOx) provides a higher-level widget
library; its callers need migration to this revised interface.

## Benchmarks

Set `ESCAPE_BUILD_BENCHMARKS=ON` for the standalone `escape.benchmarks` target
(default off). See [workloads, commands, and the local baseline](benchmarks/README.md)
for encoding, reused buffers, parser, reader, and output measurements.
