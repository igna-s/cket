# Pure integer expressions: semantics and evidence

This is an implemented step toward [complete C11 coverage](c11-roadmap.md), not completion of
integer expressions, control flow or the standard library.

## Operations and definedness

The structured, table-free synthesizer supports preserved-input integer negation, bitwise
complement, truth conversion, logical AND/OR and scalar conditional selection. GCC promotions,
signedness and conversions are retained in typed IR. Unsigned negation reduces modulo the declared
width. Signed negation requires exclusion of the minimum value of its expression type. Bitwise
complement operates on the declared integer representation; conversion to `_Bool` normalizes any
nonzero value to one.

Logical short circuiting and `?:` analyze arithmetic only on the support of the arm that C selects.
For example, `x ? 3 / x : y` can be defined when `x` includes zero: the division arm must prove
`x != 0`. A division reachable with a zero divisor is rejected. No measurement chooses an arm.

The proof carries zero/nonzero facts, negated predicates, safe truth-preserving casts, conjunctions
known true, disjunctions known false, and all six integer comparisons against a literal on either
side. Ordered comparisons retain the exact typed expression; changing signedness can change an
inequality. Narrowing casts cannot propagate a nonzero proof: a nonzero four-bit value can become
zero after conversion to two bits.

Equivalent pure expression trees can share a fact even when they occupy different JSON nodes.
Equality includes operation, type, literal/register identity and operands. It is not algebraic
equivalence or a general solver. Proved constant operands/results avoid unnecessary arithmetic
workspace, including a divider inside a branch whose operands are both constant.

## Reversible implementation and resource contracts

Inputs are preserved. Bitwise complement uses inverted bit views; negation uses a reversible
copy/negate plan. Truth tests and equality/inequality against integer literals use constrained gates
without a full-width difference register. The latter covers zero, `UINT64_MAX` and the high-bit
boundary, including constant-left comparisons.

Pure branch computations have total reversible circuit extensions on inactive data. Only the
selected arm contributes to the destination, and the recorded private plan is inverted afterwards.
An inactive extension is not a C evaluation of undefined arithmetic or permission to perform an
inactive effect. This implementation does not introduce measurements or resets to discard branch
information or intermediate values.

Each arithmetic plan limits proof/synthesis work to two million steps, recursive depth to 128, facts
to 128 per branch, additional branch-range storage to 16 MiB, and temporary register records to 512.
The backend's 128-qubit representation and the native simulator's 24-qubit limit also apply.
Exceeding a budget is an explicit resource restriction (**R**).

## Finite validation

`tests/validate_expressions.c` runs **97 checks** in each configuration:

- Two regressions for legacy signed basis metadata, across the two synthesis configurations. A
  prepared signed eight-bit value with bit pattern 249 must be analyzed as -7 when the expression
  supplies its signed interpretation.
- Sixty-eight complete joint-state comparisons: 34 expression combinations, with finite synthesis
  enabled/disabled, relative phase, preserved inputs, zero private workspace and a checked inverse.
  Cases include signed/unsigned unary operations, nested guarded division, Boolean normalization,
  every literal relation in either order, signed-minimum exclusion and guarded shifts.
- Six rejection checks for reachable undefined arithmetic, including signed minimum negation and an
  invalid nonzero proof through a narrowing cast.
- Six 64-bit truth/literal-comparison resource checks. These generate circuits; their complete
  64-bit states are not simulated or independently validated.
- Five C executions combining a typed function, fixed array, static loop, conversions, unary
  operations and guarded expressions, with ordinary-C result comparisons at `-O0`, `-O1`, `-O2`,
  `-O3` and `-Os`.
- Ten table-free frontend exports at those optimization levels: guarded 16-bit division and a
  combined 64-bit truth/complement predicate. Circuit generation does not establish correctness
  across those entire large domains.

The separate QASM campaign adds three small complete-state comparisons with QuEST: guarded
conditional division, short-circuit division and conditional signed negation/complement, each with
relative phase. Its current suite comprises 22 states and three sampled measurement/reset channels.
These comparisons use an independent QASM reader and simulator; the ordinary-C references above
separately check language results.

Run `CC=gcc-11 CXX=g++-11 bash scripts/verify.sh` for normal and ASan/UBSan checks. The
[current archive](audit-evidence/README.md) identifies completed campaigns and their source hashes.
The papers retain their distinct historical campaign.

## Remaining work

Expression-valued assignments, increments with old/new results, sequenced effects, aggregate
selection and destructive history are pending (**P**). Nonlinear predicates, general correlated
range reasoning and facts passed through arbitrary derived registers require stronger proof
machinery (**P**). The tested GCC version and finite optimization corpus do not establish that all
transformations before the plugin preserve quantum qualification or expose all source-level
undefined behavior. Signed/unsigned combinations, wider arithmetic workspace and general library
adapters remain open.

Discarding distinguishable quantum values without retained information or an explicit channel is an
incompatible context (**S**). That does not make C assignment, logical operators or conditional
expressions inherently impossible.
