# Escape 🕳️

Escape is a C++20 library for terminal byte encoding, Unicode text, incremental
input parsing, and scoped POSIX terminal sessions. The refactor on this branch
is a breaking interface revision; the complete contract is in
[docs/interface-proposal.md](docs/interface-proposal.md).

## Encoding

Raw encoding is pure and does not read the environment or retain drawing state:

```cpp
#include <esc/esc.hpp>

auto bytes = esc::escape(esc::Cursor{0, 0}, esc::fg(esc::XColor::Cyan));
std::string frame;
frame.reserve(4096);
esc::append_escape(frame, esc::Cursor{4, 2}, esc::Trait::Bold);

std::array<char, 128> buffer;
auto result = esc::escape_to(buffer, esc::Cursor{4, 2});
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
terminal.send(esc::BlankScreen{}, esc::Cursor{});
terminal.write("Hello 👽");
terminal.flush();

auto result = terminal.read(std::chrono::milliseconds{100});
if (result.status == esc::ReadStatus::Event) {
    // Visit *result.event: keys, mouse, resize, focus, paste, or unknown input.
}
terminal.close(); // Reports cleanup errors; destruction also attempts cleanup.
```

Sessions borrow input/output streams referring to the same terminal device.
Only one session owns a device's modes in this process. Passive options preserve
modes; interactive options request immediate input, echo off, alternate screen,
and a hidden cursor. Mouse, focus, and paste are explicit opt-ins. Closing restores
captured input settings and changed protocol modes to the supplied initial state.
The host application owns signal handling.

`Output` also writes directly to a borrowed FILE* without a terminal session.
Raw byte views preserve their full length, including nulls. UTF-32 output is
validated before a variadic write and encoded in bounded chunks. No output API
adds separators, newlines, private buffering, or automatic flushing.

`Parser` owns bounded incomplete input and performs no I/O or clock access.
`Reader` combines it with descriptor reads, monotonic deadlines, and explicit
Event/Timeout/EndOfInput results. A Reader's input must not have unread stdio
buffering or concurrent fread/getchar/iostreams users. Parser protocol changes
apply only to new sequence beginnings. Unknown input stays owned and bounded;
paste preserves original bytes. Ordinary keys and mouse events allocate nothing
after parser construction.

## Unicode and glyphs

```cpp
auto scalars = esc::utf8_to_utf32("Hello 👽");
auto text = esc::glyphs("Hello 👽");
auto highlighted = text | esc::fg(esc::XColor::Cyan); // text is unchanged
esc::style_in_place(text, esc::Trait::Bold);
```

Conversions are strict. `UnicodeError::offset()` identifies invalid input.
`GlyphString` is a vector of styled Unicode scalars; a glyph does not promise
one screen cell. Pipes return values; named append and mutation APIs support
reusable storage. Grapheme layout and terminal cell width remain outside the API.

## Build and install

Requires a C++20 compiler, CMake 3.16+, and macOS/Linux for Reader and Terminal.
No third-party library is fetched or required. Python 3 enables package/header
verification tests.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --build build --target termcaps escape.examples.pong
```

`ESCAPE_BUILD_TESTS` and `ESCAPE_BUILD_EXAMPLES` default on for a top-level
build and off for add_subdirectory consumers. `ESCAPE_BUILD_CONTRACT_TESTS`
remains an opt-in compatibility switch. Fault injection is private to test builds.

```sh
cmake -S . -B build-install -DESCAPE_BUILD_TESTS=OFF -DESCAPE_BUILD_EXAMPLES=OFF \
    -DCMAKE_INSTALL_PREFIX=/your/prefix
cmake --build build-install
cmake --install build-install
```

Consumers use `Escape::escape` after either `add_subdirectory(Escape)` or
`find_package(Escape CONFIG REQUIRED)`. The development package version is
0.2.0; this branch has not been published as a release.

See [contract tests](tests/contract/README.md) for verification and
[the acceptance matrix](tests/contract/live-terminals.md) for real terminal
validation. Synthetic fixtures and PTYs check library behavior, not emulator
rendering or every terminal's advertised capabilities. Windows sessions, active
probing, enhanced keyboard protocols, width calculation, and benchmarks remain
deferred.

[TermOx](https://github.com/a-n-t-h-o-n-y/TermOx) provides a higher-level widget
library; its callers need migration to this revised interface.
