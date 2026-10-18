# C11 `strncmp`: coherent bounded character comparison

This partial adapter compares checked byte ranges as unsigned characters. It stops at the first
differing byte, an equal NUL or the fixed count. Inputs and relative phases are preserved while a
fresh quantum result stores zero or the exact first unequal unsigned-byte difference in
`[-255,255]`. C11 specifies the sign, so ordinary classical libc calls may return another magnitude.
The transformation is input-preserving, rather than an overwrite or an implicit observation.

The declared target is GCC/glibc with eight-bit bytes and padding-free 8/16/32/64-bit integer
storage. Quantum scalars, fixed arrays, constant byte views, overlaps and readonly checked borrows
are supported. The other operand may be an immutable constant, string literal or a
[current CPU byte snapshot](c11-byte-snapshots.md) of known fixed-size storage. Signed objects are
compared by their object bytes. These are byte comparisons, not Unicode collation or
locale-sensitive string comparison.

The count and offsets must be constant and all counted bytes must fit both checked objects. This
conservative requirement also applies when a particular input would terminate earlier; it does not
implement every otherwise-defined C call with a larger count. A zero count at a checked one-past
address is allowed. The frontend bound is 64 bytes; expression, gate, 128-qubit circuit
representation and 24-qubit native simulation bounds apply separately.

CPU snapshots capture actual current bytes once, with disabled loads after the CPU operand's own
NUL. The inverse reuses the captured values after CPU mutation. All enabled captures must access
readable, determinate bytes; an individual quantum basis path may compare fewer bytes. Unknown
pointers, dynamic counts/addresses, volatile/atomic accesses, general quantum layouts, indirect
quantum ABI and source provenance before GCC folding remain pending. The header routes normal
`strncmp(a,b,n)` syntax through an external declaration with assembler name `strncmp`; ordinary
classical calls still invoke libc. Wrapper bypass forms retain their separate GCC-dependent
limitations. Direct comparisons with zero use Boolean lexicographic expressions. Unsigned and
narrowing casts preserve their actual C conversions and use the full integer result.

```c
#include <quantum.h>
#include <string.h>

int main(void) {
    quant uint16_t input = superposition(0, 65535);
    const uint16_t reference = 0xab00;
    quant bool earlier = strncmp((const char *)&input, (const char *)&reference, 2) <= 0;
    if (earlier)
        phase(0.17);
    measure(input);
    return 0;
}
```

On the declared little-endian target, equal zero low bytes stop comparison even when high bytes
differ. Different nonzero low bytes decide the result. Equal nonzero low bytes continue to the next
byte. The circuit retains all input bits and cleans temporary computations by their saved inverses;
logical gate accounting includes its bounded suffix computation.

The validator separates 56 full complex-state/inverse cases, 80 native programs, 80 rejection
contracts and 30 wide no-table exports: 246 checks. Byte cases cover all 256 patterns, including
signed object representations, across five GCC levels, both builtin settings and table policies.
Wider coherent cases cover all 65536 two-byte patterns at `-O2` in both builtin settings and
policies, with NUL termination, continued tails and result-dependent phases. Other cases check two
quantum inputs, zero-length one-past views and cleanup of a constant prepared result without
channels. Native programs cover helpers/borrows, unsigned bytes, overlap, raw results and libc. Two
original independent QuEST cases exercise the 21- and 24-qubit word comparisons. Wide 16/32/64-bit
exports remain generation evidence, rather than full-state validation at those widths.

The [completed integration record](audit-evidence/strncmp/integration-summary.json) identifies its
exact source state and distinguishes isolated results from main-tree reruns. The
[follow-up report](re-audit-2026-10-15-strings.md) also describes full signed 32-bit results checked
by sparse simulation and labeled basis paths beyond the dense simulator limit. `strcmp`, `strlen`,
`memchr`, the remaining string interfaces and complete quantum C11/library support remain pending.
