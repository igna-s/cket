# Discarded memory calls in quantum expressions

This is an isolated extension of the `memset` experiment, not an integrated change to the main
compiler. The two earlier regression campaigns retain their frozen sources.

The previous prototype rejected a checked memory call in the discarded operand of a comma:

```c
quant uint8_t result = (memset((void *)&value, bit, 1), value + 1);
```

The call was correctly recognized as a quantum memory operation, but the integer expression
scheduler then tried to serialize it as an ordinary scalar expression. That produced an unsupported
GCC `call_expr` diagnostic.

The extension schedules the checked call before evaluating the comma's result. It accepts discarded
`memset`, `memcpy` and `memmove` markers and nested comma chains consisting of those calls. The
expression keeps its C result type. Previously captured CPU operands remain available to the final
expression instead of being lost while the memory operation emits its own parameters.

Conditional expressions and logical AND/OR use the existing saved-decision scheduler. Only the
selected quantum arm performs its memory writes. Relative phases and private storage are checked
without inserting a measurement or reset.

## Example

```c
#include <quantum.h>
#include <string.h>

int main(void) {
    quant bool bit = superposition(0, 1);
    quant uint8_t value = 5;
    if (bit)
        phase(.31);

    quant uint8_t result = (memset((void *)&value, bit, 1), value + 1);
    if (result == 2)
        phase(.17);

    unsigned observed_result = measure(result);
    unsigned observed_value = measure(value);
    unsigned observed_bit = measure(bit);
    return observed_value != observed_bit || observed_result != observed_bit + 1;
}
```

The observations follow the current inverse-safety contract: consume a computed result before
observing an input that its saved definition still needs. General observation ordering with live
computed dependents remains pending; that restriction is not a physical impossibility.

## Sequencing and limits

The comma sequences its left operand before its right operand. The working primary reference is
[N1570](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf), sections 6.5.17 and 6.5.2.2.
Paragraph 10 of 6.5.2.2 also makes a function body's execution indeterminately sequenced with
otherwise unordered caller evaluations. The test for `(memset(&x, bit, 1), x) + x` therefore accepts
the permitted caller/call-body orders; an initial exploratory negative expectation was incorrect and
is preserved as a failed fixture. The corrected test starts with `x = 5` and permits `2 * bit` or
`5 + bit`. A modification and read such as `bit++ + bit` within one value argument remains an
unsequenced-access error.

This is finite evidence about the emitted GCC trees. It does not complete source provenance or every
possible unspecified-order expression. The general checked return-pointer ABI is still pending: the
memory call's pointer result is discarded here, not converted to a usable CPU pointer.

Other discarded calls, general CPU-effect placement, general pointer/count/layout contracts and
broader control-flow combinations still require implementation. Coherent CPU side effects retain
their diagnostic. Ordinary CPU statements and cybersecurity remain classical.

## Verification

The new [C validator](../tests/validate_memory_sequences.c) passed 360 normal checks across five GCC
optimization levels and both builtin modes:

- 240 native integrations combine filling, comparison, copying, ctype, integer division/absolute
  value, nested calls, CPU snapshots and coherent short circuiting.
- 20 rejection checks cover actual unsequenced value-argument accesses and coherent CPU effects.
  Ordinary C syntax is checked first; that is not a claim of defined execution for the deliberately
  undefined argument.
- 100 [complete labeled-action checks](../tests/memory_sequence_state.inc) cover five source
  profiles under both synthesis policies. They verify the one-H preparation and both input labels
  through the reversible X/phase suffix, then check result values, relative phases, saved inverses
  and zero private wires after inversion.

The selected normal regression also passed 97 expression checks, 414 memory-copy checks, 800
`memset` checks, 391 native tests and five classical integrations covering the 29 C11 header
families. The separate fresh scoped ASan/UBSan campaign also passed the 360 new checks, 97
expression checks, 414 memory-copy checks, 800 `memset` checks and five classical integrations. It
rebuilt its instrumented plugin, runtime, driver, backend, engine and selected validators. The
normal campaign reused source-identical tools from the freshly built normal `memset` candidate and
built its modified plugin and new validator separately. Both campaigns used the same 134 frozen
source inputs; neither is a repeated full historical `verify.sh` campaign. The
[normal record](../build/sequence-normal-evidence/results.json) and
[sanitizer record](../build/sequence-sanitized-evidence/results.json) retain their distinct scopes.

Twenty actual prefix/inverse OpenQASM exports at O2 with builtins passed independent sparse
amplitude and labeled-path references. Fourteen that fit the existing 24-qubit budget also passed
complete amplitude comparisons against independent finite C models using pinned QuEST 3.7.0.

| Source profile            | No-table physical wires | Table-enabled physical wires |
| ------------------------- | ----------------------: | ---------------------------: |
| Fill then add             |                      25 |                           18 |
| Nested fill argument      |                      25 |                           18 |
| Conditional memory effect |                      26 |                           19 |
| Logical AND memory effect |                      16 |                           12 |
| Logical OR memory effect  |                      16 |                           12 |

The six 25/26-wire prefix/inverse exports remain outside the dense QuEST budget and retain their
separate sparse and labeled checks. These are logical simulator/circuit results, not execution on a
physical QPU or evidence of a quantum speedup. Full C11 compatibility and the larger audit goal
remain open.
