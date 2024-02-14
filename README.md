# Escape 🕳️

__Terminal Escape Sequence Library__

This library provides support for generating terminal escape sequences. These
sequences, when written to `stdout`, enable control over terminal visual
presentation and behavior.

## Features

- **Dynamic Terminal Control**: Generate escape sequences for cursor movement, text formatting, color settings, and more.
- **Event Handling**: Includes a `read()` function to handle keyboard, mouse inputs, and window resize events, enhancing interactive applications.
- **Cross-Terminal Compatibility**: Designed to work across various terminals without relying on a terminfo database.

## Dependencies

- [ICU Library](https://icu.unicode.org/): For Unicode support.

## Build Instructions

    git clone https://github.com/a-n-t-h-o-n-y/Escape.git
    git submodule update --init --recursive     # Pull in Dependencies
    mkdir Escape/build && cd Escape/build
    cmake ..
    make escape                                 # Build Library
    make escape.tests.unit                      # Build Unit Tests (Optional)
    make escape.examples.pong                   # Build PONG Game (Optional)

## Example Code

- [tools/termcaps.cpp](./tools/termcaps.cpp)
- [examples/pong.cpp](./examples/pong.cpp)
- [tests/glyph.test.cpp](./tests/glyph.test.cpp)

## Planned Enhancements

- **Focus Events**: Integrate `TerminalFocusIn` and `TerminalFocusOut` events
for focus tracking. Events are triggered on `CSI I` and `CSI O` input sequences,
enabled by Focus Event Mouse 1004. Dependent on widespread terminal emulator
support.
- **BELL Sequences**: Add BELL sequences to adjust alert frequency and duration.
- **MacOS Compatibility**: Ensure full support across MacOS terminals.
- **Sixel Graphics**: Explore incorporating Sixel graphics for advanced terminal
imaging, contingent on emulator compatibility.

## Current Limitations

- **Incompatible Terminals**: Known issues with `aterm` and `eterm`.
- **Mouse Input Handling**: In Konsole, mouse input is limited to `MouseMode::Drag`.

## Contributing

Feedback and contributions are highly appreciated. If you encounter any issues
or have suggestions for improvements, please open an issue on our repository.

## Note

Terminal features and support vary significantly across emulators. It's
recommended to test functionality in your target environments.