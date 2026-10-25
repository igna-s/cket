# Version 1.0 intermediate specification and acceptance

**October 25 candidate:** [1.0.0-rc.1](releases/v1.0.0-rc.1.es.md) packages the implemented scope
and records missing work. It does not close the acceptance requirements below or the everyday C
goal. In particular, a nonidentity coherent heap copy still has a reproduced coherence failure. The
historical 0.4.0 release remains separately documented.

This document fixes a bounded 1.0 release contract, not certification that the current
implementation satisfies it. The user clarified the active project scope on October 23, 2026: a
[fully usable everyday C core](perfil-c-cotidiano.es.md), including functions, arrays, pointers and
vectors. This release contract is a milestone within that work; its exclusions do not remove dynamic
vectors or common floating-point work from the broader everyday profile. The
[complete inventory](c11-inventory.md) and [extended roadmap](c11-roadmap.md) retain the full C11
analysis as a reference. The [README](../README.md) records capabilities and evidence. Exhaustively
testing a small domain does not close a general requirement.

## Intermediate release subset

1.0 includes fixed-width integers from 1 to 64 bits, Booleans, fixed arrays and integer products,
borrows with classical addresses, owned scalar results, composable unitary functions, and explicit
CPU control. Defined C promotions, conversions, signedness, and effect order must be preserved for
the declared GCC target.

New computations preserve inputs; moves transfer ownership; measurement/reset are explicit. Cleanup
requires proof of the inverse, controls, and dependency versions. Rejecting an unsupported construct
protects this contract but does not implement that construct.

Outside this intermediate release: QPU execution, dynamic quantum memory, coherent addresses,
quantum floating point, complete C11 library compatibility, general quantum recursion and general
coherent loops. The extended language work remains on the roadmap; missing implementations are not
physical impossibilities. CPU jumps without lifetime proofs are currently rejected. A GCC fork is
not required; justification of the transformation boundary is.

Language width, circuit workspace, and simulation have distinct limits. The current reference
exports up to 128 physical qubits and simulates up to 24 in the default dense mode. The opt-in
[sparse runtime](simulacion-dispersa.es.md) supports up to 128 physical wires with an explicit
occupied-support budget. Exhausting a budget produces a diagnostic rather than silent enumeration of
a large domain.

## Priorities and verifiable criteria

| Priority        | Contract                                                                                  | Evidence required to close it                                                                  |
| --------------- | ----------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------- |
| P1. Arithmetic  | Register multiplication, division, remainder, and comparisons without tables.             | Amplitudes/inverses against C, more than 12 relevant bits, bounds, and gate/ancilla/depth CSV. |
| P2. Search      | Separate oracle and strategy; known/unknown counts, verified candidates, bounded failure. | Zero, one, several, and all solutions without a domain census.                                 |
| P3. Flow        | Correct ownership, borrows, and dependencies on every accepted path.                      | Joins, loop invariants, measurements, returns, temporaries; positive and negative cases.       |
| P4. Composition | Separate compilation, typed ABI, transferred results, control, inversion.                 | Linked modules, rejected aliases, cleanup, and relative phases under control.                  |
| P5. GCC         | Preserve semantics across the plugin boundary.                                            | Optimization differentials, promotions, casts, short circuits, effects evaluated once.         |
| P6. Validation  | Read original QASM in another implementation; reproducible CI/fuzzing.                    | Complete states, measurements, execution logs, failure reproductions.                          |
| P7. Execution   | Separate static plans, CPU parameters, and CPU control.                                   | Counters proving no repeated parsing/synthesis; equivalent final states.                       |

P1 and P2 are the first scalability blockers; P3 follows immediately. P5 and P6 accompany every
change.

## P1: synthesis and complexity

New computations implement `|a,b,0> -> |a,b,f(a,b)>` with workspace returned to zero. XOR-copy
permits subsequent uncomputation. In-place updates require an independently injective
transformation.

Required: addition/subtraction, unsigned modular multiplication, quotient/remainder with a proven
nonzero divisor, and all six register comparisons. Preserve signed promotions and prove absence of
overflow before narrowing output. Reject division by zero, signed overflow, and undefined shifts.
Preserve conditional evaluation and short circuits.

Compile widths 4, 8, 12, and 16 with tables disabled; two 16-bit inputs have 32 relevant bits. Check
complete small domains against independent C, amplitudes, controls, and inverses. Publish gate
classes, peak ancillas, physical qubits, and depth as CSV. MCX is a logical gate; physical
decomposition has a separate cost.

Document algorithmic bounds. Current controlled-increment constructions have at most cubic logical
gate cost and linear workspace for a binary operator. Composition needs a bound by tree size and a
temporary strategy. Tokens, depth, symbolic terms, actual work, and memory need global budgets:
avoiding tables does not ensure scalability.

`src/engine/arithmetic.inc` implements negative operands, truncation toward zero, remainder with the
dividend's sign, general comparisons, and `--no-tables`. Nonzero divisors are proven through ranges
or constant bits; MIN/-1 is excluded. An optional finite path remains for small expressions,
uncovered cases, and permutations. More precise ranges, large compositions, and 64-bit division
remain open. **P1 is partial.**

## P2: search

Oracle construction must not count satisfying inputs. Evaluating measured candidates is permitted;
evaluating every input beforehand to select a strategy is not.

| Operation              | Information                    | Result                                                                                              |
| ---------------------- | ------------------------------ | --------------------------------------------------------------------------------------------------- |
| `find_known(x,p,M)`    | Caller promise: `1 <= M <= N`. | Transfers the amplified register; `P(r)=sin²((2r+1)asin(sqrt(M/N)))`. No census checks the promise. |
| `search(x,p,attempts)` | Unknown count, including zero. | CPU Boolean; `true` requires a measured candidate satisfying `p`, `false` means budget exhaustion.  |

`find(x,p)` means `find_known(x,p,1)`, changing the previous counting prototype. Supply M or use
`search` for several solutions.

`search` equally mixes direct samples and a uniform Grover iteration count from zero to
`ceil(sqrt(N))-1`. Under independent uniform randomness, a nonempty solution set succeeds with
probability at least `1/8` per attempt; failure after t attempts is at most `(7/8)^t`. For M at
least N/2, direct samples suffice; otherwise average Grover success is at least `1/4`, since
`K*sin(2*theta) >= 2*sqrt(1-1/N)`, with `K=ceil(sqrt(N))`.

The runtime uses a reproducible PRNG without modulo bias. The bound describes a probabilistic model,
not every seed. Every success is verified. Zero solutions exhaust attempts without proving
nonexistence. Measurement/reset and repreparation occur between attempts; the final measured
candidate remains available.

Acceptance includes zero/one/several/all solutions, domains above 12 bits, cleanup, finite budgets,
and rejection of flat QASM for CPU feedback. Regressions cover these contracts; broad statistical
campaigns and adaptive strategies with lower query cost remain pending.

## P3: flow and ownership

Every point needs each register's identity, owner, borrow, availability, measurement/reset status,
range, and inverse state. Joins retain only facts true in every predecessor. Loops include zero
iterations where applicable and prove the back edge.

Required: coherent `if` inside CPU loops with stable owners, and measurement feeding CPU decisions.
Clean ancillas each iteration. Reject consumption in one branch, use after move, overlapping mutable
borrows, and changes to pending dependencies.

Current analysis conservatively joins branches/loops and invalidates ranges before dynamic CPU
control. It supports these combinations, terminal owned scalar returns, and pure conditional
returns. Dynamic owners, early returns with pending lifetimes, `goto`, `switch`, branch-dependent
identities, and complete CFG reconstruction remain pending. **P3 is partial.**

## P4: functions and ABI

Declare representation, width, signedness, borrow/consume modes, results, effects, and cleanup
obligations. Owned results may escape by transfer; borrowed references cannot become new owners.

Unitary functions support control/inversion. Inversion reverses order and angles; control preserves
global phase, which becomes relative. Reject measurement, reset, search, CPU feedback, and dirty
workspace inside these modules.

The QIR ABI provides `qborrow`, `qc_circuit_compile`, `qc_circuit_apply`, and C composition through
`circuit`/`inverse_circuit`. Independent files are embedded during compilation and plans retained by
the runtime. [circuit-abi.md](circuit-abi.md) defines restrictions. Arbitrary C `quant` linking
across units and owned product results remain pending. **P4 is partial.**

## P5: GCC

The transformation boundary is `PLUGIN_PRE_GENERICIZE`; earlier GCC transformations are trusted.
`volatile` preserves certain accesses without proving preservation of every promotion or condition
for defined behavior.

Require CPU references/differentials at `-O0`, `-O1`, `-O2`, `-O3`, and `-Os` for promotions,
narrowing, signedness, short circuits, and effects. Retain GCC's discarded pure expressions as
`sequence`. Reject ambiguous early multiplication narrowing when the domain could overflow the
original `int`; require a full-width unsigned result before narrowing.

The C generator produces reproducible programs, seeds, and failure files. More patterns and GCC
versions are required. A general proof of transformations preceding the plugin is not claimed.

## P6: validation and CI

The reference reads unmodified exported QASM and compares complete states up to a common global
phase, with maximum amplitude error `1e-10`. Required cases include superposition, entanglement,
relative phases, open/closed controls, Fourier/inverse, complete Grover, and correlated
measurements.

Current validation programs are C. They use the independent serial C backend of QuEST 3.7.0, pinned
by commit and archive checksum. A separate reader handles the static OpenQASM subset emitted by
`cket`, rather than all of OpenQASM. Reference simulation does not consult QIR or call the native
gate executor. Eight circuits compare amplitudes and exact final measurement probabilities,
including classical bit mappings. Three compare 8192 complete intermediate measurement/reset
transcripts between the native runtime and QuEST.

The transcript comparison uses Hoeffding and a union bound over both empirical distributions and
every outcome, with family-wise false rejection probability at most `1e-6` under independent
sampling. Tolerances and seeds are explicit; this does not certify each PRNG seed. Exported dynamic
feedback, noise, and every reset channel remain outside coverage.

CI builds with GCC 11 and runs native tests, ASan/UBSan across the project's compiler chain, growth
without tables, generated frontend differentials, and independent QASM validation.
Logs/reproductions are archived. The
[first remote run](https://github.com/igna-s/cket/actions/runs/38150646854), completed October 11,
2026, passed the previous Python/Qiskit-based campaign; release evidence retains this historical
distinction. Current validation uses C. Larger campaigns and additional intermediate measurements
are needed. **P6 is partial.**

## P7: reuse

Static plans, CPU parameters, and CPU decisions are distinct layers. Check lifetime/types each call;
do not repeat parsing/synthesis while a plan's preconditions hold.

Retain parsed documents, expression/addition plans, and compiled modules. Keys include expressions,
wires, ranges, and controls. Reuse private workspace only when its wires are not live. Addition
parameters select value variants; CPU indices select identities without measurement. Caches have
explicit budgets.

Eight computations and their inverses check five parses, one synthesis, fifteen plan hits, and final
state. General linking of synthesized gates and parameterized templates to avoid first-call
synthesis remains pending. **P7 is partial.**

## Release

Do not announce 1.0 until all seven requirements have implementations, diagnostics, and reproducible
evidence for the defined subset. QPU support, dynamic memory, and wider C coverage do not replace
correctness or scalability.
