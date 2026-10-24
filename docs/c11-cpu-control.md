# Classical branches with quantum scalar versions

Structured CPU `if`/`else` statements can join supported quantum scalar owners after destructive
assignments. The condition is a classical Boolean evaluated once using ordinary C sequencing. Only
the selected arm executes. This extends M2/M3/M4; it does not complete general C11 control flow.

```c
#include <quantum.h>

int main(int argc, char **argv) {
    int choice = argc > 1 && argv[1][0] == '1';
    quant bool value = superposition(0, 1);
    if (choice) {
        value = false;
    } else {
        value = !value;
    }
    return measure(value) > 1;
}
```

## Ownership at the join

Injective updates keep their supported in-place route. An information-losing update retains the old
value and computes fresh storage under the [history contract](c11-history.md). At a CPU join, `qphi`
transfers the selected owner's metadata to a fresh logical identifier. The selected physical wires,
amplitudes, epochs and saved inverse are preserved. The transfer adds no gates or qubits. References
in other saved inverses are relabeled consistently. The transfer allocates CPU metadata and rewrites
saved references, so its classical cost remains even though its physical cost is zero. It does not
copy a quantum state.

This differs from [coherent selection](c11-control.md): a classical decision selects one executed
path; a coherent decision transforms every active amplitude. Recomputing or copying a result merely
to merge CPU owners would create additional entanglement on an otherwise in-place path. Ownership
transport avoids that change.

When the arms produce different scalar identifiers, the frontend currently requires both owners to
be available, unmeasured and compatible in C width and signedness. Transfers that combine measured,
moved or released ownership still need further implementation and lifetime analysis (P/R). This does
not prohibit stable-owner CPU branches with their already supported explicit channels.

The branch decision is captured before storage reservation and reused at the join. Changing the
condition variable inside an arm does not change the chosen owner. Nested CPU branches create some
logical owners only on their executed path. Those owners are retained there; they are never used
before they exist. Fixed scalar array/field/pointer aliases and same-unit mutable scalar procedures
continue to name the joined source-level owner.

In the loop-free branch route, both arms' fresh storage is reserved and prepared cleanly before the
outer CPU dispatch. This keeps allocation identities consistent but incurs the sum of reserved
branch storage, rather than just the executed arm's peak. Unused storage is cleanly released. More
selective reservation is pending implementation; exceeding the 128-qubit circuit or 24-qubit native
simulation budgets is an explicit resource failure. The join itself has zero physical storage cost.

## Cleanup and information lifetime

`qdispose` marks a logical lifetime as finished. A derived value is inverted only after its live
dependents finish, using the actual saved expression, captured CPU values, controls and input epochs
of the executed path. A known basis root can be restored with X gates and released. An unknown root
remains allocated as retained joint output until context end. No measurement, reset or partial trace
is inserted to erase history.

Optional owners that were not created on the executed path need no cleanup. Disposing an absent or
already retired identifier is a no-op; malformed identifiers or fields are rejected. A finished
owner cannot become a new live owner through `qphi`. Inverse dependencies can postpone reclamation;
the associated history cost and resource limits still apply.

General user-visible history capabilities, explicit disposal of dirty storage and ownership across
all channel/exit edges remain P/R. Their absence is an implementation obligation, not a physical
prohibition on C control flow.

## IR and validation boundaries

The serialized template records `ccontrol_begin`, `ccontrol_else` and `ccontrol_end`. `qphi`
contains a fresh identifier, C width/signedness, two distinct source identifiers and a bound
unsigned Boolean condition. Only the selected source must exist and be prepared; known live source
representations must agree with the result. An inactive nested source may be absent or retired.

An unexecuted CPU template cannot certify a single quantum circuit: mutually exclusive arms may have
different inverse dependencies. Templates with CPU owner joins receive structural/type checks;
native execution and concrete selected traces retain the physical checks on actual dependencies and
epochs. Structural acceptance is not a proof of numerical definedness or inverse validity for every
runtime path.

Unresolved CPU dispatch remains rejected by flat QASM export. The finite validation helper in
`tests/select_cpu_trace.inc` selects explicitly supplied, loop-free CPU decisions after binding
parameters. It is test infrastructure, not a general CFG or execution-trace exporter. Borrowed
unitary circuit modules reject CPU ownership joins and disposal until their ABI has such a contract.

## Finite evidence

The history tests separate:

- 46 engine contracts for gate-free transfer, large logical identifiers, relabeled inverses and
  controls, deferred/optional cleanup, retained unknown outputs and malformed IR rejection. Both
  synthesis policies are checked.
- 40 Boolean C programs across five GCC optimization levels, 90 native paths and 180 complete
  selected-trace states. Cases combine nested branches, mutable procedures, saved CPU conditions,
  captured divisors and coherent control inside a CPU arm. Relative phases and every private wire
  are compared with explicit references.
- 15 word programs, 30 native paths and 60 complete selected-trace states across five levels and
  both policies. These use known basis eight-bit words through scalar, array and parameter aliases;
  they do not exhaust arbitrary eight-bit input superpositions.
- Three table-free selected CPU traces added to the independent QuEST state comparisons. They cover
  two-arm assignment, once-only condition capture and coherent control inside a CPU arm.

```bash
export CC=gcc-11 CXX=g++-11
bash scripts/build.sh
build/validate_history
bash scripts/build-validation.sh
build/validate_qasm
```

Completed campaign counts and hashes are recorded in the
[evidence archive](audit-evidence/README.md). Structured iterations now have a
[native history contract](c11-runtime-history.md). Declarations inside controlled flow, products,
dynamic indexed overwrites, `switch`, jumps, early returns, general memory and separate-compilation
ABI remain P/R obligations. CPU effects inside coherent control retain their distinct
semantic/effect requirements. Passing this finite stage does not complete M3 or the full C11
objective.
