# Runtime quantum heaps

**Current finite follow-up:** [RC2 heap coherence](audit-evidence/heap-coherence-v1/README.es.md)
corrects the published nonidentity heap-copy case; the
[RC3 fixed-word correction](audit-evidence/fixed-patches-v1/README.es.md) also closes the published
untouched fixed-byte counterexample. General lifetimes, alias/ABI and large profiles remain open.
Earlier sections retain their source-scoped results.

`malloc`, `calloc`, `realloc` and `free` have a partial native quantum contract for padding-free
scalar integer/Boolean elements. Capacity and addresses are classical; the payload is a collection
of engine-owned quantum registers. The executable never loads quantum values from ordinary host
memory. This capability belongs to the [everyday C profile](perfil-c-cotidiano.es.md); it does not
complete C11 memory support.

```c
#include <quantum.h>
#include <stdlib.h>

void sum(quant const uint8_t *p, size_t count, quant uint8_t total) {
    for (size_t i = 0; i < count; ++i)
        total += p[i];
}

int main(void) {
    size_t count = 2;
    quant uint8_t *p = calloc(count, sizeof *p);
    if (!p)
        return 1;
    p[0] = 3;
    p[1] = 5;
    quant uint8_t *grown = realloc((void *)p, 3 * sizeof *p);
    if (grown) {
        p = grown;
        p[count] = 9;
        ++count;
    }
    quant uint8_t total = 0;
    sum(p, count, total);
    unsigned observed = measure(total);
    free((void *)p);
    return observed != (grown ? 17 : 8);
}
```

The explicit `void *` casts discard the frontend's internal volatile marker at the allocator
boundary. The compiler still checks quantum provenance and type. Sizes must be whole multiples of
the allocated C element size. Ordinary unmarked allocations keep libc behavior. Quantum allocations
currently apply to compatible local pointer declarations and assignments, and expanded same-unit
buffer calls.

## Allocation, resizing and disposal

- Fresh quantum elements start in the known zero state, including `malloc`. This is the quantum
  preparation contract; ordinary C `malloc` remains uninitialized. Growth appends zero elements.
  `calloc` checks multiplication overflow.
- A zero-byte allocation returns `NULL`. `realloc(NULL, bytes)` allocates; `realloc(p, 0)` disposes
  the allocation and returns `NULL` in this target profile.
- A capacity, host-allocation or physical-wire preflight failure returns `NULL`. A failed `realloc`
  preserves the old owner and its values. Callers should assign its result to a temporary before
  replacing their pointer. Internal synthesis or engine failures remain fatal diagnostics, rather
  than recoverable libc errors.
- Growth preserves the identities of existing quantum registers. Shrink and `free` remove the
  corresponding public access paths. They do not measure or reset unknown data. Unknown retired
  states remain joint outputs until context destruction; known basis states are reversibly cleared
  when no saved inverse borrows them.
- Overwriting an element preserves the old version when necessary. A previously computed result
  retains its concrete register dependencies and captured CPU indices, including across resizing or
  freeing the public buffer. Its inverse never rereads a changed index or dereferences a freed host
  pointer.

The host allocation is a placeholder used only for classical addresses, aligned offsets and lifetime
lookup. Copying such a pointer creates an alias. No allocator or assignment duplicates amplitudes. A
read computes a correlated result in fresh storage with a saved inverse, following the
[ownership contracts](no-clonacion.es.md).

## Supported accesses and explicit limits

CPU/quantum-indexed reads, statement assignments and increments/decrements are implemented. A pure
quantum index selects writes coherently, with its original range checked before narrowing it. The
index and assigned value are captured before writing the candidate slots; their private caches are
uncomputed against the retained original registers. Selection visits the candidate slots, so its
circuit cost grows with the heap extent. Arithmetic on stored values retains the ordinary integer
promotion and definedness checks. `measure` and `reset` are explicit channels and require mutable
access and classical control/index for direct element channels.

Compatible local aliases and aligned pointer suffixes can be passed to expanded same-unit functions.
`const` borrows remain read-only even after a cast. A borrowed parameter, including an alias reached
through assignment, cannot be freed or reallocated by its callee. Mutable parameters require
disjoint heap suffixes; the current check conservatively rejects two such suffixes from the same
allocation. Equality, null tests and ordinary same-object differences use classical addresses.
Escaping into ordinary CPU data or calling a foreign CPU buffer API is diagnosed.

The frontend supports element widths 1..64 bits with padding-free storage. Each allocation is
limited to 64 elements; actual capacity is also limited by the engine's 128 physical wires and
temporary/history requirements. Dense execution has a 24-wire limit. Sparse execution supports up to
128 wires with a bounded occupied support. Repeated overwrites of unknown data can exhaust the
retained history budget; `free` does not make that history vanish.

Assignments and prefix/postfix increments also return typed quantum values to scheduled consumers
outside runtime CPU loops. `p[i]++` preserves the previous value; `++p[i]` and assignments return
the converted assigned value. Returned values retain their exact input versions across later stores
or `free`, and can be used in arithmetic, explicit measurement and supported lazy quantum
expressions. An inactive arm keeps the heap unchanged and contributes a zero private capture; the
enclosing expression selects its ordinary C result. No measurement is introduced to choose an
address or obtain an update value.

Native heap-to-heap `memcpy` and `memmove`, and heap `memset`, accept pure classical byte addresses
and runtime byte lengths, including unaligned byte views, partial integer words and different source
and destination widths. The limit is 64 bytes per operation, in addition to the physical/history
budgets. `memcpy` rejects overlapping nonempty ranges before writing; `memmove` pins the original
source versions before its first write. `memset` captures its converted unsigned-byte fill once,
including a quantum fill that reads the destination. Byte order follows the GCC target. Coherent
controls do not introduce a measurement. Copies produce basis correlations with their source.

Byte copies also cross between heaps and checked fixed quantum buffers in both directions. Known
nonvolatile CPU objects may supply once-only current byte snapshots to a heap. The fixed view
retains a checked static address; its byte length can be a pure CPU runtime value. The entire heap
range and fixed extent are checked before destination writes. Only bytes below that length are read
from CPU storage. Words outside the length keep their existing owner/wires, including the whole
destination for a zero-length copy. Heap-to-fixed writes use the compiler's versioned owners and CPU
joins, so saved inverses retain actual source registers after heap overwrite or disposal.
Heap-to-fixed writes inside runtime CPU loops now use
[per-iteration owners](c11-runtime-history.md), as do scheduled assignment and increment results.
CPU-object-to-heap and heap-to-heap loops retain their separate finite execution tests.

The returned destination is a classical address: local typed/`void *`/byte-view aliases, assignment,
equality and null comparisons are supported. Those aliases retain the original allocation type and
read-only/borrow restrictions. Direct byte scalar loads/stores through a cast of a wider heap remain
pending. Empty ranges allow a live allocation's one-past address; null or dead allocations remain
invalid even at zero length. A provably inactive quantum operation skips its range checks.

Unrestricted pointer arithmetic/ABI, general aggregates, general returned-pointer consumers, string
adapters over heaps, quantum floating-point storage, atomics and concurrency remain pending. Quantum
allocation under coherent control is unsupported. Runtime CPU control and heap allocation are
native-only: static QASM/resource export rejects that profile. Structural template validation does
not execute allocations or prove the resulting circuit. There is no physical-QPU or speedup claim.

## Reproduce the usable block and directed validation

```sh
CC=gcc-11 CXX=g++-11 bash scripts/build.sh
bash scripts/run-block.sh 05
CKET_SIMULATOR=sparse build/blocks/05/dynamic_heap 4
CKET_SIMULATOR=sparse build/blocks/05/coherent_heap
CKET_SIMULATOR=sparse build/blocks/05/heap_bytes
CKET_SIMULATOR=sparse build/blocks/05/heap_transfer
CKET_SIMULATOR=sparse build/blocks/05/heap_iterations 3
build/validate_heap
build/validate_runtime_versions
python3 scripts/validate-heap.py --output build/heap-proof --levels O0 O1 O2 O3 Os
python3 scripts/validate-heap-memory.py --output build/heap-byte-proof --levels O0 O2 Os
python3 scripts/validate-heap-transfer.py --output build/heap-transfer-proof --levels O0 O2 Os
```

[`dynamic_heap.c`](../examples/blocks/05/dynamic_heap.c) takes an initial capacity of 2, 3 or 4 from
`argv`, fills the buffer, conditionally writes under a quantum Boolean, grows it, passes it to a
function, measures the sum and elements explicitly, and frees it. It compares the observations with
an ordinary integer formula.

The directed validator compares independent joint-state formulas and independent gate evolution
before and after saved inverses, resize and disposal. It also checks selected valid programs and
semantic rejections. The C engine validator covers small complete states, resource failures and
register/wire reuse; its check count is a mixture of assertions, not a count of independent quantum
states. It also checks selected writes, old/new return values, lazy activation and inverses after
freeing the public buffer. Unsequenced heap operands currently require an explicit disjoint-address
contract that has not been implemented; sequence their accesses with comma or separate statements.
The compiler diagnoses that missing proof without classifying every such C program as undefined.
These finite checks do not certify all pointer contexts or the historical papers.

[`heap_bytes.c`](../examples/blocks/05/heap_bytes.c) combines source-preserving copy, overlapping
move and captured quantum fill, then compares explicit observations with a formula. The byte
validator checks independent complex joint-state formulas, phases, saved inverses after freeing both
buffers, full gate reversal, returned aliases and runtime CPU loops. The engine separately checks
both byte orders and malformed schemas before synthesis. Its native execution evidence is recorded
separately from earlier heap campaigns.

## Identity writes and remaining quantum findings

A bounded symbolic proof preserves the existing owner, epoch and wires when a heap store is provably
the current word. This avoids redundant history that previously dephased entangled self-assignment
and identical one-byte fill. The proof uses typed bit expressions, valid saved dependency epochs and
compatible coherent guards. It reads no amplitudes; unsupported or costly proofs use ordinary
validation/synthesis. Discarded operands retain definedness checks. Pure CPU fill arguments are
captured once, including zero-length fills. Effects under coherent control or mixed CPU effects with
a quantum fill expression remain rejected.

The [finite identity evidence](audit-evidence/heap-identities-v1/README.es.md) covers eight integer
types through 64 bits, both synthesis policies and CPU loops, with full complex states, phases,
stable owners/wires, zero private workspace and independent gate reversal. Returned captures have
been state-checked through 32 bits; two 64-bit capture profiles exceed the 128-wire budget. The
native fill identity uses a single byte with zero high bytes; engine fixtures separately check
nonzero high bits and both byte orders. This does not prove every algebraic identity or arbitrary
heap operation. The [quantum review](re-audit-2026-10-24-coherence.md) preserves the historical
dephasing findings in an inactive sector of a nonidentity coherent copy and in scoped
unobserved-borrow cleanup. Their later finite corrections have separate source identities.

The later [inactive-borrow correction](audit-evidence/inactive-borrows-v1/README.es.md) preserves
scope cleanup for demonstrably inactive fixed writes. That campaign did not close the nonidentity
coherent heap-copy finding or general cleanup after possible-active coherent overwrites.

The [1.0.0-rc.1 conditional-inverse correction](audit-evidence/rebound-borrows-v1/README.es.md)
closes the published fixed root-owner scoped-borrow cases, including possible-active controls. That
RC1 campaign did not fix the nonidentity coherent heap-copy counterexample. The later
[RC2 heap correction](audit-evidence/heap-coherence-v1/README.es.md) closes the reproduced case,
with independent public densities, joint gate states and inverses. General heap and lifetime
combinations still require their own qualification.
