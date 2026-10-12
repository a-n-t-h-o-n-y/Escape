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
   replacement preserves colors; Style replaces style; ResetStyle resets SGR.
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

## TEXT-LIVE: width policy checks

For each target, record terminal version, configured Unicode version (if known),
font/fallback fonts, ambiguous-width and emoji settings, and the chosen WidthPolicy.
Display plain samples between `|` markers on a line far from the right edge and
record the actual occupied cells; compare with `cell_width`. Do not infer results
from the grapheme conformance suite or from a terminal's name.

| Sample | Default estimated cells | What to check |
| --- | --- | --- |
| `e` + U+0301; U+00E9 | 1 each | Composed/decomposed accents |
| `中` | 2 | East Asian wide |
| `·` | 1 | Ambiguous one/two-cell setting |
| U+2764; U+2764 U+FE0F; U+2764 U+FE0E | 1; 2; 1 | Emoji/text variation selectors |
| `👩‍💻` | 2 | Joined emoji versus constituent rendering |
| `🇺🇸` | 2 | Paired regional indicators |
| `👍🏽` | 2 | Modifier attachment |
| `1️⃣` | 2 | Keycap sequence |
| U+1100 U+1161 U+11A8 | 2 | Decomposed Hangul |
| U+0915 U+094D U+0937 | 1 | Indic conjunct shaping |
| U+0301 without a base | 0 | Orphan-mark display policy |
| Application private-use symbols | Policy-dependent | Font-specific overrides |

Repeat relevant samples with Maximum/Additive clusters, EmojiWidth::One/Two,
AmbiguousWidth::One/Two, and OrphanMarkWidth::Zero/One. Record mismatches rather
than changing global defaults to match one host. Test both styled GlyphString
and plain text if the application changes fonts/styles within a cluster. Tabs,
newlines, ANSI controls, wrapping, and bidi layout are outside fixed width
measurement and need application-level tests. All real width checks remain
untested until a result is recorded.

## EXT-LIVE: controls, OSC, frames, and host waits

All of these real-emulator checks remain untested until recorded with the target
version and connection path. Enable capabilities explicitly for each connection.

1. Check all six explicit cursor shapes/blink choices and Default. Save/restore
   cursor state, and verify inclusive scroll margins, reset margins, scrolling,
   character insertion/deletion, and line insertion/deletion at boundaries.
   Restore the application's drawing state explicitly afterward.
2. Set empty and non-ASCII titles. Open, change, and end OSC 8 links, with and
   without IDs. Verify their displayed labels and destinations. Unsupported
   BestEffort paths should omit commands; Strict should reject before output.
3. Explicitly confirm mode 2026, record the exact reply, and test supported,
   rejected, and timeout paths. Present nested frames, unwind an exception, and
   close the session with a guard still live. Verify synchronization ends before
   screen cleanup, and verify a caller-supplied inherited On baseline is retained.
4. Run the event-loop example on POSIX, Windows native input, and negotiated
   Windows VT input. Verify input, Escape deadlines, resize, host timers and
   shutdown wakeups. Refresh host registrations after transport changes. Record
   pipe fallback timing separately from native readiness.
