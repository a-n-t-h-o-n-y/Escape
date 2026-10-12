# Implementation review and measured optimization

Reviewed on 2026-10-11 against commit `434a8ef` and the interface contract.

## Scope and findings

Reviewed command serialization and capability adaptation, Unicode/glyph
validation and ownership, incremental parsing and deadlines, stdio output,
POSIX/Windows input, session ownership, transition rollback, cleanup, and the
public build/package interface. This is a source review and automated check,
not proof of emulator compatibility or an exhaustive verification of all inputs.

Two Windows correctness defects were fixed:

- Canonical echo opened `CONOUT$` even when the session supplied a different
  output buffer, and failed to close that new handle for session readers. Echo
  now borrows the session output handle. Standalone readers close the handle
  they open. Checked partial writes are completed, and zero progress fails.
  A native regression verifies the selected output buffer and stable handle
  count across repeated canonical echo.
- Native F21–F24 records were serialized into identifiers the parser rejected.
  Those identifiers now use a private parser grammar enabled by Reader only for
  native Windows console input. Portable Parser and redirected input do not
  acquire an undocumented terminal protocol. Windows coverage exercises all
  24 function keys with modifiers; portable fixtures retain rejection of the
  private identifiers.

No public signature or return type was changed. The string return remains the
convenient owning API; reserved append and spans retain their allocation contracts.

## Optimization decision

The original benchmark identified 4 KiB paste parsing as avoidable per-byte work.
Paste parsing now copies ordinary byte runs with bounded bulk appends. Potential
terminators still use the incremental byte path; trailing delimiter prefixes,
false delimiters, embedded NUL, chunk limits, EOF, and subsequent input retain
their behavior. Fragments shorter than eight bytes use the original byte path.
Storage remains bounded and owned paste events may still allocate.

Added independent payload/flag checks over five chunk sizes and seven fragment
sizes, including input after the closing delimiter. Existing every-split,
byte-at-a-time, malformed, EOF, and allocation checks remain in place.

Encoding and output timings supported their existing reuse/buffering APIs;
there was no evidence here for a public return-type change or extra hidden
buffering. Palette caching and broader parser rewrites were not introduced.

## Verification

- macOS Apple Clang 15 Release: all 15 CTest groups pass, including 198
  behavior cases, independent headers, installed static/shared consumers,
  subproject consumers, example PTYs, and benchmark smoke.
- macOS ASan/UBSan Debug: all 13 non-package CTest groups pass.
- Linux and Windows: library, examples, portable/platform contracts and
  benchmarks cross-compile and link with Zig 0.15.2 / Clang 20.1.2. The Windows
  suite now has 35 cases. Cross-builds do not execute those tests; native Linux,
  Windows/MSVC, and real terminal acceptance remain outstanding.

## Before/after measurements

Same laptop and runner as the initial baseline: macOS 13.7.8, x86_64, Apple Clang
15, static Release (`-O3 -DNDEBUG`), tests/examples off, seven samples targeting
100 ms each. Before source matches committed `434a8ef`; after source includes
this review's fixes and bulk paste change. Builds and tests finished before these
runs. The runner does not control CPU frequency or other system activity.

| Workload | Before median | After median |
| --- | ---: | ---: |
| 4 KiB paste, 64-byte fragments | 34,016 ns | 2,148 ns |

That is about 15.8 times faster, or 93.7% less elapsed time, for this synthetic
workload. Three additional alternating pairs measured before medians of
34,768 / 35,047 / 34,154 ns and after medians of 2,282 / 2,151 / 2,142 ns,
confirming improvements between 15.2 and 16.3 times. This is not a terminal
latency claim or a portable performance guarantee.

[Full before run](../benchmarks/results/macos-clang15-review-before.txt),
[full after run](../benchmarks/results/macos-clang15-review-after.txt), and
[repeated paste pairs](../benchmarks/results/macos-clang15-review-paste-pairs.txt)
retain all ranges and batch sizes. Other full-run timings varied, including
unchanged encoding paths; those variations are not attributed to this change.
No benchmark threshold is enforced.
