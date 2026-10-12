# Explicit queries and enhanced keyboard input

Escape sends no queries and enables no keyboard extensions during `Terminal::open`.
Ordinary sessions retain the native/POSIX input path. All query operations require
an open control-capable session with immediate input; Windows also requires echo
off. `TerminalOptions::interactive()` supplies these input settings.

## Query interface

```cpp
auto terminal = esc::Terminal::open(esc::TerminalOptions::interactive());
auto result = terminal.query(esc::ModeQuery{2004}, std::chrono::milliseconds{100});
if (auto value = std::get_if<esc::Reply>(&result)) {
    auto reply = std::get<esc::ModeReply>(*value);
    // reply.status distinguishes unrecognized, set/reset, and permanent states.
}
```

`Query` is a variant of:

- `DeviceAttributesQuery`: primary device attributes (`CSI c`). Reply contains
  at most 32 numeric attributes, each at most 65535. These identify advertised
  attributes, not the identity or trustworthiness of an emulator.
- `ModeQuery{mode}`: a DEC private mode in 1..65535 (`CSI ? mode $ p`).
  `ModeReply` includes that mode and one of Unrecognized/Set/Reset/
  PermanentlySet/PermanentlyReset. Permanent states are not evidence that the
  application can toggle a setting.
- `KeyboardQuery`: Kitty enhancement flags (`CSI ? u`); `KeyboardReply` preserves
  the complete unsigned flags value, including future bits.

`query_to(span, Query)` and `encode_query(Query)` are pure request encoders.
Bounded encoding is all-or-nothing, reports the exact size, and allocates nothing.
Invalid mode requests throw before modifying storage or writing bytes.

`Terminal::request(query)` sends and flushes without waiting. Its span overload
validates the entire request pack first, uses bounded stack chunks, and flushes
once. The default ten-request probe fits one output write. The caller receives
`TerminalReply` events through `read`. `Terminal::query(query, timeout)` waits for
one matching reply and returns `QueryResult`. Replies match type and, for mode
queries, the mode number. Unrelated replies and ordinary input remain ordered.
Late replies remain typed events instead of being silently discarded.

`QueryResult = std::variant<Reply, Timeout, EndOfInput, InputPending, Rejected>`.
Exactly one outcome is represented, with no separate optional payload.
`Rejected{Reply reply}` preserves the actual acknowledgment for diagnostics. Rejected is
used by keyboard negotiation when a returned flags value differs from the request.
A timeout is lack of confirmation, not proof of unsupported functionality.
Negative timeouts throw before output. Zero permits a non-sleeping poll. Deadlines
use the steady clock and do not restart on input or interruptions; synchronous
output writes/flushes themselves cannot be bounded by a read deadline.

While waiting, a session preserves up to 64 events in a preallocated ring. Before
reading another event when full it returns InputPending. `read()` drains these
first; callers must drain and decide whether to retry. No library input is dropped
or unbounded queue allocated. Parser sequence/paste limits bound each event's
payload; device attributes have their own fixed count limit. I/O/allocation errors
propagate. Sessions are single-reader objects: host code must serialize request,
query, read, and negotiation calls and avoid concurrent direct reads of the streams.

The wire protocols have no transaction IDs. Matching cannot prove freshness or
protect against input deliberately imitating a reply. After a timeout, callers
must account for late replies before repeating the same request; raw requests
must not overlap a synchronous query of the same type. Cursor-position querying
is deliberately absent because its reply overlaps a legacy modified-F3 encoding.

## Capability probing

`terminal.probe(timeout)` sends a batch querying keyboard flags and private modes
25, 1049, 1000, 1002, 1003, 1006, 1004, 2004, and 2026. All observations share one
read deadline. It returns ten `ProbeObservation` values containing their typed
request, QueryResult, and EvidenceSource::Probe. Unsupported/no-response keyboard
queries cannot prevent other modes' replies from being collected. Queue pressure
and EOF stop waiting without losing ordinary input.

The returned observations are separate from `profile_report()` and never silently
rewrite environment-derived or caller-supplied assumptions, enable protocols, or
change display modes. A recognized mode reply confirms only that mode's reported
state. This API does not infer true color, Unicode width, richer mouse encodings,
or broad emulator compatibility from device attributes. An explicit caller profile
remains necessary to use optional features that the selected session profile lacks.

## Kitty keyboard events

```cpp
auto result = terminal.enable_keyboard(
    esc::KittyFlags::Disambiguate | esc::KittyFlags::EventTypes |
    esc::KittyFlags::AlternateKeys | esc::KittyFlags::AllKeys |
    esc::KittyFlags::AssociatedText,
    std::chrono::milliseconds{250});
// Only the Reply alternative confirms the requested flags.
```

`enable_keyboard` queries existing flags, writes `CSI = flags ; 1 u`, and queries
again to confirm the exact requested value. The entire operation shares a deadline.
It restores the previous flags if confirmation times out, input preservation fills,
EOF occurs, or the returned flags differ. A failed/partial mode write marks the
session unusable and reports primary/rollback I/O errors through SessionError;
close still tries to restore the captured flags. Flags must contain only the five
modeled bits and be nonempty; AssociatedText requires AllKeys.

This uses replacement of the current flags, rather than pushing/popping the
terminal's mode stack. It leaves the stack structure intact and restores the
captured current flags. Raw mode changes or screen-buffer switches outside the
session are the host's responsibility. While the session owns a keyboard change,
`set(ScreenBuffer)` rejects an effective screen change before I/O: disable keyboard,
change screen, then negotiate again. This respects independent keyboard state on
the main and alternate screens. Unchanged screen settings remain no-ops.

`disable_keyboard()` is idempotent and restores the flags captured by the first
successful change, even after renegotiation. If inherited flags are nonzero,
parsing remains Kitty after disable; otherwise it returns to legacy. Close and
the destructor restore keyboard flags before restoring screen modes and OS state.
No support claim is inferred merely from TERM or the operating system.

Both legacy and Kitty input produce `KeyEvent`. When Kitty is selected, CSI u and
enhanced navigation/function-key forms populate additional fields:

- optional `key`: a Unicode scalar, named KeyCode, or KittyKey for an unknown
  reserved functional identifier; a text-only IME event has no key;
- `action`: Press, Repeat, or Release;
- modifiers with separate shift/ctrl/alt/super/hyper/meta/caps_lock/num_lock bits;
- optional shifted and base-layout scalars;
- owned UTF-32 `text`, distinct from key identity.

Named mappings cover the protocol's lock, F1..F35, keypad, media, volume, modifier,
and ISO shift keys. Unknown reserved identifiers do not become printable text.
Associated text rejects Unicode errors and C0/C1 control codes. Malformed fields,
overflows, and invalid actions/modifiers remain owned UnknownInput units. Ordinary
UTF-8 text and compatible legacy encodings remain readable; this does not invent
release events for legacy input. Textless enhanced events allocate nothing after
parser construction. Text allocation is bounded by the configured sequence limit.

Standalone `Parser`/`Reader` users opt in through InputProtocols.replies and
InputProtocols.keyboard. Configuration changes affect new sequences, preserving
an already-started unit's protocol snapshot. Parser reset retains configuration.
Applications with exhaustive Event visitors handle TerminalReply and one KeyEvent
alternative. `matches` checks key, modifiers, and action; its default action is Press.

## Windows transport

The first explicit request prepares a bounded console stream transport, enables
VT input, and selects UTF-8 input while preserving captured OS settings. One
persistent private worker performs synchronous ReadFile into a 4096-byte buffer;
the calling thread waits on a completion event with its deadline. A pending read
survives caller timeouts so future input remains available. The worker performs
no parsing, environment access, application callbacks, or queue growth. Native
records already normalized before the switch are drained before stream input.
An incomplete native UTF-16 pair rejects transport preparation without changing
modes; callers drain its remaining input before retrying. Native line storage
retained from a prior canonical read is preserved as pending bytes.

Close cancels and joins the worker before restoring modes/code pages or releasing
session ownership. The cancellation loop also handles cancellation preceding
entry into ReadFile. Failed transport setup rolls back and stops the prepared
worker. Canonical/echo input cannot be enabled while this transport is active;
close restores the originally captured settings. Merely constructing a session
creates no worker. Supported input modes and protocol acknowledgments still depend
on the actual Windows console/ConPTY host; Windows itself does not imply Kitty
support. Pipe input remains byte-oriented without a worker.

`termcaps --probe` and `termcaps --kitty` exercise the opt-in paths. Without these
arguments, the tool's behavior remains unchanged.

## Protocol basis and verification

The contracts follow the [Kitty keyboard specification](https://sw.kovidgoyal.net/kitty/keyboard-protocol/),
[xterm DA/DECRQM replies](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html),
[Windows VT input documentation](https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences),
and [CancelSynchronousIo](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-cancelsynchronousio).
Tests use independently authored literal requests/replies and semantic events.
Synthetic PTYs verify negotiation/order/restoration; they do not verify an emulator.
Native Windows runtime and live terminal results remain pending until executed.

On macOS, the release suite passes all 15 CTest groups and the ASan/UBSan suite
passes all 13 non-package groups. This includes 222 behavior cases, independent
headers, installed static/shared and subproject consumers, PTY examples, and
benchmark smoke. Linux and Windows libraries, examples, portable/platform tests,
and benchmarks cross-compile and link; cross-builds do not execute their tests.
The Windows suite includes 40 native platform cases, five added for the VT
transport. No native Windows/MSVC or live Kitty emulator pass is claimed.
