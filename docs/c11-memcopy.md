# C11 memory copying: quantum contracts and evidence

`memcpy` and `memmove` now have partial adapters for checked classical pointers to quantum integer
storage. This advances [M4/M7](c11-roadmap.md); it does not complete byte-addressable memory,
retained-history overwrites or standard-library compatibility.

## Value copying and state preservation

The reference contracts are N1570
[7.24.2.1 and 7.24.2.2](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf). `memcpy`
requires nonoverlapping ranges. `memmove` reads original source bytes even when the ranges overlap.
Both ordinarily return the destination pointer; use of that result on quantum storage is still
pending in this implementation. The current adapter accepts a discarded result, including an
explicit cast to `void`, and rejects escape into CPU pointer storage or an unsupported pointer
borrow.

For a nonoverlapping quantum range and a destination proved zero, the transformation is

```text
sum_x a_x |x>|0>  ->  sum_x a_x |x>|x>
```

The reversible extension acts as `|x>|y> -> |x>|y XOR x>`. This produces correlations and preserves
amplitudes; it does not produce two independent copies of an arbitrary input state. The C source
values remain available. A const source borrow permits this basis-value read, though const still
forbids measurement, reset or basis transformation through that borrow.

More generally a known basis destination `k` is translated to zero by controlled XOR with `k`, then
copied into. Its reversible extension is `|x>|y> -> |x>|y XOR k XOR x>`. On the proved support
`y=k`, the output is the required copied value; inactive branches retain `k`. No distinguishable
unknown inputs are erased by this support-specific extension.

Every nonidentity destination must be available, independently in a known basis state, mutable, and
free of pending inverse dependencies. Unknown destinations need retained history (**P**), not a
silent reset. The adapter never inserts measurement or reset. Explicit source measurements and
channels remain separate operations.

## Layout, bounds and aliases

The implemented target requires a constant byte count and equal-size integer elements whose
precision equals their object size in bits. Whole elements of 8/16/32/64 bits are supported,
including equal-width signed/unsigned object representations on the declared GCC target. Byte
copying is representation transfer, not a numeric conversion that changes a sign bit. Partial
elements, padding, `_Bool` object padding, whole structures/unions, dynamic extents and byte pointer
views remain **P**. Coherent addresses need a distinct implementation (**P/R**).

Pointers are existing checked borrows of scalar/fixed-array storage, with constant offsets. Bounds
are checked before any write. A zero-length range at a valid one-past pointer is accepted. A mixed
CPU/quantum copy requires a preparation or explicit measurement contract and remains **P**; the
adapter does not export quantum values by ordinary byte copying.

`memcpy` rejects overlapping nonempty ranges as undefined C behavior. `memmove` orders reads before
writes: a write to another source element depends on reading that source first. The resulting
acyclic order implements contiguous moves in either direction without private snapshot qubits. All
nonidentity destinations must have a known basis value before the operation starts, including
overlapping source elements that will be overwritten. Exact `memmove` aliases are identity and
preserve arbitrary states. General overlapping overwrites with retained original data remain **P**.

The transformation is a sequence of controlled XOR gates. Each bit contributes at most one copy gate
and one basis-translation gate; known zero bits can be omitted. Coherent controls preserve inactive
branches. No private qubits are required for these copies. The existing 128-qubit representation,
24-qubit native simulation and gate budgets still apply (**R**); frontend range expansion is bounded
to 4096 elements. Logical gate counts do not include hardware decomposition or connectivity costs.

## C integration and GCC

Actual GCC builtins and external system-header declarations are recognized. Fortified glibc inline
wrappers are included in that identity check: with optimization and `-fno-builtin`, a system
declaration may have a saved function body and still be the standard interface. User definitions are
not rewritten merely because their spelling matches.

Purely classical calls retain GCC/libc behavior, including return pointers and overlapping
`memmove`. Calls in expanded quantum procedures support mutable destinations and const sources.
Existing procedure borrow exclusions still apply: overlapping pointer parameters cannot yet express
the direct adapter's `memmove` contract. Such helper signatures need an extended alias contract
(**P**); the overlapping-state tests call the library adapter directly. Copied values can be
measured explicitly. A computed source can be safely uncomputed after copying; the destination
retains its correlated value. An outstanding inverse that depends on a destination prevents
overwriting that destination.

```c
#include <quantum.h>
#include <string.h>

void copy_word(quant uint8_t *destination, quant const uint8_t *source) {
    (void)memcpy((void *)destination, (const void *)source, sizeof(*destination));
}

int main(void) {
    quant uint8_t source = superposition(0, 255), destination = 0;
    copy_word(&destination, &source);
    unsigned first = measure(source), second = measure(destination);
    return first != second;
}
```

## Finite evidence and remaining work

`tests/validate_memcopy.c` separates 414 checks: 120 complete phased states and inverses, four
complete overlapping states, 90 C executions, 170 negative contracts and 30 wide exports. It checks
both library functions, signed layouts, relative phase, an inverse, open/closed controls, both table
settings, builtins enabled/disabled, and all five GCC optimization levels. Overlapping moves in both
directions are contrasted with ordinary C over their complete joint states, with phase and no
private qubits. Bounds, const, dirty/inverse-dependent destinations, overlap, padding, CPU
boundaries and returned-pointer uses have negative cases; classical return pointers and libc
behavior have positive cases.

Nonidentity copying in CPU loops requires a per-iteration basis/history proof and remains **P/R**;
otherwise repeated XOR could silently differ from repeated C assignment. Proved static expansions
are checked at each expanded operation. A negative two-iteration while-loop case checks this
boundary, and a positive static-for case combines control and memory copying.

Wide 16/32/64-bit checks are generation evidence, not complete-state validation. Four independent
QuEST cases contrast controlled copying, signed object bits, exact-alias interference and copying
from a coherent source that is subsequently uncomputed. The current external suite contains 22 state
comparisons and three sampled channels; the papers retain their separately identified historical
evaluation.

Run `CC=gcc-11 CXX=g++-11 bash scripts/verify.sh` for the normal and ASan/UBSan campaign. Completed
results and source hashes belong in the [evidence archive](audit-evidence/README.md). All
outstanding byte layouts, pointer-result ABI, history, dynamic memory and remaining library adapters
retain their entries in the [C11 inventory](c11-inventory.md) and
[interface index](c11-library.json).
