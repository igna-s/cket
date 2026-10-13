# Quantum IR validation

The verifier in [src/engine/verify.c](../src/engine/verify.c) checks serialized IR before
synthesizing or executing each operation. Both the linked runtime and document validation/export use
it. GCC remains the sole C parser.

## Schema and types

- Every operation and expression has an explicit set of allowed fields. Unknown and duplicate fields
  are rejected.
- Widths are integers from 1 to 64 bits; `signed` must be a real JSON Boolean.
- Operators have checked arities. All branches are structurally checked, including those not
  evaluated for a particular value.
- Arithmetic requires operands converted to the corresponding common type. Comparisons require
  compatible types; shifts retain the left operand's type.
- Conversions to the destination type must appear explicitly in the expression, except Boolean
  results used as one-bit guards.
- Constants must fit their declared representation.
- The unary signed integer `abs` operation requires matching signed operand and result types; an
  unrepresentable signed minimum is rejected. Frontend adapters retain C promotions before applying
  this operation.
- `uabs` has an unsigned result and a corresponding signed operand of the same width. It includes
  the operand's signed minimum, whose magnitude fits that unsigned result; it preserves GCC's
  distinct `ABSU_EXPR` semantics. The arithmetic circuit keeps the original sign before narrowing a
  result.
- Controls require available Boolean registers. Contradictory controls and control/target aliases
  are rejected.
- `qmeasure`, `qobserve`, `qreset`, `qprepare`, `qalloc`, `qrelease`, `qretain` and `qhistory_end`
  do not accept a coherent control field.
- Phase angles must be representable finite numbers.
- Documents check format, version, header fields, and balanced CPU control markers.

`qretain` and `qhistory_end` accept only `op` and the register `id`. Retaining a prepared value
marks its history lifetime without changing gates or its value version; live inverse borrows remain
valid. Ending history requires no live dependents. A known basis root is restored to zero with X
gates and released; an unknown root remains allocated as a joint output until context end. There is
no implicit measurement, reset or partial trace. Derived history instead uses the ordinary saved
inverse and clean release. Current/peak retained qubits are included in existing resource totals.
See [history contracts](c11-history.md) for compiler scope and remaining control/memory obligations.

`qphi` performs gate-free ownership transport for a bound classical branch decision, relabeling
saved inverse dependencies without copying physical state. `qdispose` ends an optional logical
lifetime and defers actual inverse/release until its dependents finish. Unknown roots remain
retained outputs. Both have strict schemas, and borrowed unitary modules reject them. CPU templates
with version joins receive structural/type checks; selected native executions retain the physical
inverse checks. Unbound control cannot be exported as a flat circuit. See the
[CPU join contract](c11-cpu-control.md) for precise lifetime and validation boundaries.

Structural branch checking does not force its arithmetic to execute. Domain evaluation preserves C
short-circuit semantics: division in an unselected branch is not executed.

The updated frontend emits `signed` in every `qalloc`. For compatibility, IR v1 `qalloc` without
that field retains unspecified signedness: width is checked without inventing a C type identity
absent from the document. Validation does not reconstruct GCC's complete type system from JSON.

## Engine-maintained range proofs

Independently of expression metadata, the engine maintains:

- A known basis value when provable.
- A mask of bits proven zero.
- An intact uniform preparation and its upper bound.

A `reg` node with `known` is accepted only if it matches the engine's proven value. Reduced `active`
is accepted only if omitted bits are proven zero.

Preparation establishes these proofs. Updates refresh them for known basis values or conservatively
discard them. Reset establishes zero. Correct uncomputation restores the destination to zero.
Measurement preserves proven zero bits but invalidates the uniform preparation required by `find`.

Phases and operations on derived results invalidate input uniformity where appropriate. This
prevents a document from reusing a modified register, or one affected by measurement of a related
result, as an intact Grover input.

## Uncomputation certificate

Every `qcompute` records:

1. The typed creating expression.
1. The external control mask and pattern.
1. The identities and versions of all inputs and controls.

`quncompute` requires the same expression, normalized control, and available dependencies with
matching versions. Reordering controls does not change their mask or pattern.

The current representation compares the creating expression's JSON text; an equivalent expression
with another serialization may be rejected. This is a conservative restriction rather than a general
equivalence proof.

While a result needs an input or control for its inverse, the engine rejects changing, measuring,
resetting, or releasing that dependency. Operations changing basis values or lifetimes increment the
version.

A diagonal phase on an input or control may preserve the inverse of a Boolean computation: it does
not change the basis values used by that computation. Uniformity for `find` is invalidated, but the
phase is not treated as a value overwrite. Tests verify that relative phase survives controlled
uncomputation.

After the inverse, the destination is marked zero and the certificate is removed. Simulation also
requires probability outside zero below `1e-10` before reuse. This numerical check complements the
proof; export does not depend on simulated amplitudes.

## Structured operations and CPU arguments

`qadd_register` preserves an unsigned source and adds/subtracts modularly into a distinct
destination. `qxor_inplace` requires an operand independent of the destination, represented as a
typed expression or CPU value. `qfourier` accepts controls disjoint from the transformed register,
rejects pending inverses, and discards value/uniformity proofs.

`qprepare` with `dynamic: true` receives a C-converted value through the integer argument API. It
requires fresh clean storage and cannot also specify static bounds. Dynamic `qphase` receives a
finite `double` through a dedicated API, separate from integer addition, XOR, or preparation
arguments. Validation retains abstract effects without inventing values; export and document
simulation without actual arguments are rejected.

Symbolic bit and predicate synthesis is independent of the 12-bit table limit. All operands are
checked before emitting gates; dependencies and versions are certified even when enumeration is
unnecessary. Defensive term limits do not prove that every expression admits scalable synthesis.

`qindexed` declares candidate identities and an action on the first candidate. The verifier requires
unique identities, prepared registers with matching width/signedness, no pending inverses, and a
supported action without coherent controls: CPU-argument addition/XOR, measurement, reset, or
Fourier. `qc_apply_index` checks the index and applies the action only to the selected register.
Validation without an actual index checks every candidate and discards value, uniformity, and
cleanliness proofs: a conditional reset does not certify zero for all elements. CPU selection cannot
become a static circuit.

## Scope of the guarantee

This verifier checks IR operations and dependencies along the processed sequence. It is not a formal
compiler proof or a complete analysis of all CPU paths.

CPU branch and loop markers are structurally validated but lack a complete control-flow graph with
conditions and every route. Hybrid execution and static export retain their documented limits.

The frontend checks static moves and borrows in expanded functions, including aliases and read-only
access. General cross-module rules, dynamic effects/lifetimes, and native propagation of `quant` in
GCC need further work. The frontend controls `qmove` semantics between names; the register identity
API does not reconstruct ownership for every C name.

Tests use public APIs in export and simulation contexts to check invalid controls, live
dependencies, false metadata, types, arities, phases, and cleanup. They are not a complete fuzzing
campaign or formal proof.

## Circuits, search, and built-in budgets

`qborrow` declares a module parameter with unknown range and fixed representation. `qmodule` remaps
identities and controls or inverts a unitary body. Aliases, incompatible representations,
CPU/nonunitary effects, invalidated dependencies, and private workspace not released cleanly are
rejected. Parameters precede the body; nested modules are limited to 16 levels.

`qfind` may declare `solutions`; omission promises one solution. The number is validated as a
contract rather than by a domain census. `qsearch` requires an attempt budget, verifies measured
candidates, and cannot be exported statically. IR validation checks the oracle without knowing the
search strategy's outcome.

`sequence` preserves evaluation of pure expressions discarded by GCC. The verifier limits depth to
128 and expression tokens to 100,000; symbolic synthesis includes term comparisons in its
two-million-step budget. Arithmetic circuits record a plan and reverse its gates to return workspace
to zero.

Caches do not replace per-call verification. Keys retain the expression, proven support, wire
representation, and controls; a plan's private workspace must not overlap live registers. The format
remains version 1 with additional accepted operations; older tools must explicitly reject
unsupported operations.

`qobserve` shares the measurement fields `op`, `id`, optional `width` and optional `result`. It
requires a saved computed definition with unchanged live input epochs. It preserves that definition
through the explicit channel so a following checked inverse can clean the temporary. It is rejected
in unit-only modules and does not permit coherent controls. Predicate observations preserve
coherence inside each function fiber; source inputs are not additionally measured or reset. See
[integer observations](c11-observations.md).
