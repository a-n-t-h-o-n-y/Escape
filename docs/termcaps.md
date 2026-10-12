# Interactive capability tester

Build and launch the executable in the terminal you want to inspect:

```sh
cmake --build build --target termcaps
./build/tools/termcaps
```

The tester uses an alternate screen and restores the screen, cursor, input modes,
and negotiated keyboard flags on normal exit. Press `q` or Ctrl+C to exit; your
visual verdicts and latest query results are printed afterward. Cursor shape and
scroll margins return to their defaults. It needs at least 70 columns and 24 rows;
resizing smaller shows a compact message until space is available again.

Use Left/Right or Tab to change pages, or jump with `1` through `9` and `0`.
Press `y` for a passing visual result, `x` for a failure, and `u` to clear the
current page's verdict. Nothing is automatically marked as passing.

| Page | What to inspect |
| --- | --- |
| 1 System palette | Sixteen themed colors, including bright variants |
| 2 256 colors | Indexed color swatches; Up/Down scroll through rows on short screens |
| 3 True color | Three RGB gradients; look for smooth transitions |
| 4 Text styles | Bold, dim, italic, underline, double underline, strikeout, inverse, blink, and hidden text |
| 5 Unicode | Combining marks, CJK, emoji/ZWJ, flags, and box drawing; compare closing-bar alignment |
| 6 Input events | Decoded keys/actions/modifiers/text, mouse, focus, paste, and resize events |
| 7 Cursor controls | Shapes/visibility, save/restore, character editing, and scrolling |
| 8 Hyperlinks | OSC 8 link interaction and correctly ended link scope |
| 9 Sound | Configured bell and DECPS chime, triggered only by explicit keys |
| 0 Probe & results | Terminal replies or environment assumptions; Up/Down scroll |

Input controls: `m` toggles SGR mouse tracking, `f` toggles focus reporting, and
`v` toggles bracketed paste. `k` queries and negotiates the Kitty keyboard protocol.
These requests exercise protocols even when the environment resolver is uncertain
about their support. Input is shown as descriptions; pasted or unknown control
bytes are escaped and never replayed into the terminal. Global shortcuts, including
`q`, remain reserved while inspecting keys. Rendering is limited to 20 frames per
second so mouse movement does not trigger an unbounded redraw rate.

Cursor controls: `c` cycles the six explicit cursor shapes and shows the cursor at
the target; `h` toggles visibility. `s` should place `PASS` below the target, leaving
`decoy` elsewhere. `e` edits `ABCD` into `AC!D` using deletion and insertion. `r`
scrolls a three-line region into `row 2`, `row 3`, and `tail`. Leaving the cursor
page hides the cursor and resets its shape.

Sound controls: `b` sends BEL; the response may be audible, visual, or muted. `n`
sends a short C5/E5/G5/C6 DECPS melody. Sound is never played on startup or simply
by navigating to the page. Actual playback depends on terminal support and user
preferences.

From any page, `p` probes keyboard and terminal modes, `t` queries device
attributes, and `z` checks synchronized-output mode support. `a` opens/toggles the
environment-assumption view. A timeout means no reply was observed; it does not
prove lack of support. A recognized mode reply is protocol evidence, and does not
prove that all rendering or input behavior works. The environment view comes from
the original TERM/COLORTERM/NO_COLOR resolver, independently of the tester's
explicit session profile.

Samples use raw commands rather than capability fallback: RGB stays RGB, indexed
colors stay indexed, and unsupported samples are not silently approximated. The
interface uses terminal defaults and a cyan accent; `NO_COLOR` removes that accent
while retaining the intentional color test samples. Basic cursor positioning and
alternate-screen support are prerequisites for the interactive interface, not
capabilities it can independently establish. Fonts and terminal width policies can
affect the Unicode samples.

```sh
termcaps --report          # Plain environment assumptions; no control sequences
termcaps --probe           # Run mode queries at startup
termcaps --kitty           # Negotiate enhanced keyboard input at startup
termcaps --force           # Try interactive mode despite empty/dumb TERM
termcaps --help
```

Redirected input/output automatically selects the plain report. Empty or `dumb`
TERM also selects it unless `--force` is supplied; forcing still requires terminal
input and output. The plain report supports copying into an issue or log. The
interactive summary records human observations, separately from the resolver's
assumptions and query replies.
