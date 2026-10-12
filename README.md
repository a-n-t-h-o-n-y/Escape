# Escape

C++20 terminal I/O for Linux, macOS, and Windows. Compose styles and commands as
values, read typed input events, and let a session restore the terminal on exit.
No third-party runtime dependencies.

Here's a small destination picker:

<!-- example: examples/tutorials/picker.cpp -->
```cpp
#include <array>
#include <esc/esc.hpp>
#include <string_view>

int main()
{
    using namespace esc;
    constexpr std::array<std::string_view, 3> choices = {"Local", "Staging",
                                                         "Production"};
    auto accent = fg(colors::cyan) | bold;
    auto terminal = Terminal::open(TerminalOptions::interactive());
    unsigned selected = 0;

    for (;;) {
        auto frame = terminal.frame();
        terminal.write(Cursor{}, styled("Choose a destination", bold),
                       BlankRow{Erase::ToEnd}, "\r\n");
        for (unsigned i = 0; i < choices.size(); ++i)
            terminal.write(styled(sequence(i == selected ? "> " : "  ", choices[i]),
                                  i == selected ? accent : StylePatch{}),
                           BlankRow{Erase::ToEnd}, "\r\n");
        terminal.write(styled("Up/Down choose · Enter accept · q cancel", dim),
                       BlankRow{Erase::ToEnd});
        frame.close();

        auto input = terminal.read();
        if (std::holds_alternative<EndOfInput>(input)) return 0;
        auto event = std::get_if<Event>(&input);
        auto key = event ? std::get_if<KeyEvent>(event) : nullptr;
        if (!key) continue;
        if (key->matches(U'q') || key->matches(KeyCode::Escape)) return 0;
        if (key->matches(KeyCode::ArrowDown))
            selected = (selected + 1) % choices.size();
        if (key->matches(KeyCode::ArrowUp))
            selected = (selected + choices.size() - 1) % choices.size();
        if (key->matches(KeyCode::Enter)) break;
    }
    terminal.close();
    write("Selected ", choices[selected], "\n");
    flush();
}
```

Arrow keys select a destination; Enter accepts it. The alternate screen and
cursor are restored before printing the selection. Styles compose with `|`,
and `styled` restores the surrounding appearance after each label.

## Build

Requires CMake 3.16+ and a C++20 compiler.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
./build/examples/escape.tutorial.picker
```

For your own project, link `Escape::escape` with `add_subdirectory` or
`find_package(Escape CONFIG REQUIRED)`. See [building and installing](docs/building.md).

## More

- [Tutorial walkthrough](docs/tutorials.md): styled output, Unicode columns, then the picker.
- [API reference](docs/reference.md): function guide and optional generated HTML.
- [Terminal behavior](docs/behavior.md): colors, ownership, modes, and errors.
- [Demos](docs/demos.md): Lumen, Pong, and the terminal capability tester.

[TermOx](https://github.com/a-n-t-h-o-n-y/TermOx) builds widgets on top of Escape.
