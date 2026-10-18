# Classical byte snapshots in coherent comparisons

This extension supplies current classical object bytes to the partial quantum `memcmp` and `strncmp`
adapters. The classical operand remains CPU storage. A runtime capture supplies classical parameters
to an input-preserving quantum comparison, and its inverse reuses those parameters even after the
CPU object changes. It neither measures a quantum input nor loads the C representation of a quantum
placeholder. Ordinary calls involving only classical operands continue to use libc.

The accepted CPU address must identify named storage with a fixed, known object size, optionally
with a constant array index and constant byte offset. Mutable objects, runtime-initialized const
objects, fixed arrays, and selected bytes of CPU floating or aggregate representations are
supported. Access uses unsigned characters. This does not implement quantum floating arithmetic or
general quantum aggregate layouts. Immutable integer constants and string literals can still be
embedded without runtime parameters.

Every counted byte must fit both declared objects, even when the comparison will terminate earlier.
The byte count and offsets must be constant; the frontend limit is 64 bytes. Existing expression,
gate, circuit and simulator budgets remain separate. A zero-length comparison at a checked one-past
address performs no captures. Only the selected CPU bytes become parameters, so a one-byte view of
an 8192-byte CPU array does not capture the complete array.

`memcmp` captures the selected CPU bytes once. `strncmp` queries each selected byte once but
disables loads after that operand's first NUL, passing zero parameters for that suffix. It may
capture bytes that an individual quantum basis input would not compare because of an earlier
difference or NUL on the quantum side. Therefore, all enabled CPU loads must address readable,
determinate bytes for the duration of the capture. No concurrent mutation is supported. This is a
conservative classical snapshot contract; it does not supply coherent, branch-dependent CPU memory
access.

Volatile and atomic storage, including such members inside an aggregate, are rejected. Unknown CPU
pointers, dynamic extents, general subobject address expressions, indirect quantum ABI and general
quantum byte layouts remain pending. Object padding and indeterminate representations acquire no new
portable meaning from this extension. Object-byte results depend on the declared GCC/glibc target's
representation and endianness.

```c
#include <quantum.h>
#include <string.h>

int main(void) {
    unsigned char reference = 65;
    reference = 17;
    quant uint8_t input = superposition(0, 255);
    quant bool earlier = memcmp((const void *)&input, &reference, 1) <= 0;
    reference = 99;
    if (earlier)
        phase(0.17);
    measure(input);
    return reference != 99;
}
```

The circuit predicate uses 17, including during cleanup after `reference` becomes 99. As in the
existing comparison adapters, the raw result is the first unequal unsigned-byte difference, while
C11 itself specifies its sign. This example observes the input explicitly at the end.

The private CPU helper `__cket_read_byte(address, enabled)` returns zero without dereferencing a
disabled address. Its GCC `noipa` attribute keeps the conditional load opaque to the caller. An
earlier prototype with an inline conditional passed numerical state checks but failed when the CPU
NUL occupied the last byte before a protected page: GCC speculated the next object-byte load. The
failing source, source capsules and native/classical results remain in the isolated prototype's
`build/nul-page-before` and `build/nul-page-after` evidence. The current guarded helper is tested
with protected and null disabled addresses across five optimization levels and both builtin modes.
Those historical failures must not be reported as successful complete campaigns.

The finite validator defines 650 checks: 20 CPU read guards, 250 native complete-state cases, 140
native programs, 180 rejected boundary cases and 60 template/export contracts. State cases inspect
all complex amplitudes, input preservation, private zeros, phases, actual native binding values,
byte-query counts and saved inverses at four checkpoints. Byte domains cover all 256 values; word
domains cover all 65536 patterns at `-O2`. Native programs additionally exercise all six relations,
helpers, callback initialization, raw results, reversed operands and zero counts. Four additional
QuEST cases independently execute bound QASM circuits with CPU mutation and saved cleanup. Their
binding rows are supplied explicitly; the native tracing cases separately check real CPU captures. A
completed campaign must identify its exact sources and the configurations actually run; a test
matrix alone is not evidence of completion.

C11's object-representation and character-aliasing rules are in sections 6.2.6.1 and 6.5, and the
comparison contracts are in sections 7.24.4 and 7.24.4.4 of the
[WG14 N1570 draft](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf).
[WG14 defect report 451](https://www.open-std.org/jtc1/sc22/wg14/www/docs/dr_451.htm) explains why
indeterminate values must not be treated as arbitrary stable byte constants. The private helper uses
the documented
[GCC 11.4 `noipa` function attribute](https://gcc.gnu.org/onlinedocs/gcc-11.4.0/gcc/Common-Function-Attributes.html).

`memcmp` and `strncmp` remain partial inventory entries. This work does not establish complete
quantum C11/library support, physical QPU execution, or support for the other string interfaces.
Cybersecurity operations remain classical and are outside the quantum adaptation scope.
