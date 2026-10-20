# Checked quantum `strcpy` and `strncpy`

These are partial adapters for copying checked strings into quantum integer storage. They reuse
typed scalar updates and retained overwrite history. Full state, inverse, sanitizer and compiler
context qualification remains deferred until the implementation block closes.

The C reference is
[WG14 N1570, §7.24.2.3–4](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf). `strcpy`
copies through the first NUL and leaves later destination bytes untouched. `strncpy` writes exactly
`n` bytes, zero-padding after a source NUL; a source with `n` nonzero bytes need not produce a
terminated destination. Both return the destination and require nonoverlapping copies.

```c
quant char source[2] = {letter ? 'a' : 0, 0};
quant char copied[3] = {'X', 'Y', 'Z'};
quant char padded[3] = {'X', 'Y', 'Z'};
char *alias = strcpy(copied, source);
strncpy(padded, source, sizeof padded);
```

The destination must be mutable checked quantum integer object storage without padding. Fixed
arrays, compatible checked borrows and constant byte offsets are supported. Partial integer writes
preserve the other bytes and use target byte order. Known nonvolatile CPU sources and compatible
quantum byte views are accepted; quantum data cannot be written into CPU storage without explicit
measurement. Ordinary CPU calls retain libc behavior and function-address ABI.

For `strcpy`, the full remaining source view is at most 64 bytes and the source must contain a NUL
within the smaller of its source view and available destination. For `strncpy`, `n` is a constant
from 0 to 64 that fits the destination. A shorter source object requires an in-view NUL before it
ends; padding then continues without reading beyond the object. If `n` readable source bytes exist,
an in-view NUL is optional. Zero length performs no loads or writes; pointer arguments still need
valid checked views.

Potentially overlapping selected byte ranges are conservatively rejected. Some calls whose actual
shorter prefix is disjoint may therefore still be pending. CPU volatile/atomic access, dynamic
counts and addresses, general layouts and indirect quantum ABI remain pending. Saved search-pointer
destinations are outside this adapter's static address model.

Known CPU source bytes are captured once per call, with loads disabled after their own NUL. Updates
share those captures and saved inverses retain them after CPU mutation. Coherent control keeps the
selected writes and domain guards in the circuit. Missing terminators currently use the existing
internal arithmetic-domain guard; that check must never execute as host division by zero.

Old unknown destination values are retained when the scalar update model needs them for an inverse.
This is a reversible mapping with history, not a copy of an arbitrary unknown quantum state into an
independent state. Gate, history and simulation budgets apply separately from the 64-byte frontend
bound. General memory effects and lifetime combinations remain subject to the existing restrictions.

A returned pointer may initialize a checked local destination alias. Discarded calls and ordered
discarded-call expressions use the existing memory-sequence scheduling. Other return-pointer
consumers, general pointer ABI and complete composition contexts remain pending.

[`string_copy.c`](../examples/blocks/06/string_copy.c) demonstrates a variable-length quantum
source, unchanged tail bytes, zero padding, a destination-return alias and a partial 16-bit write
from a mutable CPU source. Build with GCC 11 and run `CC=gcc-11 bash scripts/run-block.sh 06`. Its
native outputs are finite evidence; exhaustive coherent/inverse and mutation matrices are deferred.
Physical QPU execution and a speedup over CPU are not claimed.

The mutable CPU source introduces `qbind` parameters in the exported IR. The runner uses the
example's [`string_copy.bindings.json`](../examples/blocks/06/string_copy.bindings.json) to
specialize static QASM/resources for the initial zero byte. This file records the two typed
parameter values of that capture; it is tied to the example's current instruction layout. Native
execution reads the actual CPU object and does not substitute the fixture. Unbound templates require
explicit specialization before static circuit export.
