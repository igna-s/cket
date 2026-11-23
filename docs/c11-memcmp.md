# C11 `memcmp`: coherent object-byte comparisons

The adapter implements an input-preserving comparison of checked integer storage. It advances M4/M7;
dynamic memory, general string processing and complete C11/library compatibility remain open. The
separate [strncmp adapter](c11-strncmp.md) implements bounded NUL-stopping comparison. The reference
is N1570 [7.24.4 and 7.24.4.1](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf).

For each basis input, bytes are read in the declared target's object order and interpreted as
`unsigned char`. The result is zero for equal ranges, or the exact difference of the first unequal
bytes, within `[-255,255]`. C11 requires the resulting sign; this exact magnitude is the quantum
adapter's specified choice. Classical calls continue to use the host implementation and are only
required to agree in sign. Comparing multi-byte integers this way can differ from comparing their
numeric values, especially on the current little-endian GCC/glibc target.

The transformation preserves both inputs and computes the result into fresh storage:

```text
sum_(a,b) alpha_(a,b) |a,b,0> -> sum_(a,b) alpha_(a,b) |a,b,memcmp(a,b,n)>
```

Distinct input states remain distinct in the joint output. Correlations, relative phases and
entanglement are preserved. There is no implicit measurement, reset or erasure. A saved inverse uses
the original inputs; their ordinary dependency and history rules continue to apply. Pure comparisons
can be uncomputed after their last use.

## Storage, bounds and CPU inputs

Supported quantum ranges are scalars, fixed arrays and checked borrows of 8/16/32/64-bit integer
elements whose precision equals their object size. Signed elements are read as object bits, rather
than sign-extended numeric bytes. Read-only borrows and overlapping inputs are allowed. A constant
byte offset may select part of an element or cross element boundaries. This read-only byte view does
not implement general byte-pointer dereferences or byte writes elsewhere in the language. Existing
quantum-procedure parameter alias exclusions still apply at helper-call boundaries.

The other operand may be a string literal, immutable integer constant or a
[current CPU byte snapshot](c11-byte-snapshots.md) of known, fixed-size storage. CPU objects use
unsigned-character loads of their actual current representations, including selected floating or
aggregate bytes. This does not implement quantum floating/aggregate layouts or portable padding
interpretation. Each selected byte is captured once; the saved inverse reuses those values after CPU
mutation. General pointers, dynamic offsets/counts, volatile/atomic access and general quantum
layouts remain pending. Ordinary classical calls still use libc.

Both ranges and the constant byte count are checked before expression generation. Zero bytes at a
valid one-past address compare equal. Out-of-bounds views/counts are rejected. The frontend accepts
at most 64 compared bytes; expression depth/work, the baseline 128-qubit gate representation
(`5c5a965`; expanded to 1024 in Wave 1A engine `QC_MAX_PHYSICAL`, pending Wave 1B/2), 24-qubit
native dense simulator and gate limits apply separately. A frontend byte bound does not promise that
every combination fits those subsequent budgets. Resource reports describe logical gates before
hardware decomposition.

```c
#include <quantum.h>
#include <string.h>

int main(void) {
    quant uint16_t input = superposition(0, 65535);
    const uint16_t reference = 0xab20;
    quant bool earlier = memcmp((const void *)&input, &reference, sizeof input) < 0;
    if (earlier)
        phase(0.23);
    measure(input);
    return 0;
}
```

The explicit pointer cast removes the frontend's storage-preserving `volatile` qualifier at this
checked library boundary. It never makes the quantum object readable by CPU byte loads.

## GCC integration and synthesis

`include/string.h` includes the system header, then routes ordinary `memcmp(a,b,n)` calls through an
external declaration with assembler name `memcmp`. Classical calls still invoke libc. This keeps GCC
from folding a one-byte call into host byte loads before the quantum pass. Taking the standard
function's address keeps its ordinary C interface; a checked indirect quantum ABI remains pending.
Bypassing the wrapper with `#undef`, parenthesized calls or explicit builtins remains subject to
GCC's earlier transformations and does not acquire a new general provenance guarantee. User-defined
functions are not adapted merely because their spelling matches.

Comparisons of the call directly against zero lower to Boolean lexicographic expressions. This
covers all six relations and reversed operands such as `0 < memcmp(...)`. Narrowing or unsigned
casts of the integer result are preserved, since they can change its sign. A standalone integer
result retains its complete difference expression and its larger resource cost.

The backend synthesizes these expressions with reversible gates. Ordered comparisons against
constant values use disjoint bit prefixes without a difference register; constant shifts select
existing wires/sign bits/zero bits without barrel-shifter output storage. Neither optimization
enumerates the input domain. Variable comparisons retain their separate reversible workspace.

## Verification

`tests/validate_memcmp.c` distinguishes complete complex-state/inverse checks, C execution,
diagnostic contracts and wide circuit generation. The complete-state cases include all byte
patterns, signed object representations, all 65536 patterns of a two-byte input, selected byte
views, two coherent inputs and raw differences. The byte cases span all five GCC levels and both
builtin settings; wider complete-state cases use `-O2` with both settings. Both table policies are
checked.

Native programs compare all 256 input byte values, all six relations and narrowed raw results with C
expectations. Separate bounded programs enumerate all 256 values for each relation and the raw
result, keeping each static expansion within the expression budget without dropping byte values.
Separate cases cover const borrows, byte offsets, zero-length one-past ranges, result casts and
ordinary libc. Wide 16/32/64-bit exports use `--no-tables` and are generation evidence.

Four original independent QuEST cases exercise signed byte order, a complete two-byte comparison,
byte views and two quantum inputs with relative phases. Current test counts, exact source hashes and
completed normal/sanitized logs belong to the [campaign archive](audit-evidence/README.md). They do
not turn this partial adapter into complete memory or library support.

A supplementary [composition capsule](audit-evidence/memcmp/oracle-states.tar.gz) checks this
comparison inside a `find_known` predicate against the analytic Grover amplitudes, with complete
private-wire cleanup. Its twenty normal and twenty sanitized states cover five GCC levels, both
builtin modes and both table policies. The [follow-up report](re-audit-2026-10-15-memory.md)
distinguishes that check from the independent QuEST suite and preserves the original importer
capacity failure separately from its successful rerun.

The later [string/full-result audit](re-audit-2026-10-15-strings.md) adds sparse simulation and
labeled basis-path checks of complete signed 32-bit results, including private zeros. Its source
identity and compiler modes remain separate from the earlier comparison campaign.
