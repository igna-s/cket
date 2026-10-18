# Discarded memory calls in quantum expressions

This integrated extension composes checked quantum memory calls in discarded expression positions.
Earlier exploratory campaigns retain their own frozen sources and finite scopes.

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
possible unspecified-order expression. These discarded calls do not consume their pointer result.
[Local destination-return borrows](c11-memory-returns.md) have a separate partial contract; general
pointer consumers and the general return-pointer ABI remain pending.

Other discarded calls, general CPU-effect placement, general pointer/count/layout contracts and
broader control-flow combinations still require implementation. Coherent CPU side effects retain
their diagnostic. Ordinary CPU statements and cybersecurity remain classical.

## Verification

The maintained [C validator](../tests/validate_memory_sequences.c) passes 360 checks in both normal
and ASan/UBSan configurations, across five GCC optimization levels and both builtin modes:

- 240 native integrations combine filling, comparison, copying, ctype, integer division/absolute
  value, nested calls, CPU snapshots and coherent short circuiting.
- 20 rejection checks cover actual unsequenced value-argument accesses and coherent CPU effects.
  Ordinary C syntax is checked first; that is not a claim of defined execution for the deliberately
  undefined argument.
- 100 [complete labeled-action checks](../tests/memory_sequence_state.inc) cover five source
  profiles under both synthesis policies. They verify the one-H preparation and both input labels
  through the reversible X/phase suffix, then check result values, relative phases, saved inverses
  and zero private wires after inversion.

The complete [134-input campaign](audit-evidence/memory-expressions/results.json) rebuilt normal and
instrumented tools and passed the broad regression in both configurations, including 800 `memset`
checks, 650 CPU byte-snapshot checks, 665 generated frontend compilations and 70 independent QuEST
circuits: 64 states and six sampled channels. The source-identical main checkout then passed
separate fresh normal and instrumented builds and both exact memory contract programs at five GCC
levels and both builtin modes: 20 executions per configuration. The
[integration record](audit-evidence/memory-expressions/integration-summary.json) identifies the
separate scopes. Earlier selected campaigns and their older source identities are retained in the
[scoped evidence capsule](audit-evidence/memory-expressions/earlier-scoped-evidence.tar.gz).

The broad campaign completed in segments after WSL stopped during the sanitizer library suite. Its
original prefix and the resumed complete library suite and remaining tail retain separate logs and
process records. The original exit status is unknown; all 134 input hashes matched again before and
after resumption.

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
