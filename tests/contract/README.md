# Testing Escape

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Tests default on in a top-level build and off for subproject consumers.
`ESCAPE_BUILD_CONTRACT_TESTS=ON` also enables them. Python 3 adds header, package,
documentation, and PTY checks; the C++ suites do not require it.

Run a group directly by its case-name prefix:

```sh
build/tests/contract/escape.tests.contract PAR_
build/tests/contract/escape.tests.contract.posix SES_
```

A filter matching no tests fails. See [API behavior](../../docs/behavior.md)
for the guarantees the tests exercise.

## What is checked

- Encoders: literal protocol bytes, validation, overlap, short spans, and allocation.
- Text: strict UTF-8/UTF-32, styled scalars, Unicode 17 grapheme vectors, and width policies.
- Profiles: environment resolution, fallback, strict rejection, and color approximation.
- Parser: fragmented input, modifiers, malformed units, paste boundaries, and EOF.
- I/O and sessions: deadlines, partial writes, mode ownership, rollback, and restoration.
- Extensions: query replies, keyboard negotiation, synchronized frames, OSC, and sound bytes.
- Packaging: every public header compiles alone; installed and subproject consumers link and run.
- Examples: tutorials, Pong, Lumen, termcaps, and the event loop run against separate PTYs.

POSIX fixtures use files, pipes, and PTYs. Windows fixtures allocate their own
console and inject native records. Neither uses the developer's active terminal.
A PTY checks program behavior and emitted bytes; actual rendering and sound
still need a terminal emulator. Use [termcaps](../../docs/termcaps.md)
to try those features interactively.

## Faults and allocations

`support/faults.hpp` injects backend read/write/poll, size, environment, and
attribute failures. It is enabled only in test builds and is not installed.
Fixtures check short transfers, interrupted waits, partial transitions, and
cleanup after failure.

`AllocationScope` counts C++ allocations on the current thread, including
aligned forms. It does not count libc allocations or allocations on other threads.
Benchmarks measure throughput separately; see the [runner](../../benchmarks/README.md).

CI runs native Linux, macOS, and Windows builds, plus Linux sanitizers. A
successful cross-compilation alone does not establish runtime behavior.
