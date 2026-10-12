# Performance benchmarks

Opt-in, dependency-free benchmarks for the current interface. They do not
change the library API or replace terminal validation.

```sh
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release \
  -DESCAPE_BUILD_BENCHMARKS=ON -DESCAPE_BUILD_TESTS=OFF \
  -DESCAPE_BUILD_CONTRACT_TESTS=OFF -DESCAPE_BUILD_EXAMPLES=OFF
cmake --build build-bench --config Release --target escape.benchmarks
./build-bench/benchmarks/escape.benchmarks
```

For Visual Studio builds, run
`build-bench/benchmarks/Release/escape.benchmarks.exe`. No performance thresholds
are enforced. When both benchmarks and tests are enabled, CTest runs a short
fixture/runner smoke check; CI enables this on all three platforms.
Performance runs disable tests to exclude the private fault-injection adapter.

Use `--milliseconds 100 --samples 7` to control each case's sampling, or
`--filter encode/` to select cases. `--help` lists options. Invalid options and
filters selecting no cases return failure. The output includes CSV rows after
its metadata, followed by an observed checksum.

## Workloads and measurement

Fixtures, reserved buffers, profiles, and output streams are prepared before
timing. Preflight checks compare encoding against literal bytes, mixed parser
input against authored events, and paste against the original payload and
boundary flags. The reader fixture verifies an ASCII key. Warm-up and adaptive
calibration are separate from samples. Each case reports median and min/max
nanoseconds per unit and byte throughput, using steady_clock.

- String return, reserved append, span, and true-color profile cases encode the
  **same 35-byte frame**: cursor position, RGB foreground, bold, and row erase.
  Reserved append clears and reuses its capacity; the span reuses 128 stack bytes.
  The sixteen-color case encodes **one RGB foreground command**, including
  palette approximation; compare it only against equivalent color workloads.
- Mixed parsing measures ASCII, Unicode, navigation/modifiers, SGR mouse, and
  focus as a whole stream or one byte at a time. A unit is one of seven events;
  reset happens once per stream. Paste is a 4 KiB payload in 64-byte fragments;
  a unit is the complete paste. Parser construction and fixture collection are
  excluded from timing; owned paste chunks remain part of the timed workload.
- Text layout measures a 256-byte ASCII string and mixed accented Latin, CJK,
  emoji ZWJ, flag, and Indic text. Width and borrowed grapheme iteration are
  separate cases; a unit is a whole string. Data conversion is outside timing.
  Use `--filter layout/` to run these workloads.
- Reserved UTF-32-to-UTF-8 conversion measures 1,664 mixed scalars producing
  3,072 bytes. The destination capacity is reused; validation remains timed.
- OSC title encoding measures a 4 KiB printable ASCII payload with reserved
  output. The 256-color cases measure RGB approximation for one fixed color
  and eight varied colors spanning the system palette, cube, and grayscale.
  A varied-case unit is one color, with all eight encoded per iteration.
- Reader measures ASCII events from a cached 1 MiB temporary file with read-ahead
  and zero-timeout calls. One unit is an event. Construction and rewind happen
  once per batch (and when wrapping the file), so their cost is amortized.
  This measures ready file delivery, not terminal latency or cold disk speed.
- Output measures 32-byte and 4 KiB writes to `/dev/null` or Windows `NUL`.
  One unit is a write. Buffered cases use an explicit host-owned 8 KiB stdio
  buffer; the unbuffered case uses none. The final batch flush is timed.
  These measure library/stdio/kernel overhead, not terminal rendering.

Output buffers are observed with a compiler barrier (MSVC uses its intrinsic
and a volatile byte read), and event counts feed a checksum. No allocation,
clock, or terminal probe instrumentation is added to the library. Compare runs
with the same compiler, flags, machine, workload, and sampling settings; compiler
barrier overhead differs between toolchains. This runner does not isolate CPU
frequency, scheduling, or background activity.

## Initial local baseline

Recorded on 2026-10-11, macOS 13.7.8/x86_64, Apple Clang 15.0.0,
CMake Release (`-O3 -DNDEBUG`), static library, tests/examples disabled,
seven samples targeting 100 ms each, before the later layout and encoding optimizations.
The [raw results](results/macos-clang15.txt) include batch sizes and ranges.
This is one laptop run, not a portable performance guarantee.

The standalone target also cross-compiles and links for Linux and Windows;
benchmark execution on those platforms and MSVC remains pending.

| Case | Median ns/unit |
| --- | ---: |
| encode/string | 212.50 |
| encode/reserved-append | 147.36 |
| encode/span | 134.75 |
| encode/profile-truecolor | 153.70 |
| encode/profile-16-color | 72.33 |
| parse/mixed-whole | 78.70 |
| parse/mixed-byte-fragments | 113.47 |
| parse/paste-4KiB-fragments | 34157.21 |
| reader/ready-file-ASCII | 67.72 |
| output/buffered-32B | 95.03 |
| output/unbuffered-32B | 808.65 |
| output/buffered-4KiB | 511.23 |

For the shared frame, reserved append took about 31% less time than string
return and span encoding about 37% less time. Buffered 32-byte writes took about
8.5 times less time at this null sink. These results support using the existing
reusable-buffer APIs and batching output; they do not justify changing the
ordinary string-return interface by themselves.

## Text layout local baseline

Unicode 17 layout, macOS 13.7.8 x86_64, Apple Clang 15, Release, tests disabled.
Recorded 2026-10-11 using `--filter layout/ --milliseconds 100 --samples 7`.
These are per complete string, without allocation, and not emulator timings.
The mixed input is 34 UTF-8 bytes / five clusters / eight estimated cells.

| Workload | Median ns/string | Min–max ns/string | MiB/s |
| --- | ---: | ---: | ---: |
| Width, 256-byte ASCII | 256.23 | 252.62–312.44 | 952.81 |
| Graphemes, 256-byte ASCII | 932.32 | 927.74–950.13 | 261.86 |
| Width, mixed text | 332.07 | 324.26–336.33 | 97.65 |
| Graphemes, mixed text | 330.23 | 327.08–334.19 | 98.19 |

Printable ASCII uses a measured optimization that preserves the last scalar
for possible combining/emoji lookahead. These figures are a baseline for future
changes; scheduling, CPU frequency, and background activity remain uncontrolled.
