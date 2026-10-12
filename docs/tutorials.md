# From a log line to an interactive picker

The three examples below share the same output vocabulary. Start with styled
text, use terminal-cell widths to align it, then add a session and keyboard input.
Each source is a complete program built with the examples.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/examples/escape.tutorial.output
./build/examples/escape.tutorial.layout
./build/examples/escape.tutorial.picker
```

## 1. Compose a log line

[output.cpp](../examples/tutorials/output.cpp) nests a dim detail inside a green,
bold message. The inner run restores the outer style when it finishes.

<!-- example: examples/tutorials/output.cpp -->
```cpp
#include <esc/esc.hpp>

int main()
{
    using namespace esc;
    auto profile = resolve_profile(snapshot_environment(), true).profile;
    auto success = fg(colors::green) | bold;
    auto detail = styled("42 files", dim);
    auto message = styled(sequence("Built ", detail, " successfully"), success);
    Output output;
    output.write(encode(profile, message, "\n"));
    output.flush();
}
```

`fg(colors::green) | bold` produces a style patch. `styled` owns its content;
`sequence` owns the pieces it combines. Neither writes to the terminal.
`encode` turns the whole value into bytes, and `Output::write` sends them.
`flush` delivers any bytes still held by stdio.

The profile adapts the requested colors and traits to the selected terminal.
`snapshot_environment` reads TERM, COLORTERM, and NO_COLOR; `resolve_profile`
is a pure function over that snapshot. These two output examples assume stdout
is a terminal. Pass `false` as its second argument when output is redirected.

For plain logs, `TerminalProfile{}` drops styling. Raw `encode(message)` keeps
exactly the requested commands. See [profiles and colors](behavior.md#colors-and-profiles).

## 2. Align Unicode text

[layout.cpp](../examples/tutorials/layout.cpp) uses the same styles, but builds a
table in one reusable string.

<!-- example: examples/tutorials/layout.cpp -->
```cpp
#include <array>
#include <esc/esc.hpp>
#include <string>
#include <string_view>

int main()
{
    using namespace esc;
    constexpr std::array<std::string_view, 3> names = {"Café", "東京", "e\u0301clair"};
    auto heading = fg(colors::cyan) | bold;
    auto profile = resolve_profile(snapshot_environment(), true).profile;
    std::string table = encode(profile, styled("Name      Status", heading), "\n");
    for (auto name : names) {
        auto width = cell_width(name).value();
        append_encoded(table, profile, name, std::string(10 - width, ' '),
                       styled("ready", fg(colors::green)), "\n");
    }
    Output{}.write(table);
    flush();
}
```

A UTF-8 string's `size()` counts bytes. `cell_width` estimates terminal columns:
`東京` occupies four columns under the default policy, while `e\u0301` occupies
one. Padding by measured cells keeps the status column aligned.

These names are fixed, printable strings shorter than ten cells, so `.value()`
and the subtraction are safe here. For user input, handle `nullopt`, cap the
width before subtracting, and catch `UnicodeError`. Newlines, tabs, and other
cursor-dependent controls have no fixed width.

`append_encoded` preserves the existing prefix. Reserving space ahead of time
avoids repeated allocation when assembling larger tables. Use `graphemes` to
walk visible clusters without splitting a combining sequence. See
[text layout](text-layout.md) for font and terminal width policies.

## 3. Add input and a screen

[picker.cpp](../examples/tutorials/picker.cpp) is the complete program in the
[README](../README.md). Its drawing code is the same `styled`/`sequence` code
from the first two examples, now written through a `Terminal`.

`TerminalOptions::interactive()` requests immediate input, disables echo,
switches to the alternate screen, and hides the cursor. The session owns those
changes and restores them when closed or destroyed.

Each iteration starts a frame, moves to `Cursor{}` (column 0, row 0), and draws
the choices. The selected label gets the cyan/bold patch; an empty `StylePatch`
leaves other labels at the surrounding appearance. `BlankRow{Erase::ToEnd}` erases stale
characters after shorter lines. `frame.close()` flushes the drawing, and uses
synchronized output when support has been explicitly enabled or confirmed.

`read()` blocks until input is available. Its result distinguishes `Event`,
`Timeout`, and `EndOfInput`. The picker looks for a `KeyEvent`; other events,
including resize, cause the loop to redraw. `matches` checks the key, modifiers,
and action, defaulting to an unmodified press. Arrow keys wrap the selection;
Enter accepts it; `q` or Escape cancels.

Closing before printing the answer returns to the original screen. Cancellation
returns through the destructor, so it restores the terminal too. Applications
that need to report cleanup errors should call `close()` explicitly.

## Where to go next

- [Reference](reference.md) for functions and command values.
- [Terminal behavior](behavior.md) for borrowing, failures, and mode restoration.
- [Queries and keyboard input](queries-and-keyboard.md) for explicit negotiation.
- [Output extensions](terminal-extensions.md) for frames, links, and sound.
- [Lumen and Pong](demos.md) for larger programs.
