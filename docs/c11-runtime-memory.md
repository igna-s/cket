# Runtime lengths for fixed quantum buffers

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

The [delivery evidence](audit-evidence/runtime-memory-v1/README.es.md) records source identity and
separates complete complex states/inverses, native observations, boundary rejections and preflight
state preservation. It does not certify arbitrary states or complete C11. Retained history consumes
the 128-wire representation budget; dense simulation supports 24 wires and sparse mode has bounded
occupied support.

Unbound counts require CPU parameter bindings. Runtime branches require native execution and cannot
be flattened into QASM. Borrowed unitary modules reject `cmemory` until their ABI includes such a
precondition. No physical-QPU execution or speedup is claimed.
