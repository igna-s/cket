# Coherent scalar assignments and control joins

**Current finite follow-ups:** [RC2 heap coherence](audit-evidence/heap-coherence-v1/README.es.md)
and [RC3 fixed buffers](audit-evidence/fixed-patches-v1/README.es.md) correct the published heap and
untouched fixed-byte counterexamples. The later
[string-write block](audit-evidence/string-writes-v1/README.es.md) covers a separate finite scope.
Earlier results retain their source identities. General lifetimes/alias/ABI and large profiles
remain open.

This advances M2/M3/M4 toward full C11 coverage. Structured coherent `if` statements can update
supported scalar owners, including fixed array/field/pointer aliases and mutable same-unit quantum
procedures. [Structured CPU scalar versions](c11-cpu-control.md) have a separate ownership transport
contract. [Runtime iteration history](c11-runtime-history.md) and [native heaps](c11-heap.md) now
cover selected integer/memory paths. General CFG ownership, further dynamic-memory/ABI integration,
frontend/ABI floating/complex values (beyond Wave 1A engine `float`/`double` circuits) and wider
library contracts remain required.

## Selected values and preserved branch decisions

An injective update with available inputs may use its existing controlled in-place route. Direct
fixed overwrites otherwise compute the active value into zero storage and exchange it with the
public owner under the controls. The history is zero on inactive amplitudes and holds the old value
where the write occurs. The value formula `p ? f(x) : x` alone does not specify the required quantum
correlations: computing it into another register while retaining `x` can dephase an inactive
destination.

Typed `select` expressions describe the selected values. Nested controls require all enclosing
predicates. Open controls select the opposite arm. `if/else` bodies use successive versions with the
same saved branch decision, so an assignment to a condition variable does not change which arm
executes. Each predicate undergoes C truth conversion.

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

The [quantum audit](re-audit-2026-10-24-coherence.md) records the earlier failures and their later
scoped-borrow, heap and fixed-byte corrections. Those finite results do not certify every controlled
program.

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
general Boolean functions do not receive that alias certificate. A separate bounded polynomial proof
can now infer a Boolean branch fact from a saved producer with valid input epochs, including its
recorded control mask. It reasons about zero/one values; the low bit of an arbitrary integer does
not determine C truth. Neither proof reads amplitudes.

The arithmetic route uses these relations in branch facts. The bounded table route filters rows
inconsistent with a certified Boolean relation when its canonical source is also in the domain. It
assigns zero on those unreachable rows, defining a reversible extension without evaluating invalid C
arithmetic there. It neither samples branches nor builds a table in `--no-tables` mode. The
certificate is conservative; stronger correlations remain P/R.

Plan keys include source epochs, so changing a predicate cannot reuse a circuit justified by its
earlier definition. A regression changes an identity predicate into its complement: an earlier
inactive division becomes reachable and must fail before output gates or a new definition.

The Boolean proof has a depth bound of 128 and shares the arithmetic work budget. Circuit storage
remains bounded to 128 qubits in baseline `5c5a965` (expanded to 1024 in Wave 1A engine
`QC_MAX_PHYSICAL`, pending Wave 1B/2) and native dense simulation to 24. Versions, predicates and
work have separate costs. The eight-bit one-arm fixture generates a table-free circuit with 21
qubits; that resource report alone is not a correctness proof. Wider generation and complete states
must continue to be reported separately.

## Validation scope

The current [reference correction](paper/lazy_values_note.es.md) passes the normal history suite and
focused instrumented checks. The SDK comparisons listed below retain their historical source
identities; they are not a new independent SDK qualification of this follow-up.

The coherent additions in `tests/test_coherent_history.inc` distinguish:

- 110 Boolean domains: 11 C profiles at five GCC optimization levels and both table policies. These
  cover one/two arms, self controls, nesting, CPU snapshots, inactive arithmetic, mutable functions
  and successive joins. Relative phases and every private wire are compared to C
  value/phase/history-channel references and independent gate trajectories. The complete physical
  state is checked, including private zeros, then the unitary gate sequence is inverted
  independently. Source measurement counts are checked separately.
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
[evidence archive](audit-evidence/README.md). General CFG and runtime loop iterations still need
further version/ownership support. Declarations inside controlled flow, dynamic indexed overwrites,
`goto`/unstructured jumps, and unverified arbitrary early returns (`5c5a965` adds structured
`switch` and same-unit imperative owned returns) remain separate P/R obligations. This finite stage
does not close M3 or the full C11 objective.

When typed saved certificates prove a write globally inactive, materialization keeps existing
inverse definitions and the inactive swap leaves owners/epochs unchanged. This permits checked
scoped temporaries to uncompute with their expected phase kickback. The
[finite inactive-borrow evidence](audit-evidence/inactive-borrows-v1/README.es.md) covers selected
bool/8/16/32-bit assignments and one-byte writes. A control in superposition can still require a
conditional old-owner inverse; that archived scope case is corrected in the later
conditional-inverse follow-up below.

## Conditional inverse transport in 1.0.0-rc.1

The [later scoped-borrow correction](audit-evidence/rebound-borrows-v1/README.es.md) supersedes the
earlier failing possible-active root-overwrite examples. Saved inverses refer to the conditional
location of each old input after a controlled swap. Reconstructible private decisions are expanded
from stable roots so their physical copies can clean while declared borrowers remain live. Epochs,
control masks, acyclicity and proof budgets are checked before the exchange. Complete complex states
are checked before/during/after cleanup, including negative signed values and phase kickback. This
finite correction does not qualify general repeated frontend writes, derived destinations,
correlated range proofs, 64-bit scope cleanup or all coherent heap copies. The later RC2 heap
correction closes the specific reproduced inactive-sector failure. See the
[candidate scope](releases/v1.0.0-rc.1.es.md).
