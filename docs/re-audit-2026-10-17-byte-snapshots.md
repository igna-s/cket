# Classical byte snapshot audit, 2026-10-17

`memcmp` and `strncmp` now accept current bytes of known, fixed-size CPU objects in comparisons with
checked quantum integer storage. Each selected CPU byte is captured once. Saved inverse cleanup
reuses the original parameters after CPU mutation; no quantum placeholder is read on the CPU and no
implicit quantum observation is introduced. Ordinary classical calls continue to use libc.
Cybersecurity operations remain classical and outside the quantum adaptation scope.

## Reproduced NUL-tail bug and correction

The first prototype eagerly read the complete CPU range, including bytes after its first NUL. A
source with the NUL at the last byte before a protected page returned zero as ordinary C but
received SIGSEGV under the candidate. An inline conditional fixed the logical values and passed 630
finite checks, yet GCC speculated the suffix load and the separate page probe still failed. Neither
exploratory run is counted as a successful complete campaign.

A private `noipa` CPU helper now guards the actual load. A disabled address is not dereferenced;
`strncmp` passes disabled queries after its CPU operand's own NUL. The
[preserved failure capsule](audit-evidence/byte-snapshots/exploratory-failures.tar.gz) includes both
failing sources, original results and post-failure source capsules. Those historical hashes were
captured after reproduction, not frozen before passing builds. The current guards additionally test
disabled null/protected addresses, all five optimization levels and both builtin modes.

## Exact sources and completed verification

The [integration record](audit-evidence/byte-snapshots/integration-summary.json) identifies 129
inputs frozen before normal and ASan/UBSan verification in the isolated checkout. The normal run was
interrupted during byte snapshots. Its completed prefix is retained, and the complete snapshot suite
plus all remaining normal suites and the fresh sanitizer pipeline resumed on matching sources. The
original prefix, resumed tail, process metadata and terminal result remain separately archived; this
is a segmented campaign, not a single uninterrupted invocation. Its
[full transcript](audit-evidence/byte-snapshots/verify.log) repeats the established native, example,
frontend, arithmetic, history, binding, locale, memory and library-boundary suites in both
configurations. This includes 391 native checks, 1452 example checks, 665 generated frontend
compilations, 302 `memcmp` checks, 246 `strncmp` checks, 650 byte-snapshot checks and 63 QuEST
circuits per configuration. All other finite suite counts are listed in the
[machine-readable campaign report](audit-evidence/byte-snapshots/results.json).

The snapshot matrix separates 20 CPU guards, 250 native complete-state cases, 140 native programs,
180 rejected boundary cases and 60 template/export contracts. Tracing checks actual CPU binding
values, exactly one query per selected byte, enabled reads only through the CPU NUL, full complex
amplitudes, input preservation, phases, private zeros and saved inverses after CPU mutation. All 256
byte values and selected complete 65536-value word domains are checked; word states use `-O2`, not
all optimization profiles. Four new QuEST states independently execute bound QASM with CPU mutation
and inverse cleanup. The complete QuEST suite has 57 states and six sampled channels; explicit
binding fixtures are separate from validation of real native captures.

Fourteen compiler/test/script inputs differ from the preceding 127-input string state. The exact
delta was applied only after the full isolated campaign passed and the main base hashes matched. All
129 resulting main input hashes match the candidate. Separate fresh main normal and instrumented
plugin/driver/backend/engine/runtime builds each pass the 650-check validator and the exact contract
example. The broad regression remains identified as an isolated execution; main smoke logs are
recorded separately. Earlier campaigns and their capsules remain intact.

## Remaining scope

The [snapshot contract](c11-byte-snapshots.md) requires known storage, constant offsets/count and
readable, determinate bytes for every enabled capture. CPU capture can read more bytes than an
individual quantum basis path compares. It supplies classical parameters, not branch-dependent
coherent CPU memory effects. Volatile/atomic access, unknown pointers, dynamic extents, general
quantum layouts and indirect quantum ABI remain pending. CPU floating/aggregate representation bytes
do not implement quantum floating/aggregate operations.

The regenerated C11 inventory still has 783 pending interfaces, 73 partial interfaces and 368
metadata facilities. Both comparison entries remain partial. General strings, the other pending C11
interfaces and full source provenance before GCC transformations are not completed. These finite
checks do not prove general absence of bugs or physical QPU execution. Historical paper experiments
remain tied to their original revision, separately from the current compiler evidence.
