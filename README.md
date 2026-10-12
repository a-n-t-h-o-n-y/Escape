# Escape 🕳️

__Terminal Escape Sequence Library__

`Escape` provides support for generating and interpreting terminal escape sequences.
These can be written to the terminal to control cursor movement, text formatting, and
colors. It also includes a `read()` function to produce keyboard input, mouse input, and
window resize events.

```cpp
#include <iostream>
#include <esc/esc.hpp>

using namespace esc;

int main()
{
    std::cout
        << escape(Brush{
                .background = XColor::Cyan,
                .foreground = TColor{0xF02127},
                .traits     = Trait::Italic,
           }) << "Hello"

        << escape(Brush{}) << " 👽 "

        << escape(
                fg(TColor{0x76F09C}),
                Trait::Bold | Trait::Underline
           ) << "World!"

        << escape(Brush{}) << std::endl;
}
```

Strict Unicode conversion and glyph styling are available through `<esc/text.hpp>`
and `<esc/glyph.hpp>`:

```cpp
auto scalars = esc::utf8_to_utf32("Hello 👽");
auto text = esc::glyphs("Hello 👽");
auto highlighted = text | esc::fg(esc::XColor::Cyan); // text is unchanged
esc::style_in_place(text, esc::Trait::Bold);
```

`GlyphString` is `std::vector<Glyph>`; a glyph represents a Unicode scalar without
an implied terminal cell width. Invalid Unicode throws `UnicodeError` with the
input offset. Pipes return owned values; named append and mutation APIs support
reusable buffers and mutable ranges. See the [interface proposal](docs/interface-proposal.md)
and [contract-test instructions](tests/contract/README.md) for the refactor's scope.

See [TermOx](https://github.com/a-n-t-h-o-n-y/TermOx) for a more full-featured widget
based library that uses `Escape` under the hood.

## Features

- **Dynamic Terminal Control**: Generate escape sequences for cursor movement, text formatting, and colors.
- **Event Handling**: Includes a `read()` function to handle keyboard, mouse input, and window resize events, enabling interactive terminal applications.
- **Cross-Terminal Compatibility**: Designed to work across various terminals without relying on a terminfo database.

## Dependencies

- [zzz Library](https://github.com/a-n-t-h-o-n-y/zzz)
    - Utilities and Tests.
    - CMake fetches this on configure.

## Build Instructions

    git clone https://github.com/a-n-t-h-o-n-y/Escape.git
    mkdir Escape/build && cd Escape/build
    cmake ..
    make escape                                 # Build Library
    make escape.examples.pong                   # Build PONG Game (Optional)

Capability-aware encoding is available through `<esc/capabilities.hpp>`.
Pass a `TerminalProfile` before commands to `escape`, `append_escape`, or
`escape_to`. Best effort adapts colors and traits; strict mode rejects losses.
Unsupported geometry and protocol modes throw in either policy.
`resolve_profile(environment, output_is_terminal)` is pure; call
`snapshot_environment()` explicitly to read the process environment. TERM
resolution uses the exact heuristic table in the interface proposal, with
per-feature evidence; live terminal verification remains future work.

The implemented contract-test slices are opt-in:

```sh
cmake -DESCAPE_BUILD_CONTRACT_TESTS=ON ..
make escape.tests.contract.encoding escape.tests.contract.text escape.tests.contract.capabilities
ctest -R '^escape\.contract\.(encoding|text|capabilities)\.' --output-on-failure
```

The legacy glyph tests remain for migration review and no longer compile against
the new interface. Parser and session contracts are specified but
remain under implementation; see the contract-test README for their status.

## Example Code

- [tools/termcaps.cpp](./tools/termcaps.cpp)
- [examples/pong.cpp](./examples/pong.cpp)
- [tests/contract/text.test.cpp](./tests/contract/text.test.cpp)
- [tests/contract/sequence.test.cpp](./tests/contract/sequence.test.cpp)

## TODO

- **Focus Events**: Integrate `TerminalFocusIn` and `TerminalFocusOut` events for focus
tracking. Events are triggered on `CSI I` and `CSI O` input sequences, enabled by Focus
Event Mouse 1004. Dependent on widespread terminal emulator support.
- **BELL Sequences**: Add BELL sequences to adjust alert frequency and duration.

## Current Limitations

- **Incompatible Terminals**: Known issues with `aterm` and `eterm`.
- **Mouse Input Handling**: In Konsole, mouse input is limited to `MouseMode::Drag`.

## Contributing

Feedback and contributions are appreciated. If you encounter any issues or have
suggestions for improvements, please open an issue on our repository.

## Note

Terminal features and support vary significantly across emulators. It's recommended to
test functionality in your target environments.