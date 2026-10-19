# Checked quantum `memset`

This is an integrated partial adapter for checked integer storage. It does not establish complete
C11 memory or library support. Its expression composition contract is described in
[discarded memory-call sequences](c11-memory-sequences.md).

The adapter implements the byte writes of `memset(destination, value, count)` on checked integer
quantum storage. It converts the value to an unsigned byte, evaluates that argument once, and
repeats the captured byte over the selected range. Ordinary CPU storage with ordinary CPU values
retains its classical libc call and return pointer.

## Storage and argument semantics

The implemented destination forms are checked scalar integer objects, fixed integer arrays and
compatible checked borrows. Their elements have 8, 16, 32 or 64 bits, with no padding. Constant byte
offsets can select part of an element or cross element boundaries. The byte count must be constant,
within the resolved object and at most 64. These checks do not promise that the resulting circuit
fits the backend's physical wire budget or the simulator's smaller statevector budget.

Unwritten bytes keep their original values. The implementation uses the target byte order when
mapping object bytes to integer bits. Current execution evidence uses GCC 11.4 on little-endian
x86-64 Linux/WSL; it does not verify a big-endian platform.

The value argument may be a classical integer captured once outside coherent control, or a supported
quantum integer expression. Sequenced quantum value effects use the compiler's existing expression
scheduler: postfix/prefix updates, decrement, comma and selected conditional arms execute before the
destination writes. The captured result is reused across every destination element. An argument that
aliases a destination therefore supplies its value from before the writes.

Zero length suppresses the memory writes. It still evaluates the value argument once, including its
supported CPU or quantum effects. A reachable division by zero in that argument remains an error;
zero length does not make its evaluation disappear. A one-past destination is accepted for the
checked zero-length cases. General pointer validation and currently rejected layouts remain pending.

## Retained information, conditions and inverses

Writing a fixed byte over an unknown quantum value cannot erase its previous information as a
unitary operation. The adapter preserves the old owner as history and computes a new current value.
An independent destination already known to be a basis value can reuse its wires by reversibly
removing that known value and applying the new pattern.

Coherent conditions select the writes and the supported quantum argument effects without a hidden
measurement or reset. Inactive branches retain their original destination values. Condition
snapshots and intermediate versions can remain live when later definitions still depend on them.
Their lifetimes and the circuit resource counts include that retained information.

The state validators compare relative phases and current results against independent finite C
models. They also apply saved definition inverses and reverse the known-basis writes. After those
inverses, the retained original input and its phases must be restored, the original known
destination must be restored, and the private wires must be zero. No claim is made that an unknown
old value can be discarded independently while its information is still needed.

## Example

```c
#include <quantum.h>
#include <string.h>

int main(void) {
    quant bool value = superposition(0, 1);
    quant uint8_t bytes[2] = {17, 34};

    if (value)
        phase(.31);
    memset(bytes, value--, sizeof bytes);
    if (bytes[0] == 1)
        phase(.17);

    unsigned first = measure(bytes[0]);
    unsigned second = measure(bytes[1]);
    unsigned updated = measure(value);
    return first != second || first > 1 || updated != !first;
}
```

Both bytes receive the same value captured before the decrement. C's conversion back to `_Bool`
makes the decremented value the opposite of its original bit. The test observes these correlations
explicitly; the adapter itself does not observe the input.

## Remaining work

[Local checked destination-return borrows](c11-memory-returns.md) now support compatible typed
pointers, void pointers, constant byte views and supported address relations. General pointer
consumers and escapes, quantum indirect calls and separately compiled library ABIs remain pending.

Runtime counts, general pointers and aliasing, dynamic memory, general aggregates, padding, `_Bool`
object-byte writes, floating objects, volatile/atomic accesses and CPU-loop history require further
implementation. Static loop expansion and the supported CPU branch contexts have finite integration
tests; they do not imply arbitrary control-flow support.

Unsequenced modifications and accesses retain their C11 undefined-behavior diagnostic. CPU side
effects under coherent control are rejected. A quantum value cannot silently be written into a CPU
buffer; crossing that boundary requires explicit observation and the ordinary classical operation.
Cybersecurity and security-specific memory interfaces remain classical and outside this extension.

## Verification scope

The maintained [validator](../tests/validate_memset.c) passes **800 checks** in each normal and
ASan/UBSan configuration: 140 dense state/inverse cases, 20 complete controlled labeled-action
circuits, 160 sequenced-effect state/inverse cases, 350 native executions, 70 diagnostic contracts
and 60 wide generation cases. Source profiles use five GCC optimization levels and both builtin
modes; state/action checks also use both synthesis policies. These proof categories are distinct.

The [category correction](audit-evidence/memset-counter-correction/summary.json) preserves the two
closed normal/ASan/UBSan logs from the 144-input campaign. The earlier documentation split of 340
native executions and 80 diagnostics was incorrect; the total remains 800. Those two closed commands
do not establish that the complete campaign has finished.

The complete [134-input campaign](audit-evidence/memory-expressions/results.json) rebuilt its normal
and instrumented tools in an isolated checkout. All 134 source hashes then matched main. The
original WSL process stopped during the sanitizer library suite. Its completed prefix and the
resumed whole library suite and remaining tail are preserved separately on identical sources; the
original exit status is unknown. This is a segmented campaign. Separate fresh main builds passed
both exact contract programs at five GCC levels and both builtin modes: 20 executions per
configuration. The [integration record](audit-evidence/memory-expressions/integration-summary.json)
keeps those main checks distinct from the broad isolated regression.

The 26-wire controlled byte profile follows all 512 labeled paths after checking its nine-H
preparation; it is not a dense 26-qubit simulation. Five added QuEST states check retained history,
self aliasing, partial words, lazy value effects and array aliasing at 17, 17, 18, 16 and 24 wires.
Separate O2 sparse-amplitude and complete labeled-path checks cover four self-modifying value
profiles over all 256 inputs and their saved inverses, using 32, 17, 25 and 40 wires. The
[earlier evidence capsule](audit-evidence/memory-expressions/earlier-scoped-evidence.tar.gz)
preserves their original source identities, results and scopes; those earlier checks are not
relabeled as full regressions.

The full independent regression has 70 circuits: 64 states and six sampled channels. No physical QPU
execution, general absence of bugs or measured quantum speedup is claimed.

The successful continuation and the separate fresh integration checks ran in a hash-verified Linux
filesystem copy, without Windows drive mounts. The first continuation was also interrupted and has
no known terminal status. Existing core binaries were transferred for the successful broad tail; the
separate integration tools were freshly rebuilt. Applying the identical files back to the Windows
repository requires an additional destination-hash guard.

A further WSL restart interrupted sanitizer fuzzing after 63 complete cases. The successful
continuation retained that prefix and used an archived temporary driver with unchanged generator,
generated program and oracle. Its original seed and case indices, all five optimization levels and
all 256 byte inputs were preserved. The 665 sanitizer fuzz compilations are an explicitly verified
segmented aggregate; extra harness sources and the original interrupted raw log are retained
separately.

A later restart interrupted the final sanitizer QASM suite after fuzzing and growth checks had
already closed. Only that unfinished QASM suite was repeated in the final segment, with the earlier
raw logs retained.
