# Evidence for the tested C11 subset

The new [controlled concatenation advance](controlled-concat-v2/README.es.md) adds coherent Boolean
control and a phase/Fourier example to the existing string adapters. Seven examples execute and
export; complete state/inverse and compiler-context qualification remains deferred. The first
attempt's actual export rejection and source snapshot are preserved.

The previous [concatenation advance](string-concat-v1/README.es.md) adds partial strcat/strncat with
snapshotted original lengths and retained-history updates. Six runnable examples remain separate
from deferred complete coherent/inverse and compiler-context qualification.

The previous [string-copy advance](string-copy-v4/README.es.md) adds partial strcpy/strncpy with
retained overwrite history, known CPU sources and explicit static export bindings. Failed attempts
retain their actual outcomes; complete qualification remains deferred.

The previous [saved-search advance](saved-search-v2/README.es.md) adds local immutable search
pointers, aliases and character reads, with backend-proven support intervals. Its four examples and
normal native regressions remain separate from the deferred complete qualification. The
[delivery index](delivery-index.json) identifies the active source snapshot; earlier deliveries
retain their original sources and results.

The previous [search-position advance](search-positions-v2/README.es.md) adds first/last positions,
same-object differences and address comparisons. Its three runnable examples remain separate from
deferred full qualification; the earlier failed compilation is preserved.

The previous [string library advance](string-library-v1/README.es.md) adds seven partial interfaces:
prefix lengths and direct null search predicates. The compiler and two examples build and execute;
full coherent/inverse, sanitizer and GCC-context qualification is explicitly deferred. Historical
campaigns below do not qualify this new implementation.

The previous [shift truth/cast advance](shift-truth-v3/README.es.md) removes three previous resource
failures and corrects a numeric logical-negation error in the arithmetic fallback. The fourth block
02 example executes with 85 wires. Targeted checks retain source identity, earlier failed attempts
and the exact wrong-state reproduction; complete C11 remains open.

The previous [shift multiplexer](shift-mux-v1/README.es.md) removes the former full-word stage
workspace, enabling complete32-bit variable-shift circuits and a105-wire native example. Targeted
normal/sanitizer runs check71 shift domains/contracts,3505 expressions and393 native cases on161
inputs, with complete finite phased sparse states and actual previous failure preserved.

The previous [sparse execution advance](sparse-simulation-v1/README.es.md) makes low-support int32
and LP64-size_t64 examples executable in the native C runtime. Targeted normal/sanitizer runs pass
88 new checks,393 native tests and1509 example checks on159 frozen inputs. A separate reproduction
confirms and fixes the old dense small-probability cutoff. Full C11 remains open.

The previous [usable block 01](common-division-v3/README.es.md) adds small-constant quotient
synthesis, direct Boolean consumers and a runnable superposition/bucket example. Its fresh Linux
delivery runs eight programs and exports, with 1509 example checks. Separate normal/sanitizer
qualifications pass 3505 expression checks per configuration, including 2310 new finite gate-action
domains. These targeted results do not certify complete C11 or a new broad campaign.

The previous [151-input integration](selection-views-v5/README.es.md) adds internal selection views,
288 maintained finite domains and 768 independent compiled complete-state checks. Direct selection
between signed64-bit limits now fits the existing gate representation. Targeted normal/sanitizer
checks pass with explicitly verified tool reuse; the fresh broad151 campaign remains in progress.
The [previous148 integration](common-remainder-v2/README.es.md) adds small-constant remainder, and
the [previous146 integration](known-fill-cleanup-v2/README.es.md) adds known-fill scope cleanup.

The historical [144-input integration](strings-integration-v6/README.es.md) adds partial
`strlen`/`strcmp`, supported deferred phase angles and guarded CPU loads. Its targeted normal and
ASan/UBSan checks passed, including 519 binding checks and 460 string checks per configuration.
Independent compiled string circuits passed 640 complete complex states with saved inverses. Its
80-command broad campaign later completed in two segments, archived with the
[148-input evidence](common-remainder-v2/README.es.md); its
[remote CI](integrated144-remote-ci/README.es.md) also passed. These results do not certify the
later inputs. The current inventory has 769 pending, 87 partial and 368 metadata entries.
Cybersecurity stays classical.

The preceding 138-input [full campaign](controlled-operands-full/summary.json) and
[remote CI](integrated138-remote-ci/summary.json) passed on their exact historical inputs. Their
results do not certify the new 144-input source state. Earlier sections retain their original source
maps, outcomes and finite scopes.

The historical [memory-expression integration](memory-expressions/integration-summary.json) adds
checked `memset` writes and discarded memory-call composition. Its complete fresh normal and
ASan/UBSan [isolated campaign](memory-expressions/results.json) identifies 134 inputs, 800 fill
checks, 360 expression checks and 70 independent QuEST circuits per configuration: 64 states and six
sampled channels. All 134 input hashes matched its original main-tree installation. Separate fresh
main normal/instrumented builds each passed both exact contract programs at five GCC levels and both
builtin modes, for 20 executions per configuration; those targeted runs are not a repeated full main
regression. The [follow-up](../re-audit-2026-10-17-memory-expressions.md) describes the corrections,
finite proof categories and pending pointer/count/layout work.

That campaign completed in segments after the original WSL process stopped during the sanitizer
library suite. The original prefix and resumed whole library suite and remaining tail have separate
logs and metadata; the original exit status is unknown. All 134 sources matched at resumption and
completion.

The [preceding memset experiment](memset/integration-summary.json) preserves its separate 132-input
segmented broad campaign. Earlier scoped proofs and their original failures remain archived. Current
C11 coverage at that point was 782 pending, 74 partial and 368 metadata entries; cybersecurity
remained classical. Counts in the older sections below describe their original campaigns.

## Preceding byte-snapshot source state

The preceding [CPU byte-snapshot integration](byte-snapshots/integration-summary.json) supplies
current classical object bytes to partial `memcmp`/`strncmp` comparisons. Its 129
[source inputs](byte-snapshots/sources.tar.gz) were frozen before fresh complete normal and
ASan/UBSan verification in segments. The interrupted normal prefix and resumed normal/sanitized tail
are preserved separately; sources matched before resumption and the sanitizer binaries were built
fresh. The [complete transcript](byte-snapshots/verify.log) and
[suite counts](byte-snapshots/results.json) identify the isolated campaign. No instrumented core
binaries were reused from earlier source states.

The candidate passes 650 snapshot checks and all established broad suites in each configuration,
including 63 independent QuEST circuits: 57 states and six sampled channels. All 129 source hashes
then matched the main checkout. Separate [main normal](byte-snapshots/root-normal.log) and
[instrumented](byte-snapshots/root-sanitized.log) runs each pass the 650 checks and exact contract
example after fresh core/plugin/runtime builds; isolated results are not relabeled as main runs.

The [follow-up report](../re-audit-2026-10-17-byte-snapshots.md) records the protected-page/NUL bug,
failed eager/inline prototypes, guarded CPU helper and actual capture/inverse checks. Preserved
exploratory failures remain distinct from successful full verification. The
[contract](../c11-byte-snapshots.md) states the readable/determinate snapshot requirement and the
limits on addresses, layout and volatile/atomic access. C11 coverage remains 783 pending, 73 partial
and 368 metadata entries. Cybersecurity remains classical and excluded from quantum adaptation.

## Preceding bounded-string source state

The preceding [bounded-string integration](strncmp/integration-summary.json) adds partial `strncmp`
support. Its 127 [source inputs](strncmp/sources.tar.gz) were frozen before the isolated normal
rebuild and instrumented-plugin rebuild, then copied byte-identically to the main checkout. The
[main-tree normal](strncmp/root-strncmp-normal.log) and
[sanitized](strncmp/root-strncmp-sanitized.log) reruns each pass the 246-check suite and the exact
contract example. Its source hashes identify the actual local changes; a base commit alone does not
identify the tested implementation.

The isolated integration repeats 391 native checks, 1452 example checks, five classical-header
comparisons, 302 `memcmp` checks, 246 `strncmp` checks and 59 QuEST circuits in each configuration.
The QuEST suite now has 53 states and six sampled channels; the two new string states use 21 and 24
qubits. The unchanged compiled driver/runtime/engine sources match the preceding full campaign. This
scoped integration is not relabeled as a full rerun of every original suite after `strncmp`. The
report lists all ten changed compiler/test/script/CI inputs, including the new validator and the
result-wire mapping optimization. The latter precomputes and checks a mapping once before the
amplitude loop, retaining all state/probability comparisons.

The [full-result capsule](strncmp/raw-circuits.tar.gz) adds 36 normal/sanitized exports of signed
32-bit `memcmp`/`strncmp` results. These comprise 17 distinct QASM programs; some exports are
identical across functions or configurations. Sparse simulation checks all 256 byte patterns at
seven references and all 65536 word patterns at two references for each function. Labeled basis
paths additionally check that an input permutation cannot hide behind uniform amplitudes. Every
selected path preserves its inputs, computes the full result, retains the expected phases and
returns private wires to zero. The circuits reach 76 qubits while occupying a sparse subspace. These
checks use `-O2`, builtins and no-table export; they do not simulate arbitrary dense states or
channels at 76 qubits. GCC may produce different valid circuits for `x-1` and `x+(-1)` with
sanitizer flags, so each distinct exported circuit is checked separately.

A separate [native endpoint capsule](strncmp/native-endpoints.tar.gz) records 24 programs with eight
word endpoints each, all six relations, two references and both builtin modes at `-O2`. The
[exploratory capsule](strncmp/exploratory.tar.gz) retains the earlier native timeout and audit
assumptions that were corrected. None of those failed runs is counted as passing verification. The
[string contract](../c11-strncmp.md) states the supported cases and remaining bounds.

That historical index had 783 pending interfaces, 73 partial interfaces and 368 metadata facilities.
Full quantum C11/library support, general layouts and dynamic memory, mutable CPU snapshots,
indirect quantum ABI and full pre-GCC source provenance remain open. These finite checks do not
prove absence of bugs or physical QPU execution. The CI budget is now 180 minutes; no hosted CI run
is claimed by these local checks. The older campaigns below retain their exact sources and logs.

## Preceding byte-comparison source states

The preceding [byte-comparison follow-up](memcmp/comparison-summary.json) is verified in three
explicit segments. The [normal prefix](memcmp/normal-prefix.log) completes all core suites, then
fails because its QuEST importer accepted at most 20 qubits and the new word-comparison circuit
needs 22. That failure remains in the log. After changing only the three reference-validation files,
the [complete normal QuEST suite](memcmp/reference-normal.log) and the
[complete ASan/UBSan phase](memcmp/sanitized.log) pass. This is not an uninterrupted successful
`verify.sh` run on one snapshot.

The two [prefix](memcmp/normal-prefix-sources.tar.gz) and [final](memcmp/sources.tar.gz) source
capsules each contain 126 inputs, their SHA-256 manifest and the license. All 123 inputs outside the
three reference validators are byte-identical between them. The machine-readable report identifies
each segment's source state, commands, exit status, toolchain and artifact hashes. Frozen snapshots
identify the pre-build and archival endpoints; they do not continuously monitor compilation or all
external dependencies.

Each configuration passes 302 `memcmp` checks: 92 complete phased state/inverse cases, 110 native
programs, 70 negative contracts and 30 wide no-table generations. Native relation/raw-result
programs exhaust all 256 byte values. Wider complete-state cases include all 65536 patterns of a
two-byte input at `-O2`; other optimization levels have separate native/generation checks. QuEST
checks all 57 circuits: 51 states and six sampled channels, including four new comparison states.
The reference importer now accepts up to 24 qubits and 20 result bits, with separate resource
diagnostics and parser boundary checks. Actual independent states include the new 22-qubit
word-comparison case.

A supplementary [Grover composition capsule](memcmp/oracle-states.tar.gz) preserves its C source,
checker, IR, states and reports. Twenty normal and twenty sanitized states compare a `memcmp`
predicate with the analytic amplitudes for one marked input in 256, including private-wire cleanup,
both table policies, five GCC levels and both builtin modes. Twelve iterations give predicted
success about 0.999947; this is a probability rather than a deterministic guarantee. These forty
checks are separate from the 302-check suite and the 57-circuit QuEST suite.

The existing arithmetic, memory, binding, history, locale, example, frontend and harness suites
complete in each configuration as recorded above. The frontend campaign has 665 compilations, seed
20261012 and all 256 byte values per expression. Normal and sanitized resource CSVs match. At that
source state the inventory had 784 pending interfaces, 72 partial interfaces and 368 metadata
facilities; `memcmp` moved from pending to partial. The 2100 library-boundary checks include its
forbidden implicit CPU-result context. Those rejections do not implement pending interfaces.

Mutable CPU byte snapshots, dynamic extents/addresses, general layouts and an indirect quantum ABI
remain open. The [comparison contract](../c11-memcmp.md) states supported cases and bounds. Finite
simulator evidence does not prove absence of bugs, complete quantum C11/library support or execution
on a physical QPU.

## Earlier source states

Two earlier locale-related campaigns identify prior source states. The broad campaign in
`verify.log` and `results.json` precedes the final numeric-locale correction. Its 122 source inputs
are preserved in [broad-source.tar.gz](broad-source.tar.gz), together with their SHA-256 manifest
and the license. The archive contains source text, not generated executables or external
dependencies.

The later [numeric-locale report](numeric-locale.json) identifies that correction's C sources and
the byte-identical copies used for targeted normal/ASan tests. Those tests repeat 391 native checks,
1452 example checks, 1815 character checks, 96 profile proofs, 70 numeric-locale checks and all 53
independent QuEST circuits in each configuration. Its two logs are `numeric-locale-normal.log` and
`numeric-locale-sanitized.log`. Copy identities were captured after build start and checked after
testing/application; no pre-build frozen snapshot is claimed for that targeted campaign. The earlier
broad transcript is not relabeled as a full rerun of the subsequent tree.

`verify.log` records the completed broad normal and ASan/UBSan campaign. `results.json` identifies
the base commit, modified working tree, toolchain, seed, counts, source hashes and artifact hashes.
`synthesis-growth.csv` contains the 24 resource rows. Hashes identify the tested source bytes; the
base commit alone does not identify the local modifications.

Each configuration includes 391 native checks, 1452 example checks across 36 programs, 18 runner
fault-injection checks, 55 shift checks, 97 pure-expression checks, 271 in-place checks, 414
memory-copy checks, 990 division checks, 399 CPU-binding checks, 2332 history checks, 1815 immutable
byte-locale checks and 96 profile-proof regressions. Wide circuit generation is distinguished from
complete state validation in each suite's log.

The library boundary matrix checks 210 pending interfaces with 2100 specific diagnostics at five GCC
levels and both builtin settings. Every fixture first compiles as ordinary C. These negative checks
do not implement the interfaces. The frontend corpus has 133 programs and 665 compilations per
configuration, using seed 20261012; each expression is compared with C at all 256 byte values.
Basis-value comparisons remain distinct from coherent phase and inverse tests.

QuEST independently checks 53 exported circuits: 47 states/final probability distributions and six
intermediate-measurement/reset channels, with 8192 transcripts per implementation for each channel.
Exports use `--no-tables`. The extended sanitized validators run beside the instrumented compiler
and backend, with the GCC wrapper and instrumented generated programs. Their growth CSV must equal
the normal CSV. Host GCC itself is not instrumented; its process disables leak attribution while
ASan/UBSan remain active in the plugin.

Character tests generate an isolated Latin-1 locale fixture. A narrow LeakSanitizer suppression for
glibc's `argz_add_sep` allocation applies only to the locale-fixture validators. A separate guard
checks that project leaks and leaked locale handles remain detectable. The JSON report records the
scope and upstream reference; the same narrow exception covers the new numeric-locale fixture. This
exception does not disable general leak detection.

For a new complete campaign on the final sources, use the declared GCC/G++ 11.4.0 Linux/WSL target:

```bash
export CC=gcc-11 CXX=g++-11
export CKET_FUZZ_CASES=128 CKET_FUZZ_SEED=20261012
mkdir -p build/audit
python3 scripts/archive-audit.py --freeze build/audit/current-sources.json
bash scripts/verify.sh >build/audit/verify-final.log 2>&1
python3 scripts/archive-audit.py --log build/audit/verify-final.log \
    --sources build/audit/current-sources.json \
    --destination docs/audit-evidence/reproduced
```

The optional Python archiver compares a pre-verification snapshot with hashes captured at archival.
Snapshot creation refuses to overwrite a file. A source mismatch or incomplete/failing transcript
preserves the previous archive. Snapshot coverage includes C/C++ sources, headers, include
fragments, scripts, CI/format configuration and JSON inputs such as `examples/increment.qir.json`.
These are endpoint checks, rather than continuous monitoring of compilation or all system
dependencies. Choose a new `--destination` when comparing campaigns so earlier files remain
preserved.

The [2026-10-14 re-audit](../re-audit-2026-10-14.md) documents the profile-equality bug, stale
interface contracts, snapshot coverage and sanitizer subprocess correction. The 1224-entry interface
index at that source state listed 785 pending interfaces, 71 partial interfaces and 368 metadata
facilities. Neither this campaign nor the five classical integration comparisons across 29 header
families certify complete quantum C11/library support, absence of bugs or physical QPU execution.

Historical paper evidence stays under `docs/paper/evidence/` and belongs to its fixed revision. The
[isolated rerun summary](re-audit-baseline.json) reproduces its 354 native tests, 426 example
checks, 145 frontend compilations, 24 synthesis rows and 11 QuEST cases after comparing 60 source
files with Git. The current byte corpus, locale profiles and boundary tests are subsequent work.
Earlier incremental cohorts remain described in the language contracts and
[repository audit](../repository-audit.md); their counts are not the current campaign totals.

The successful continuation and the separate fresh integration checks ran in a hash-verified Linux
filesystem copy, without Windows drive mounts. The first continuation was also interrupted and has
no known terminal status. Existing core binaries were transferred for the successful broad tail; the
separate integration tools were freshly rebuilt. Applying the identical files back to the Windows
repository requires an additional destination-hash guard.

A further WSL restart interrupted sanitizer fuzzing after 63 complete cases. The successful
continuation retained that prefix and used an archived temporary driver with unchanged generator,
generated program and oracle. Its original seed and case indices, all five optimization levels and
all 256 byte inputs were preserved. The 665 sanitizer fuzz compilations are an explicitly verified
segmented aggregate; extra harness sources and the original interrupted raw log are retained
separately.

A later restart interrupted the final sanitizer QASM suite after fuzzing and growth checks had
already closed. Only that unfinished QASM suite was repeated in the final segment, with the earlier
raw logs retained.
