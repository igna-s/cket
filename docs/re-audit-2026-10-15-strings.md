# Bounded string comparison and full-result audit, 2026-10-15

`strncmp` now lowers to input-preserving integer byte comparisons with an equal-NUL stopping
condition. It shares checked object views with `memcmp`; direct relations against zero retain their
Boolean lowering, and unsigned/narrowing casts retain the complete integer result first. Ordinary
calls go through a system-header alias before GCC can fold one-byte builtins; classical calls still
link to libc. General wrapper bypass, dynamic byte ranges, mutable CPU snapshots, layout/provenance
and indirect ABI remain separate pending work.

## Implementation and evidence

The [integration record](audit-evidence/strncmp/integration-summary.json) identifies 127 source
inputs frozen before its normal rebuild and instrumented-plugin rebuild. Only ten inputs differ from
the [preceding full verification](audit-evidence/memcmp/comparison-summary.json). Among
`src`/`include`, only the byte-comparison mixin and string header differ; the compiled engine,
driver and runtime sources are identical. Instrumented common binaries were reused after checking
their source identities and build timestamps. The new plugin and validators were rebuilt.

In the isolated checkout, each configuration passes 246 `strncmp` checks, the existing 302 `memcmp`
checks, 391 native checks, 1452 example checks, five classical-header comparisons and all 59 QuEST
cases (53 states, six sampled channels). No claim is made that every previous suite was rerun after
these changes. All 127 inputs were then applied byte-identically to the main checkout; fresh main
normal and instrumented-plugin builds each pass the 246-check validator and the exact contract
example. Isolated logs retain their original scope and paths.

The new validator separates 56 full complex-state/inverse cases, 80 native programs, 80 negative
contracts and 30 wide no-table generations. It covers byte patterns, signed object representations,
all 65536 word patterns for NUL-stop and continued-tail cases, two coherent inputs and constant
zero-length results. Wider coherent cases use `-O2`; byte cases span five GCC levels, both builtin
settings and both table policies. The two new QuEST circuits use 21 and 24 qubits. Their mapping
validation now records result wires once before the amplitude loop, avoiding a scan of every gate
for every basis without reducing the states or probabilities checked.

## Full signed results beyond dense simulation

The supplementary sparse audit checks the full signed 32-bit results, rather than only a narrowing
projection. It records 36 normal/sanitized exports: seven references over all 256 byte patterns and
two references over all 65536 word patterns for each function. These are 17 distinct QASM programs
after hashing identical exports. Each distinct program is simulated under independent H/X/phase
semantics, with terminal observations skipped explicitly. Inputs, result bits, relative phases and
all private zeros match the reference. Selected circuits reach 76 physical wires while their support
remains sparse; this is not a dense 76-qubit simulation or a general channel test.

A second check follows every labeled basis path after verifying canonical H preparation and a suffix
of reversible X/phase gates. This detects input permutations that a uniform state could hide. For
each selected path, it verifies unchanged input values, the full signed result, phases and clean
private storage. Its scope is the listed finite circuits and fresh zero ancillas, not all possible C
programs, metadata states or wider inputs.

GCC with sanitizers can retain subtraction where an ordinary build folds addition of a negative
constant. Circuit text equality is therefore not imposed on this corpus; both distinct forms are
checked. The separate 24-circuit growth CSV equality belongs to its original corpus.

The archive also records 24 native endpoint programs covering all six relations and eight word
values per program at `-O2` in both builtin modes. An earlier 256-value native program with a
nonzero-prefix tail exceeded its 120-second command limit. The initial constant-result inverse test
incorrectly attempted to release a prepared `|1>`; the engine rejected it, and corrected unitary
disposal then passed complete phase checks. Those exploratory failures and the initial
circuit-identity assumption remain preserved and are not counted as successes.

## Remaining scope

The regenerated C11 index records 783 pending interfaces, 73 partial interfaces and 368 metadata
facilities. `strncmp` moved to partial; all n bytes must fit both checked objects even when one
particular input would stop earlier. The 64-byte frontend bound is separate from expression, gate,
128-qubit IR and 24-qubit dense/native limits. This does not implement `strcmp`, `strlen`, general
string processing, floating/complex quantum C, general memory or the rest of C11. Physical QPU
execution and absence of bugs are not proved. Historical paper experiments remain tied to their
fixed revision, separately from this implementation and these checks.
