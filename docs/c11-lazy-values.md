# Selected effects in conditional and logical expressions

Supported integer scalar effects can now produce values inside conditional selection and logical
short circuits. This advances M2/M3 in the [complete C11 roadmap](c11-roadmap.md); it does not
complete C11, the function/memory model or the standard library.

The [pending-value correction](paper/lazy_values_note.es.md) transports serialized operands across
later selected effects and defers ancestors whose runtime inverse may survive an inactive arm. It
also updates the old state references for stable destination identities. Historical campaign results
remain tied to their original sources; this change does not certify the full control model.

## Evaluation and quantum meaning

The condition is evaluated first, including its supported ordered effects. Its C truth value is
computed into a saved decision register. Conditional selection executes the first arm under that
decision and the other arm under its complement. Logical AND executes its right operand only when
the saved left value is true; logical OR executes it only when that value is false. Their result
remains the C integer zero or one, with the original conversions preserved.

Each supported basis input follows its selected C evaluation. The transformation extends linearly to
superpositions, retaining prior values when an overwrite loses information. A branch that does not
execute preserves its storage value. Reassigning the condition inside an arm does not alter which
arm was selected. Nested decisions include their enclosing controls.

```c
#include <quantum.h>

int main(void) {
    quant bool condition = superposition(0, 1);
    quant bool value = superposition(0, 1);
    quant bool previous = condition ? value++ : value--;
    measure(previous);
    measure(value);
    measure(condition);
}
```

Decisions and versions have unconditional storage lifetimes and selected value transformations.
Their saved input values remain available until dependent results finish or invert. Checked
uncomputation then clears decisions and intermediate derived history. Unknown root history follows
the [joint-output lifetime contract](c11-history.md). No measurement, reset or trace implements
selection, overwriting or cleanup. Explicit observations keep their
[channel contract](c11-observations.md).

Pure CPU conditions and operands use once-only bindings. Their saved inverse definitions retain the
captured values after later CPU mutations. This does not authorize a CPU side effect to execute on a
superposed branch. CPU effects and volatile reads in these expressions remain P.

## Definedness, resources and remaining work

Arithmetic must be defined where its arm executes. A saved Boolean decision with a fixed truth value
can certify an inactive arm, even when its original source is absent from the immediate arithmetic
domain. Certificates inspect known basis roots, literal definitions, truth-preserving casts,
complement and selected nested decisions. Recorded input epochs must match; partially controlled
definitions do not receive an unconditional certificate.

The bounded table route excludes certified unreachable rows. The arithmetic route propagates the
same fixed truth values without a table. The known-false condition regression no longer evaluates an
inactive division by zero. Reachable division by zero still rejects before its output circuit is
emitted. This remains a conservative proof system; arbitrary correlations and narrowing conversion
chains require further analysis P.

Versions, decisions, synthesis workspace and the physical backend representation (128 qubits in
baseline `5c5a965`; 1024 in Wave 1A engine `QC_MAX_PHYSICAL`, pending Wave 1B/2)/24-qubit native
dense simulator limits have explicit resource costs R. Static loop expansion and supported same-unit
procedures compose with these expressions. Native runtime iterations now use the
[per-iteration history contract](c11-runtime-history.md). General indexed reads/products, effectful
return ABI, frontend/ABI floating/complex encodings (beyond Wave 1A engine circuits) and full
source-level GCC sequencing remain P/R. Undefined operations on a reachable branch have no defined C
result; conservative rejection of an unproved inactive branch must remain distinct from that
semantic incompatibility.

## Finite evidence

`tests/test_lazy_values.inc` adds 573 checks to the history suite:

- 320 basis executions against ordinary C references at five GCC optimization levels.
- 65 combinations of CPU bindings, fixed memory aliases, same-unit functions, nested coherent
  control, static loops, inactive arithmetic, signed/unsigned words and explicit observation.
- 160 phased domains at five GCC levels and both synthesis policies, with independent C
  values/phases/history channels, complete gate-state comparisons, independent unitary inverses,
  exact source observations and private zeros. The history encoding need not use the old root
  identifier as a separate register.
- 15 diagnostics for reachable undefined operations and volatile CPU effects, plus five native
  executions checking supported lazy results inside runtime CPU loops.
- Eight direct fixed-predicate contracts, including stale epochs, partial controls, phase-preserving
  inverses and rejection before state/gate mutation.

Three additional table-free QuEST state fixtures contrast conditional updates, nested short circuits
and a reassigned condition. Completed full-campaign results and exact source identity belong to the
[evidence archive](audit-evidence/README.md). These finite checks are not a proof of every C11
program.
