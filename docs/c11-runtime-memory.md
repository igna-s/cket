# Runtime lengths for fixed quantum buffers

**Current finite correction:** [1.0.0-rc.3](audit-evidence/fixed-patches-v1/README.es.md) preserves
untouched fixed-word bits in the published counterexample. CPU masks, root histories, producer
cleanup and scoped old-value phases have explicit finite density/state/inverse evidence. General
runtime-memory, alias/ABI and all-library qualification remain open.

`memcpy`, `memmove` and `memset` accept pure CPU byte lengths over checked fixed integer storage.
Addresses and object extents remain statically checked. A call selects at most 64 bytes and fails
before destination writes if its count exceeds either object or that synthesis bound.

```c
static void copy_prefix(quant uint8_t *destination, quant const uint8_t *source, size_t length) {
    memcpy((void *)destination, (const void *)source, length);
}
```

The [complete example](../examples/blocks/05/buffer_lengths.c) receives its length through `argv`,
passes it to a function, copies a prefix, moves overlapping bytes and fills a selected prefix. It
observes results explicitly and compares them with C formulas. Run `bash scripts/run-block.sh 05`.

## Preflight, bytes and history

`cmemory` takes a typed unsigned 64-bit bound CPU count, a byte limit from 0 to 64, an optional
overlap threshold and Boolean controls. Schemas are checked even on inactive paths. A proven
inactive coherent control skips range/domain checks; a reachable invalid count fails before
captures, writes or enabled CPU source reads. The preflight adds no gates, allocations or channels.

`memcpy` checks the first prefix that would overlap. `memmove` instead schedules original reads
before any word overwrites them. Unsupported cyclic word dependencies remain rejected. Each selected
word is computed from real register expressions. Other bytes preserve their value; whole unselected
words and zero-length copies preserve their physical wires. Same-range `memmove` validates its
length and otherwise remains an identity.

Runtime writes keep stable public owners across nested CPU branches and loops. Saved inverses follow
private retired identities, avoiding an owner consumed by the other arm during template validation.
Unknown roots remain joint outputs; history is inverted only after borrowers finish. There is no
hidden measurement, reset or partial trace, nor independent cloning of an unknown state.

`memset` captures its fill once before changing any word, even for zero count. Compiler-private
captures have optional CPU branch lifetimes and fresh iteration slots. User quantum declarations
inside arbitrary control still require their pending lifetime contract. Supported CPU fill effects
remain outside coherent control. Effectful/volatile counts need a further access/sequencing
contract.

Known nonvolatile CPU source objects provide only enabled snapshots. Zero bytes do not terminate the
copy. Quantum host placeholders are never treated as CPU source payload.

## Returned addresses

Checked local destination borrows remain supported. A direct compatible read such as
`measure(*(uint8_t *)memcpy(destination, source, length))` sequences the effect before observing the
actual destination register. The same CPU read without `measure` is rejected. Returned addresses
cannot escape to unknown CPU calls or aggregates. Direct returned-expression writes require a
checked local borrow. This closes a bug where a direct read could return a placeholder byte.

Conditional returned-pointer data access, general pointer/indirect ABI, runtime addresses, padding,
floating layouts and quantum-to-CPU writes remain pending. Ordinary CPU-only calls retain libc.
Cybersecurity, cryptography and authentication stay classical.

## Finite evidence and resources

The [later quantum audit](re-audit-2026-10-24-coherence.md) found that selecting the previous value
into another public register could dephase an inactive unknown destination. The direct fixed-store
route now computes the active value into zero storage and exchanges it under the quantum controls.
History is zero on inactive branches. Saved GCC expressions use a conditional old-owner view.
Computed borrows whose inverses would read changed wires are retained as roots; their general scope
cleanup remains an open quantum-correctness issue when the write can activate. A
[later finite correction](audit-evidence/inactive-borrows-v1/README.es.md) keeps inverse recipes
when saved typed certificates prove the write globally inactive; those scoped temporaries clean with
the expected phase kickback. Heap identity writes now have a bounded symbolic proof and
[directed evidence](audit-evidence/heap-identities-v1/README.es.md); coherent nonidentity heap
copies still have an open coherence finding. Passing the finite campaigns below does not close the
remaining findings.

The [delivery evidence](audit-evidence/runtime-memory-v1/README.es.md) records source identity and
separates complete complex states/inverses, native observations, boundary rejections and preflight
state preservation. It does not certify arbitrary states or complete C11. Retained history consumes
the 128-wire representation budget; dense simulation supports 24 wires and sparse mode has bounded
occupied support.

Unbound counts require CPU parameter bindings. Runtime branches require native execution and cannot
be flattened into QASM. Borrowed unitary modules reject `cmemory` until their ABI includes such a
precondition. No physical-QPU execution or speedup is claimed.

## Conditional inverse transport in 1.0.0-rc.1

The [later scoped-borrow correction](audit-evidence/rebound-borrows-v1/README.es.md) supersedes the
earlier failing possible-active root-overwrite examples. Saved inverses refer to the conditional
location of each old input after a controlled swap. Reconstructible private decisions are expanded
from stable roots so their physical copies can clean while declared borrowers remain live. Epochs,
control masks, acyclicity and proof budgets are checked before the exchange. Complete complex states
are checked before/during/after cleanup, including negative signed values and phase kickback. This
finite correction does not qualify general repeated frontend writes, derived destinations,
correlated range proofs, 64-bit scope cleanup or nonidentity coherent heap copies. The latter still
has a reproduced inactive-sector coherence failure. See the
[candidate scope](releases/v1.0.0-rc.1.es.md).
