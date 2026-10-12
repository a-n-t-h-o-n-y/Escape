# Interface behavior tests

These tests specify [the interface contract](../../docs/interface-proposal.md),
including incorrect-input and failure paths. Core tests are shared; platform
suites cover POSIX PTYs and native Windows consoles/files/pipes. The obsolete glyph test target was removed after its useful
coverage moved into the new behavior suite.

## Running

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Build tests default on for a top-level project and off for subproject consumers.
`ESCAPE_BUILD_CONTRACT_TESTS=ON` still enables them explicitly. The standalone
runner fails when a filter matches no tests:

```sh
build/tests/contract/escape.tests.contract PAR_
build/tests/contract/escape.tests.contract.posix SES_
```

The optional `escape.tests.contract.encoding`, `escape.tests.contract.text`,
and `escape.tests.contract.capabilities` targets select sources from the full
suite. Their cases are registered with CTest through the full targets, without
duplicate slice runs. POSIX tests use files, pipes, and PTYs; none changes the
developer's active terminal. Windows tests detach their process from any existing
console and allocate a fresh console for synthetic native-record injection; this
happens only in the test fixture, never in the library. They check UTF-16 input,
AltGr, key repeats, deadlines, canonical editing, viewport resizing, mouse/focus,
stream borrowing, ownership, mode/code-page/CRT restoration, and injected failures.

## Coverage and evidence

| Contract | Coverage |
| --- | --- |
| ENC / BUF / COL | sequence, values, encoding-allocation, allocation, capability-allocation |
| CAP | capabilities, session, failures |
| TXT / GLY | text, text-allocation |
| PAR | parser, allocation |
| OUT | output, failures |
| IO | reader, failures |
| SES | session, failures |
| WIN | windows.test.cpp (35 native platform cases) |
| PKG | verify_headers.py, verify_package.py |
| LIVE | Manual [acceptance matrix](live-terminals.md), still untested |

Sequence expectations are literal bytes or an independent protocol formula.
Input event expectations are authored semantic values, with every split and
byte-at-a-time delivery. Deterministic mixed-stream cases use independent event
goldens. Paste tests check payload, bounds, and first/last/aborted flags without
assuming arbitrary read boundaries. Fixtures are synthetic, not emulator captures.

The suite checks pure resolution, owned environment snapshots, exact profile
matching, fallback, strict rejection, validation before writes, borrowed storage,
EOF, absolute deadlines under interruption and trickling input, inherited input
protocols, device ownership, moves, rollback, and cleanup after failure.
Canonical restoration is checked with queued bytes still present. On macOS a
checked FIONREAD query completes the kernel's canonical reprocessing without
consuming or flushing input; signal dispositions remain the host's responsibility.

The test-only `support/faults.hpp` adapter controls actual library backend
read/poll/write/flush, platform attributes, size, and environment operations. It counts only
library calls, handles short transfers and partial writes, injects selected
errors/delays, and restores a preceding scope. It is not installed or used in
production builds configured with tests off.

AllocationScope counts C++ new/new[], including aligned forms, on the current
thread. It does not instrument libc malloc, other threads, or throughput.
Ordinary fragmented Unicode, named keys, focus, and mouse parsing allocate
nothing after construction. Reserved appends and bounded encoding retain their
allocation contracts. Owned paste/unknown events may allocate.

## Verification status

The macOS suite contains 198 behavior tests, plus independent header/package
checks and PTY smoke tests for Pong and termcaps. It passes locally with Apple
Clang 15 in release and ASan/UBSan builds after the backend split. Public headers
stand alone without platform headers; Windows tests also include windows.h
before esc.hpp and preserve the host RGB macro. Subproject and installed static
and shared consumers compile, link, and run through Escape::escape on macOS.

The library, both examples, portable contracts, and platform contract executables
also cross-compile and link for x86_64 Linux and Windows with Zig 0.15.2's Clang
20.1.2 toolchain. Cross-compilation does not execute behavior tests. The 35 native
Windows cases are compiled but have not run on this laptop. Native Linux,
Windows/MSVC, and real emulator results are pending. GitHub Actions is configured
for ubuntu-latest, macos-latest, windows-latest/MSVC, and Linux sanitizers; no
hosted passing run is claimed. Package checks are separate from behavior tests.

An earlier independent Unicode harness compared all 1,112,064 Unicode scalars
and 75,536 short valid/malformed UTF-8 inputs against Python's strict codec.
Those checks passed; the temporary harness is not part of CTest. No claim is
made about multi-gigabyte stress inputs or all possible terminal byte streams.
