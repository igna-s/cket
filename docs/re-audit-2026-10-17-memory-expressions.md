# Checked memory fills and expression sequencing, 2026-10-17

The compiler now implements a partial `memset` adapter on checked integer quantum storage and
schedules discarded `memset`, `memcpy` and `memmove` calls before comma-expression results and in
selected lazy arms. Ordinary CPU calls remain libc calls. Cybersecurity stays classical.

## Behavior and corrected rejection

The [fill contract](c11-memset.md) supports fixed byte ranges, captures the value argument once and
preserves overwritten unknown values as history. Quantum value effects and aliases read before the
writes; zero count still evaluates the argument. Known-basis destinations may use reversible
in-place writes. These operations preserve correlations and phases without implicit observation.

Previously, `(memset(&value, bit, 1), value + 1)` reached the integer serializer as an unsupported
GCC `call_expr`. The [expression contract](c11-memory-sequences.md) schedules the memory effect
once, retains previously captured CPU parameters and uses saved decisions for coherent lazy arms.

An initial negative fixture incorrectly treated `(memset(&x, bit, 1), x) + x` as necessarily
undefined. N1570 6.5.2.2 paragraph 10 permits the otherwise unordered caller/call-body orders. The
corrected fixture accepts the permitted outcomes; original exploratory records remain preserved.
Actual unsequenced accesses within an argument and coherent CPU effects retain specific diagnostics.

## Verification and exact integration

The [full isolated campaign](audit-evidence/memory-expressions/results.json) passes fresh normal and
ASan/UBSan regressions on 134 frozen inputs. It includes 800 fill checks, 360 expression checks, 650
CPU snapshot checks, 391 native checks, 1452 example checks, 665 generated frontend compilations, 24
growth circuits and 70 independent QuEST circuits per configuration: 64 states and six sampled
measurement/reset channels. Dense states, complete labeled paths, native programs, diagnostic
contracts and generation checks retain their separate proof scopes.

The original process stopped during the sanitizer library matrix. The complete normal campaign,
fresh instrumented core build and completed sanitizer suites were retained. The entire incomplete
library suite and remaining examples/classical/fuzz/growth/QuEST tail resumed on the same 134
inputs. The [original prefix](audit-evidence/memory-expressions/interrupted-prefix.log) and
[resumed tail](audit-evidence/memory-expressions/resume.log) keep their process/source records. The
original invocation has no terminal exit status; the resumed tail returned zero. This is a completed
segmented campaign, not one uninterrupted invocation.

The [integration record](audit-evidence/memory-expressions/integration-summary.json) checks the
exact 17-input delta from the preceding 129-input state and all 134 resulting main hashes. Separate
fresh main normal and instrumented builds each pass both exact contract programs at five GCC levels
and both builtin modes: 20 executions per configuration. The full regression belongs to the isolated
source-identical checkout; no second full main regression or intermediate 132-input promotion is
claimed. Earlier 131/132/134 scoped experiments retain their own hashes and original reports.

## Remaining scope

The current inventory has 782 pending, 74 partial and 368 metadata entries, including 27 partial
function entries and 47 partial type/alias entries. None is classified as fully implemented. General
pointer returns and indirect ABI, runtime extents, dynamic allocation, general layouts,
volatile/atomic memory and broader observation/control ordering remain pending. These finite proofs
do not certify complete C11, arbitrary programs, physical QPU execution or a quantum speedup.

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
