# Versioned assignments and retained quantum history

This advances M2/M4. Complete C11 expressions, dynamic control, memory and library coverage remain
open. The implemented paths use quantum registers and gates; they do not evaluate unknown quantum
values on the CPU.

## Assignment contract

An assignment that cannot safely update its old register may compute into a fresh register and
transfer the source-level owner to that new version. The previous register remains available to old
dependent values. For a computational-basis function `f`, the embedding is `|x>|0> -> |x>|f(x)>`,
extended linearly to arbitrary superpositions. It preserves relative phases and information in the
complete joint state. The old register is retained history, not a cloned unknown quantum state.

The compiler implements this for unconditional and structured coherent scalar assignments and
statically resolved array, structure and pointer-borrow aliases. Covered paths include constant
overwrites; division and remainder; shifts; masks; even multiplication; and updates of derived
values or values with live old dependents. Existing safe in-place addition/XOR and certified word
transformations keep their separate paths. Some Boolean updates use versions even when a smaller
in-place circuit would be possible; that is an optimization obligation.

[Value-producing updates](c11-effect-values.md) retain old reads across an effect and return the
appropriate quantum old/new value. Their pending-read protection participates in the same history
and cleanup rules. Supported integer expression observations have a
[channel contract](c11-observations.md). Mixed CPU effects and general memory/float observations
remain open.

Pure CPU integer inputs specialize the new computation through the binding contract. Its concrete
definition retains the original values for its inverse. Generic effectful operands still require
sequenced capture. When GCC has already narrowed a signed multiplication and erased its promoted
definedness information, the compiler conservatively rejects that case; this is **P** qualification
and promotion preservation, not physical incompatibility. A CPU divisor remains supported with
concrete numerical checks after binding.

A clean zero destination can compute a value into its existing storage when it does not depend on
that destination. Copying a computational-basis value can entangle the destination and source; it
does not duplicate an arbitrary quantum state. Fresh direct-name initializers retain the separate
ownership-transfer contract and its current GCC-folding limitations.

## Lifetime, cleanup and resources

Each retained version keeps its own register identifier and its original inverse dependencies.
Checked aliases continue to resolve the current source-level owner. The `qretain` IR instruction
marks history without changing its gates, value or epoch. This allows an old dependent computation
to finish or invert after an overwrite of its source-level variable.

Derived history is uncomputed when all of its uses and dependents have ended and its inputs remain
available. For a root history value proven to be in a known basis state, `qhistory_end` restores it
to zero with X gates and releases the clean wires. Unknown root history remains allocated until the
quantum context ends and remains part of the circuit's joint output. Ending its source-level
lifetime does not emit a measurement, reset or partial trace. A source-level history handle and
explicit ownership transfer/discharge API remain **P**.

For an overwritten word of width `w`, the old version retains `w` qubits and the new value requires
fresh storage. Chains and live dependent results can retain several versions. Synthesis work has its
own additional cost. Resource reports include current and peak history qubits, already counted in
the logical/ancillary totals. Known-basis cleanup may reduce the current history while leaving the
recorded peak unchanged.

The backend represents at most 128 physical qubits and the native statevector at most 24. A
table-free circuit can require more private work than the bounded table route, so passing a native
execution does not certify the same program's table-free resource budget. Wider generation is
reported separately from complete-state execution. Histories are never discarded to fit these
limits; failures are explicit **R** resource contracts.

Structured coherent branch versions follow the [selected-value contract](c11-control.md), including
escaping predicate lifetimes. [Structured CPU scalar joins](c11-cpu-control.md) transfer the
selected owner without physical copying and clean actual saved inverses when their dependents
finish. General CFG and runtime loop iterations still need compatible versions and history
ownership. Structured runtime iterations now have a
[native version contract](c11-runtime-history.md); general CFG edges remain **P/R** with specific
diagnostics. These are not semantic prohibitions on C conditionals or loops. Full aliases, dynamic
allocation, escaped pointers, unions and byte layouts remain separate M4/M5 obligations.

## Measurement ordering regression

The compiler must preserve a pending measurement's inputs until that measurement instruction has
been emitted. Treating the output as consumed too early can schedule an inverse of its input ahead
of the output measurement. The published compiler rejected this valid chain:

```c
#include <quantum.h>

int main(void) {
    quant bool n = superposition(0, 1);
    quant uint8_t a = (unsigned)n + 3;
    quant uint8_t b = a * 2;
    unsigned result = measure(b), input = measure(n);
    return result != 2 * (input + 3);
}
```

The reproduction with the published `50ea73b` compiler failed with an inverse-dependency diagnostic.
The corrected compiler emits the measurement before any affected input cleanup and executes the same
source successfully. Measurement and reset keep their explicit source semantics; the fix does not
add either channel.

## Validation

The history suite separates:

- 78 complete phased domains over synthetic 2/3/4-bit words, signed and unsigned arithmetic, both
  table policies, a live old dependent, and clean derived inverses. These do not certify C
  bitfields.
- Five lifetime/schema/proof contracts, including known-basis restoration, safe wire reuse, live
  inverse protection, rejection before gates and predicate-cache invalidation.
- 75 native C integrations at five GCC levels: assignments, CPU snapshots, fixed arrays and pointer
  aliases, mutable quantum procedures and the measurement-chain regression. Their measurements are
  explicit. Combined even affine and self-dependent XOR expressions select history; the odd affine
  combination keeps its existing storage, checked in emitted IR.
- 30 complete states from compiled C programs at five GCC levels and both synthesis policies:
  cleanup of derived versions, unknown retained outputs and a CPU snapshot used by an inverse after
  the CPU variable changes. Terminal measurements are ignored only for these coherent state
  comparisons; their counts are checked separately.
- 110 coherent Boolean states and 30 coherent word states, with phases, saved branch decisions,
  predicate cleanup and fixed aliases. Their precise scope is in [c11-control.md](c11-control.md).
- 46 CPU owner contracts, 55 compiled CPU branch programs, 120 native paths and 240 complete
  selected-trace states. Their scope, eight-bit basis restrictions and structural/native validation
  distinction are in [c11-cpu-control.md](c11-cpu-control.md).
- 20 specific pending-contract rejections for controlled declarations/quantum loop conditions, CPU
  branches under coherent control and erased multiplication promotions. Infrastructure faults cannot
  count as semantic rejections.

```bash
export CC=gcc-11 CXX=g++-11
bash scripts/build.sh
build/validate_history
bash scripts/verify.sh
```

The current [campaign archive](audit-evidence/README.md) identifies completed verification by source
hash. Passing this finite stage does not close M2, M4, the C11 inventory or the full project goal.
