# Coherent scalar assignments and control joins

This advances M2/M3/M4 toward full C11 coverage. Structured coherent `if` statements can update
supported scalar owners, including fixed array/field/pointer aliases and mutable same-unit quantum
procedures. General CFG, runtime CPU versions, dynamic memory, ABI, floating/complex and library
contracts remain required.

## Selected values and preserved branch decisions

An injective update with available inputs may use its existing controlled in-place route. Otherwise
the compiler retains the old version and computes a fresh owner. For a branch predicate `p`, old
value `x` and active transformation `f`, its value is `p ? f(x) : x`. The complete embedding
preserves the old inputs and extends linearly to superpositions. An inactive branch preserves the
previous value; it does not prepare zero as a replacement.

The compiler serializes this value as typed `select` expressions. Nested controls require all
enclosing predicates. Open controls select the opposite arm. `if/else` bodies use successive
versions with the same saved branch decision, so an assignment to a condition variable does not
change which arm executes. Each predicate undergoes C truth conversion.

```c
#include <quantum.h>

int main(void) {
    quant bool condition = superposition(0, 1);
    quant bool value = superposition(0, 1);
    if (condition) {
        value = false;
        condition = !condition;
    } else {
        value = !value;
    }
    measure(value);
    measure(condition);
}
```

These are gate computations on registers. No unknown quantum value or branch is evaluated on the
CPU. There is no inserted measurement, reset or partial trace. The explicit terminal measurements in
the example keep their source semantics.

## Predicate lifetime and CPU captures

A predicate needed by an escaping version remains a derived history register. Its saved input
versions and CPU values remain available until the dependent results finish or invert. Then the
predicate is uncomputed and its clean wires are released. Unknown root history follows the
[retained-history contract](c11-history.md); known basis roots can be restored unitarily. Current
and peak history costs include escaping predicates.

Pure CPU integers are captured by the predicate computation, rather than its storage allocation. For
example, `if (n + a)` must apply the captured `a` to the arithmetic predicate. The compiler at
`22113c9` instead attached its binding to `qalloc` and rejected the program with an
unused-descriptor diagnostic. The regression now executes successfully. Saved inverse definitions
use their original captured values after a later CPU mutation. Effectful or volatile CPU operations
still need their own sequencing and interface contracts.

Explicit measurement/reset of an input needed by a live inverse still requires resolving or
transferring that inverse obligation. Source-level dirty-history handles and general channel/CFG
cleanup remain P; these restrictions do not constitute a prohibition on C control flow.

## Definedness, correlations and synthesis

A nested predicate is zero outside its enclosing active controls. C arithmetic only needs to be
defined on the branches where it executes. Range proofs can identify an unconditional, saved Boolean
identity or complement through truth-preserving casts, logical negation and equality with zero. The
definition's inputs must be alive at the recorded epochs. Partially controlled definitions and
general Boolean functions do not receive that certificate.

The arithmetic route uses these relations in branch facts. The bounded table route filters rows
inconsistent with a certified Boolean relation when its canonical source is also in the domain. It
assigns zero on those unreachable rows, defining a reversible extension without evaluating invalid C
arithmetic there. It neither samples branches nor builds a table in `--no-tables` mode. The
certificate is conservative; stronger correlations remain P/R.

Plan keys include source epochs, so changing a predicate cannot reuse a circuit justified by its
earlier definition. A regression changes an identity predicate into its complement: an earlier
inactive division becomes reachable and must fail before output gates or a new definition.

The Boolean proof has a depth bound of 128 and shares the arithmetic work budget. Circuit storage
remains bounded to 128 qubits and native state simulation to 24. Versions, predicates and work have
separate costs. The eight-bit one-arm fixture generates a table-free circuit with 21 qubits; that
resource report alone is not a correctness proof. Wider generation and complete states must continue
to be reported separately.

## Validation scope

The coherent additions in `tests/test_coherent_history.inc` distinguish:

- 110 complete Boolean joint states: 11 C profiles at five GCC optimization levels and both table
  policies. These cover one/two arms, self controls, nesting, CPU snapshots, inactive arithmetic,
  mutable functions and successive joins. Relative phases and every private wire are compared to
  explicit value references; source measurement counts are checked separately.
- 30 complete word states: signed/unsigned eight-bit results and fixed array/pointer aliases with
  known basis word inputs, at five levels and both policies. These do not exhaust arbitrary
  eight-bit superpositions or certify bitfields.
- A predicate-cache regression with a reachable invalid branch after a definition change.
- Four further table-free circuits compared with pinned QuEST: two-arm versions, self-controlled
  information loss, nested versions and a CPU predicate snapshot used by cleanup.

```bash
export CC=gcc-11 CXX=g++-11
bash scripts/build.sh
build/validate_history
bash scripts/build-validation.sh
build/validate_qasm
```

Completed campaign counts and hashes are recorded in the
[evidence archive](audit-evidence/README.md). CPU branch joins and runtime loop iterations still
need a version/ownership model. Declarations inside controlled flow, products, dynamic indexed
writes, `switch`, jumps and early returns remain separate P/R obligations. This finite stage does
not close M3 or the full C11 objective.
