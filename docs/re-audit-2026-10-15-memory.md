# Coherent byte-comparison follow-up, 2026-10-15

`memcmp` now has a partial quantum adapter for padding-free integer object bytes, checked byte
views, fixed extents and immutable classical operands. Inputs and relative phases are preserved;
there is no implicit measurement/reset. Direct comparisons against zero use Boolean lexicographic
predicates. Raw results retain the first differing unsigned-byte difference.

GCC could fold a one-byte call into host loads before the quantum pass. The new system-header
wrapper keeps ordinary call syntax visible while classical calls still link to libc. General source
provenance and an indirect quantum ABI remain pending. Mutable CPU object initializers are never
treated as their current values.

Constant shifts now select existing wires/sign bits/zero bits. Ordered constant comparisons use
disjoint bit prefixes without a difference register. These changes reduce workspace without
enumerating quantum input values. Definedness checks and inverse certificates remain active.

## Verification and source identity

The [verification report](audit-evidence/memcmp/comparison-summary.json) records three segments,
rather than claiming an uninterrupted successful full run:

1. The original normal run completes every core suite, then fails when the independent QASM importer
   rejects a valid 22-qubit comparison circuit under its former 20-qubit budget. Its unedited log
   and exact frozen source capsule are retained.
1. The importer budget is raised to 24 qubits, independently retaining the 20-result-bit budget. The
   three reference-validation files are the only changed inputs among the 126 frozen files. All 57
   normal QuEST cases then pass; parser tests separately accept the maximum wire/control declaration
   and reject qubit/result exhaustion with resource diagnostics.
1. The complete ASan/UBSan phase, including the leak-boundary guard, all core suites and all 57
   QuEST cases, passes on the final pre-build snapshot. Its normal/sanitized growth CSVs match.

The other 123 frozen inputs are byte-identical between segments. Capsule verification compares each
archived source with its corresponding pre-build hash, and current final-source hashes are checked
again at archival. This proves endpoint identity, not continuous observation, complete dependency
capture or compiler correctness. Earlier broad and locale-specific campaigns retain their original
files and identities.

The new validator passes 302 checks per configuration: 92 complete complex-state/inverse cases, 110
native programs, 70 specific rejections and 30 wide no-table exports. Complete-state cases include
all byte patterns, all 65536 two-byte patterns, exact aliases, byte views, two coherent inputs and
small raw-result domains. Wider coherent cases use `-O2`; byte cases span all five GCC levels,
builtin settings and both table policies. Native relation/raw-result programs exhaust all 256 input
bytes. Wide generation remains distinct from complete-state validation.

All existing native, example, arithmetic, copying, binding, history, character/profile, numeric
locale, boundary, frontend and growth suites complete in both configurations. QuEST independently
checks 57 circuits (51 states, six channels), including four new comparison states. A separate
Grover composition check compares all amplitudes and private-wire cleanup with the one-solution
analytic formula: twenty normal and twenty sanitized states, five GCC levels, both builtin modes and
both table policies. For 256 inputs, twelve iterations give success probability about 0.999947. This
result does not promise a solution on every random seed or validate arbitrary oracle promises. The
source, checker, IR and states are preserved in the supplementary capsule.

The exact example in the [contract](c11-memcmp.md) compiled and executed. The C11 index was
regenerated temporarily from the local official N1570 PDF; all 1224 entries/contracts match the
generator. The completeness gate still fails as intended for an incomplete implementation.

## Remaining work

Compatibility remains incomplete: 784 pending interfaces, 72 partial interfaces and 368 metadata
facilities. Dynamic byte counts/addresses, mutable CPU snapshots, padding/general layouts and
indirect calls remain pending for this adapter. Generic expression/gate/qubit limits apply in
addition to its 64-byte frontend bound. Procedure alias exclusions still apply. The simulator
results provide finite evidence on the declared GCC/glibc target; physical QPU execution and absence
of bugs are not proved. Historical papers retain their fixed revision and experimental results;
these subsequent tests are not attributed to their baseline.
