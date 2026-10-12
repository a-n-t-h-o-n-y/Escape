# Encoding, text, and input performance review

Reviewed on 2026-10-12 against `4ccc7b4` on `refactor/terminal-api`. Public
signatures, canonical bytes, strict validation, capability rules, ownership, and
allocation guarantees are unchanged. No terminfo or runtime dependency was added.

## Changes

- Numeric encoding counts decimal digits during measurement and formats directly
  into the destination during writing. This removes temporary formatting and
  copying from the size pass while retaining checked sizes and exact spans.
- UTF-32 conversion and output encode previously validated scalars directly into
  their destination or bounded output chunk. The public single-scalar encoder
  shares that serializer and still validates input and zeros unused bytes.
- Zero-timeout Reader calls use an explicit polling path. Buffered events avoid
  clock reads; parser expiry, finite caller deadlines, EOF, and interrupted reads
  retain their behavior.
- Indexed256 approximation checks the supplied sixteen system colors and exactly
  one candidate from each fixed palette group. Squared RGB distance separates
  across the cube's axes, so nearest channel values identify the nearest cube
  entry. The grayscale optimum is nearest the mean channel value. Integer
  rounding and candidate order preserve the lowest-index tie rule, including
  ties against customized system colors. No color cache or approximation change
  is involved.
- Printable ASCII scanning tests eight bytes at a time using unsigned word
  arithmetic and bounded memcpy loads. It requires no alignment or platform
  intrinsics. Nonprintable/non-ASCII bytes return to strict scalar processing.
  OSC payloads validate during measurement rather than repeating validation
  during the write pass; aliased appends still snapshot before growth.
- Unicode property lookup uses a compile-time index of 256-scalar pages, adding
  about 8.5 KiB of immutable data to narrow searches of the pinned intervals.
  ASCII width runs share the word scanner. Grapheme iterators select interior
  ASCII clusters directly and preserve Unicode boundary lookahead.

A more complicated grapheme iterator with cached ASCII runs was also measured.
Its small gain did not justify extra state and code, so the simpler shortcut was
retained. Byte-output buffering and paste parsing were already efficient for the
measured workloads; their implementations were retained. No extra public buffer,
stateful encoder, lazy cache, or platform-specific SIMD layer was introduced.

## Verification

- macOS Apple Clang 15 Release: all 15 CTest groups pass, covering 272 behavior
  cases, independent public headers, installed static/shared and subproject
  consumers, PTY examples, and benchmark preflights.
- macOS ASan/UBSan Debug: all 13 non-package groups pass.
- New cases compare palette selection with an exhaustive distance oracle under
  Basic8, Basic16, and Indexed256, with conventional and custom system palettes.
  They exercise all combinations around cube midpoint boundaries, every gray
  value, and off-gray inputs.
- Widths for every valid Unicode scalar agree with a linear walk of the pinned
  property intervals. All 766 official grapheme vectors still pass in UTF-8 and
  UTF-32. ASCII/OSC cases check every byte value at sixteen word offsets, invalid
  Unicode offsets, controls, and combining/keycap suffixes.
- Zero-timeout reader tests check buffered keys, expired Escape/CSI units, and
  poll/read interruptions without losing input. Existing finite deadline,
  fragmentation, EOF, allocation, and failure cases pass.
- A separate temporary harness compared conversion of all 1,112,064 valid scalars
  with Python's strict UTF-8 codec and checked the Escape round trip. It passed;
  this harness is not part of CTest.
- Linux and Windows libraries, examples, portable/platform tests, and benchmarks
  cross-compile and link with Zig 0.15.2 / Clang 20.1.2. Linux needed
  `CMAKE_LINK_DEPENDS_USE_LINKER=FALSE` for Zig's linker. These builds do not
  execute native tests; native Linux, Windows/MSVC, and emulator checks remain
  separate pending evidence.

## Measurements

The comparison uses macOS 13.7.8/x86_64, Apple Clang 15, static Release
(`-O3 -DNDEBUG`), tests/examples disabled, seven samples targeting 100 ms each.
Before is the committed `4ccc7b4` source with the expanded benchmark runner;
after includes this review's changes. Both use identical benchmark workloads.
Three alternating complete before/after pairs run sequentially after builds and
verification finish. No compilation runs alongside these measurements. CPU
frequency, OS scheduling, and unrelated background activity remain uncontrolled.

The new cases cover reserved mixed UTF-32 conversion, a 4 KiB title, and fixed
and varied RGB-to-Indexed256 conversion. Existing cases retain their workloads;
units and correctness preflights are described in the
[benchmark README](../benchmarks/README.md). These are library/fixture timings,
not emulator latency or portable performance guarantees. No timing threshold is
enforced.

The table uses the median of the three run medians (ns per documented unit).

| Workload | Before ns/unit | After ns/unit | Before / after |
| --- | ---: | ---: | ---: |
| `text/reserved-utf32-to-utf8` | 14,328.03 | 5,608.72 | 2.55× |
| `encode/title-4KiB` | 18,950.49 | 502.79 | 37.69× |
| `layout/width/ascii` | 256.38 | 82.42 | 3.11× |
| `layout/graphemes/ascii` | 939.63 | 520.99 | 1.80× |
| `layout/width/mixed` | 322.16 | 290.27 | 1.11× |
| `layout/graphemes/mixed` | 331.78 | 257.92 | 1.29× |
| `encode/string` | 212.62 | 174.36 | 1.22× |
| `encode/reserved-append` | 148.74 | 114.65 | 1.30× |
| `encode/span` | 131.90 | 102.42 | 1.29× |
| `encode/profile-truecolor` | 150.52 | 118.83 | 1.27× |
| `encode/profile-256-color` | 1,140.54 | 64.56 | 17.67× |
| `encode/profile-256-varied` | 1,149.87 | 65.43 | 17.57× |
| `encode/profile-16-color` | 72.98 | 61.06 | 1.20× |
| `parse/mixed-whole` | 75.71 | 76.75 | 0.99× |
| `parse/mixed-byte-fragments` | 109.79 | 105.24 | 1.04× |
| `parse/paste-4KiB-fragments` | 2,181.77 | 2,147.58 | 1.02× |
| `reader/ready-file-ASCII` | 67.12 | 23.56 | 2.85× |
| `output/buffered-32B` | 94.84 | 95.28 | 1.00× |
| `output/unbuffered-32B` | 815.55 | 815.85 | 1.00× |
| `output/buffered-4KiB` | 517.53 | 515.36 | 1.00× |

Mixed parsing, paste, and raw byte-output results remain close to their baseline;
small changes on those unchanged paths are not attributed to an optimization.
The largest gains are specific to palette approximation and long ASCII OSC
payloads. Whole-application speed still depends on its workload and terminal.

[Before pair 1](../benchmarks/results/macos-clang15-speed-before-1.txt), [after pair 1](../benchmarks/results/macos-clang15-speed-after-1.txt).
[Before pair 2](../benchmarks/results/macos-clang15-speed-before-2.txt), [after pair 2](../benchmarks/results/macos-clang15-speed-after-2.txt).
[Before pair 3](../benchmarks/results/macos-clang15-speed-before-3.txt), [after pair 3](../benchmarks/results/macos-clang15-speed-after-3.txt).
