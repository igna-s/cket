# Checked quantum `memset`

This contract describes an **isolated experimental extension**. It has not been applied to the main
compiler. Its positive cases do not establish complete C11 memory or library support.

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

The returned pointer for quantum destinations does not yet have a checked return-pointer ABI. The
implemented calls discard that result; CPU initialization or other unsupported consumers diagnose
the quantum marker. Quantum indirect-call and separately compiled library ABIs also remain pending.

Runtime counts, general pointers and aliasing, dynamic memory, general aggregates, padding, `_Bool`
object-byte writes, floating objects, volatile/atomic accesses and CPU-loop history require further
implementation. Static loop expansion and the supported CPU branch contexts have finite integration
tests; they do not imply arbitrary control-flow support.

Unsequenced modifications and accesses retain their C11 undefined-behavior diagnostic. CPU side
effects under coherent control are rejected. A quantum value cannot silently be written into a CPU
buffer; crossing that boundary requires explicit observation and the ordinary classical operation.
Cybersecurity and security-specific memory interfaces remain classical and outside this extension.

## Verification scope

The first maintained exploratory run passed 360 checks against 131 frozen source inputs: 140 dense
state/inverse cases, 20 complete controlled basis-action circuits, 130 native executions and 70
diagnostic contracts. Thirteen additional O2 native executions also passed. The sources and results
are preserved in the experiment's `build/validator-v1-evidence` directory. That run reused unchanged
base driver, runtime and engine tooling and built its plugin and validator separately.

After adding sequenced quantum arguments, eight finite native executions and the unsequenced-access
diagnostic passed at O2 with builtins enabled. Sixteen dense state/inverse checks passed for those
effects under both synthesis policies. A fixture that redundantly kept another eight-bit reference
exceeded the 24-qubit statevector budget; its failed run is preserved separately and is not a
passing result. The corrected array fixture checks the correlation without that extra register.

The expanded maintained [validator](../tests/validate_memset.c) passed **800 checks** against 132
frozen source inputs: the original 140 dense state/inverse cases and 20 controlled labeled circuits,
160 [sequenced-effect state/inverse cases](../tests/memset_effects.inc), 340 native executions, 80
diagnostic contracts and 60 wide no-table generation cases. The source profiles run across five GCC
optimization levels and both builtin modes; state checks also use both synthesis policies. This is
still an exploratory normal configuration with reused unchanged base tooling, rather than the full
fresh normal/sanitized regression campaign.

Dense state checks, controlled labeled basis paths, native executions, expected rejections and wide
generation are different forms of evidence. The controlled full-byte profile has 26 physical wires:
the validator follows every one of its 512 labeled input paths after verifying its canonical nine-H
preparation and supported reversible suffix. A separate sparse OpenQASM calculation also passed for
that profile. These checks are distinct from an unrestricted dense 26-qubit simulation.

Five added table-free exports passed independent QuEST state and measurement-probability
comparisons: full-byte retained history, a full-byte self alias, a partial word with quantum fill, a
lazy sequenced value argument and a two-element array whose fill aliases its first element. Those
circuits use 17, 17, 18, 16 and 24 physical wires respectively. They are a scoped five-state
addition, not a claim that the whole independent regression suite was repeated against this
extension.

Four self-modifying argument profiles were also checked over all 256 byte values: `x++`, `++x`,
`(x = 7, x)` and an array fill from `a[0]++`. Eight prefix/inverse exports passed independent sparse
amplitude checks and complete labeled input paths after verifying their eight-H preparation. The
profiles use 32, 17, 25 and 40 physical wires respectively. Saved result/history definitions and the
reversible in-place increment are inverted in dependency order; known destination writes are also
undone. The final states retain the original input phases and have zero private wires. This evidence
uses O2, builtins and no-table synthesis; it is distinct from dense statevector execution and the
five QuEST additions above.

The full fresh normal/sanitized regression campaign, integration checks and final
inventory/documentation updates remain to be completed. There is no physical QPU execution or
measured quantum speedup claim.
