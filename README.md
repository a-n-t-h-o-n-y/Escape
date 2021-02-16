# Escape 🕳️

__Terminal Escape Sequence Library__

Consists mainly of functions returning escape sequence strings, that, when
written to stdout, will alter the appearance or functionality of the terminal
emulator.

Also provides a read() function that returns an Event variant for keyboard,
mouse and window resize events.

This library tries to be as general as possible and work with as many terminals
as possible without relying on the terminfo database.

Please open an issue if you find any bugs.

Note that not all terminals implement these features in the same way.

## Future Features

- Add `Terminal_focus_in` and `Terminal_focus_out` Events that are triggered on
  `CSI I` and `CSI O` input sequences. Enabled by Focus Event Mouse 1004. Only
  if most terminal emulators support this.

- Add BELL modifier sequences for frequency and duration.

- Check MacOS support

- Sixel graphics, but only if most terminal emulators support this.

## Known Issues

- Does not work with: aterm, eterm

- Some terminal emulators(Konsole) mouse input is always in `Mouse_mode::Drag`.
