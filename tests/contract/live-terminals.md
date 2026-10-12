# LIVE-01: real-terminal acceptance checks

These checks are intentionally manual until an emulator-specific automation
adapter exists. A PTY does not render output, and synthetic parser fixtures are
not evidence that a real terminal sends those bytes.

All rows begin **untested**. Do not mark a row passing from unit-test results.

| Target | Version / OS | TERM / COLORTERM | SSH / multiplexer path | Result |
| --- | --- | --- | --- | --- |
| xterm | Record at execution | Record at execution | Record at execution | Untested |
| macOS Terminal | Record at execution | Record at execution | Record at execution | Untested |
| VTE terminal | Record at execution | Record at execution | Record at execution | Untested |
| Konsole | Record at execution | Record at execution | Record at execution | Untested |
| kitty | Record at execution | Record at execution | Record at execution | Untested |
| Alacritty | Record at execution | Record at execution | Record at execution | Untested |
| WezTerm | Record at execution | Record at execution | Record at execution | Untested |
| tmux | Record at execution | Record at execution | Record at execution | Untested |
| screen | Record at execution | Record at execution | Record at execution | Untested |
| SSH connection | Record at execution | Record at execution | Record at execution | Untested |
| Linux console | Record at execution | Record at execution | Record at execution | Untested |
| Windows Terminal / ConPTY | Record at execution | Record at execution | Record at execution | Untested |
| Windows console host | Record at execution | Record at execution | Record at execution | Untested |

For each connection, record the profile and fallback policy, then verify:

1. Position labeled markers at origin, interior, and right/bottom edges. Confirm
   coordinate orientation, boundaries, and relative movement without wrapping.
2. Erase beginning/end/all of a line and display. Confirm cursor position is
   unchanged, scrollback is preserved, and document the erase-background result.
3. Display default colors, all basic/bright slots, selected cube/grayscale slots,
   and RGB samples supported by the profile. Verify configured palette behavior
   and intended lower-depth fallback. NO_COLOR suppresses color requests.
4. Display each supported trait and replacement combinations. Verify trait
   replacement preserves colors; Brush replaces style; ResetStyle resets SGR.
5. Capture actual keyboard bytes for arrows, home/end, function keys, keypad,
   modifiers, Escape, Alt, ASCII, and multibyte text. Preserve ambiguous cases as
   ambiguous, rather than labeling them physical-key identities.
6. Capture press/release/drag/motion and both wheel axes under each enabled mouse
   protocol. Confirm cell coordinates and unknown-button release semantics.
7. Enable focus and paste only when supported. Capture focus transitions, empty
   and large paste, multibyte payload, control bytes, and delimiters.
8. Resize repeatedly and verify observed dimensions and documented coalescing.
9. Close normally and unwind an application exception. Verify captured OS modes
   and declared protocol baseline are restored, without extra style/autowrap
   changes. Separately document shutdown/suspend handling owned by the application.

Store captured byte fixtures as hexadecimal plus expected semantic events.
Attach emulator/version, environment, profile, connection path, capture method,
and date. Label synthetic fixtures as synthetic. Do not include clipboard,
private pasted data, or unrelated terminal output in committed captures.
