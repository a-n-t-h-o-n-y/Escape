# Proposed-interface behavior tests

These tests specify [the interface proposal](../../docs/interface-proposal.md).
They target the agreed interface rather than legacy behavior. The encoding, capability, and
text/glyph slices now compile and pass. Remaining missing
headers/types/functions and the test-only I/O adapter still prevent the full suite
from compiling/linking until the refactor reaches those areas. There are no skipped
assertions, fake passing implementations, or TODO test bodies masking that gap.

The new suite is independent of zzz's test framework. It lives beside the legacy
glyph tests and is opt-in so ordinary library builds remain usable during
implementation. The unchanged legacy target now fails to compile because it tests
removed char8_t/char16_t scalar pipes and operators on arbitrary containers. Its
remaining assumptions about mutating range pipes also differ from the new
contract. Remove it when its useful coverage has moved to this suite.

## Running the implemented encoding slice

```sh
cmake -S . -B build-contract -DESCAPE_BUILD_CONTRACT_TESTS=ON
cmake --build build-contract --target escape.tests.contract.encoding
ctest --test-dir build-contract -R '^escape\.contract\.encoding\.' --output-on-failure
```

This target runs 30 tests shared with the full suite: raw command encoding,
color/trait/geometry values, destination guarantees, and allocation checks.
Capability-aware encoding remains in the full suite. Tests that also need the
future capability resolver or input values were moved to their respective source
files with their original assertions intact.

## Running the implemented text and glyph slice

```sh
cmake --build build-contract --target escape.tests.contract.text
ctest --test-dir build-contract -R '^escape\.contract\.text\.' --output-on-failure
```

This target runs 34 Unicode, styling, concatenation, aliasing, and allocation tests.
Original text/glyph allocation expectations were moved to text-allocation.test.cpp
so they can run independently of future capability and parser APIs.

## Running the full suite after the corresponding API exists

```sh
cmake -S . -B build-contract -DESCAPE_BUILD_CONTRACT_TESTS=ON
cmake --build build-contract --target escape.tests.contract escape.tests.contract.posix
ctest --test-dir build-contract -R '^escape\.contract\.' --output-on-failure
```

The POSIX executable applies to macOS/Linux. Core allocator support also currently
uses POSIX aligned allocation; port that test helper when testing other platforms.
No test sends mode changes to the developer's own terminal. Live-emulator checks
are a separate [acceptance matrix](live-terminals.md), initially untested.

Run one area while implementing it:

```sh
build-contract/tests/contract/escape.tests.contract ENC_
build-contract/tests/contract/escape.tests.contract PAR_
build-contract/tests/contract/escape.tests.contract.posix SES_
```

All executable test sources are in the targets above; remaining unimplemented
API references are not silently filtered out. A temporary per-slice target may
select a source while implementing that slice. The runner returns a failing exit
code if no test matches a filter, so a misspelled filter cannot look like a pass.
CTest limits runtime to catch non-progress loops and deadline-reset bugs.

## Coverage map

| Proposal group | Automated coverage |
| --- | --- |
| ENC-01..04, BUF-01..03 | sequence.test.cpp, values.test.cpp, encoding-allocation.test.cpp, allocation.test.cpp |
| CAP-01..03 | capabilities.test.cpp, session.test.cpp, failures.test.cpp |
| TXT-01..02 | text.test.cpp, text-allocation.test.cpp |
| GLY-01, COL-01 | text.test.cpp, values.test.cpp, text-allocation.test.cpp |
| OUT-01..02 | output.test.cpp, failures.test.cpp |
| PAR-01..05 | parser.test.cpp, allocation.test.cpp |
| IO-01 | reader.test.cpp, failures.test.cpp |
| SES-01..05 | session.test.cpp, failures.test.cpp |
| LIVE-01 | Manual live-terminals.md; no emulator results claimed |
| PKG-01 | verify_headers.py and verify_package.py through CTest |

Sequence oracles are literal bytes or the protocol's independent indexed-color
formula. Expected input events are hand-authored semantic values. Parser fixtures
are **synthetic**, not captures from validated emulators. Fragmentation helpers
test every split and byte-at-a-time delivery. Paste tests compare payload and
first/last/aborted flags rather than freezing read-dependent chunk boundaries.

The suite is substantial but not a proof of every possible byte stream or all
terminal versions. In particular, it does not allocate multi-gigabyte text merely
to cross INT32_MAX; that large-input property needs a separately bounded stress
test or a reviewed decoder using size_t. Color variants with only nonthrowing
stored alternatives cannot reliably be forced valueless by a portable unit test.

## Test-only I/O fault seam

`support/faults.hpp` declares the seam needed by failures.test.cpp. It has no
implementation yet; supplying a no-op implementation would invalidate these tests.
The future backend test adapter must:

- Route library read/poll/write/flush, termios capture/apply, size query, and
  environment sampling through controllable operations when
  ESCAPE_CONTRACT_TEST_HOOKS is enabled.
- Count operations starting at FaultScope construction. Do not count test
  fixture syscalls, which intentionally bypass the library adapter.
- Fail on the selected call with the specified error. A persistent fault repeats
  from that call onward. Nonpersistent faults fire once and permit cleanup.
- Limit each transfer to maximum_transfer when provided, independent of errors.
- For Write.after_bytes, accept that many total bytes, split the crossing transfer
  if necessary, then fail once. Subsequent cleanup writes must be permitted unless
  persistent was requested. This keeps failure tests independent of batching.
- Apply an optional delay before a fault, without restarting the caller deadline.
- Restore the previous adapter on scope exit. Keep hooks out of installed headers
  and normal production builds.

This seam is a testing requirement, not a public virtual backend or a prescribed
production architecture. Definitions can live in an internal test adapter; CMake
enables the private build flag and support include path for the opt-in suite.

## Allocation checks and verification status

AllocationScope observes/rejects C++ new/new[] (including aligned/nothrow forms)
on the current thread. Construct fixtures and owned expected values before the
scope, run the operation, then assert after leaving it. It does not instrument
libc malloc or other threads and is not a throughput benchmark. Parser storage
may allocate at setup; owned output strings and paste/unknown events may allocate.

The standalone runner, allocator instrumentation, and POSIX fixtures passed four
independent smoke tests on this laptop with Apple Clang 15. Such validation does
not establish that Escape passes these tests.

The encoding target passes 30 tests in release and debug builds, including a run
with address and undefined-behavior sanitizers. All 256 indexed colors and all 512
modeled trait subsets are checked. Its six public headers compile independently,
and constexpr value checks pass.

The text/glyph target passes 34 tests in release and with address and
undefined-behavior sanitizers. Reserved conversion/glyph appends and iterator
output have no observed C++ allocations; concatenation allocates only its result.
The text, glyph, and brush headers compile independently without zzz, ICU, or
platform includes. termcaps and Pong build. ICU is no longer a build dependency.

A separate, temporary verification harness compared all 1,112,064 Unicode scalars
and 75,536 short valid/malformed UTF-8 inputs against Python's strict codec, including
error offsets. Those independent checks passed; that harness is not part of CTest.
The legacy glyph target is intentionally incompatible with this interface and no
longer compiles; it is retained unchanged for migration review.

The complete suite still requires future capability, text, parser, and session
APIs and the test-only backend adapter. Compile and run each slice as it becomes
available; report failures rather than changing expectations to preserve old
behavior.

## Small contract clarifications

These tests interpret the proposal's aliasing and end-of-input rules as follows:

- GlyphString += GlyphRange accepts borrowed ranges and handles overlap before
  reallocation. This makes the proposal's borrowed-range aliasing requirement
  testable without adding operators to arbitrary destination containers.
- EOF resolves a lone ESC as the Escape key, just as its ambiguity timeout does.
- APIs consuming invalid Glyph scalars use UnicodeError and validate before
  in-place mutation.

These clarifications are reflected in the proposal. They do not claim to
describe the current API.


## Running the implemented capability slice

Build `escape.tests.contract.capabilities` with the opt-in configuration above,
then run `ctest -R '^escape\.contract\.capabilities\.' --output-on-failure`.
The slice currently contains 29 tests; together with encoding and text/glyph,
93 implemented contract tests pass in release and ASan/UBSan builds.
This covers profile-aware encoding, color/trait fallback, mouse negotiation,
pure environment resolution, owned environment snapshots, validation,
transactional writes, and allocation guarantees. Session integration remains
part of the future POSIX contract target.
