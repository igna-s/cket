# Checked quantum `strcat` and `strncat`

These partial adapters append strings to checked quantum integer storage. They snapshot the original
destination length and source prefix length before writing, then use the shared typed string-write
path with retained overwrite history. Full coherent/inverse, sanitizer and compiler context
qualification remains deferred until the implementation block closes.

The C reference is
[WG14 N1570, §7.24.3](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf). `strcat` appends
the source through its NUL. `strncat` appends at most `n` source characters and always writes a
terminating NUL; it does not zero-pad the remaining destination. Both return the original
destination and require nonoverlapping copies.

```c
quant char destination[3] = {leading ? 'a' : 0, 0, 'Z'};
quant char source[2] = {'b', 0};
char *alias = strcat(destination, source);
```

The destination is a mutable checked padding-free integer byte view with 1–64 remaining bytes. It
must contain a NUL before concatenation. The original prefix and bytes following the new NUL are
preserved. The appended prefix and its final NUL must fit the destination. Fixed arrays, compatible
checked borrows and constant byte offsets use the existing target byte-order model.

For `strcat`, the remaining source view has 1–64 bytes and requires an in-view NUL. For `strncat`,
the append count is constant and at most 64. A source NUL is optional when that many bytes exist; a
shorter source object must terminate before its end. A zero append count reads no source bytes,
while the destination still needs a valid in-view NUL. Pointer arguments need valid checked views.

Source views may be quantum storage or known nonvolatile CPU objects. CPU source bytes are captured
once, with guarded loads after their own first NUL. Static exports with mutable CPU sources need
explicit parameter specialization; the native program captures the actual CPU values. CPU volatile
or atomic access and writing unmeasured values into CPU destinations remain unsupported.

Two private quantum byte values retain the original lengths. Their initialization and domain guards
are lowered within the circuit. Later destination updates therefore cannot shift the append position
or change the meaning of later writes. Previous owners remain available when the inverse needs them;
unused length computations can be uncomputed without measuring their values.

Potential overlap between the checked destination and source read ranges is conservatively rejected.
Some disjoint actual short-prefix cases remain pending. Dynamic pointers/counts, general layouts,
saved search-pointer destinations, general pointer ABI and further composition/lifetime contexts
also remain pending. The local returned-destination borrow and discarded-call sequence contracts
follow [checked string copy](c11-string-copy.md).

Coherently controlled calls now reserve their compiler-private zero captures independently of the
condition. Each captured length is computed only on the active branch and is zero on the inactive
branch. Subsequent writes select the preceding destination byte values when inactive, using the
documented retained-history mapping. No measurement chooses the branch.

This exception applies to compiler-private zero storage. User quantum allocations inside coherent
branches retain their existing restrictions. Dynamic CPU loops and further classical branch/merge
lifetimes for these captures remain pending, as does complete state/inverse qualification of mixed
contexts.

The byte bound is separate from gate, expression, history and simulator budgets. Concatenation can
use more workspace than a copy because it preserves two lengths and previous destination values.
This is a reversible basis-value mapping with retained information, not cloning an arbitrary unknown
quantum state. Physical QPU execution and acceleration over CPU are not claimed.

[`string_concat.c`](../examples/blocks/06/string_concat.c) demonstrates a superposed original
destination length, preserved tail bytes, a return alias, and bounded append from a nonterminated
source. Run `CC=gcc-11 bash scripts/run-block.sh 06` after building with GCC 11. Its finite native
observations and exports do not replace the deferred complete state and inverse qualification.

[`controlled_concat.c`](../examples/blocks/06/controlled_concat.c) adds a call under a quantum
Boolean and a phase-interference example. A controlled `strncat` into temporary storage marks the
phase of a Boolean before Fourier; the expected measured value is `1`. Finite native observations
check that example, rather than certifying arbitrary quantum overwrite histories or all controls.
