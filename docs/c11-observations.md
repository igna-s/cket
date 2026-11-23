# Explicit observation of integer expressions

`measure` now accepts supported quantum integer expressions as well as storage references. This
advances the [complete C11 roadmap](c11-roadmap.md); floating/complex encodings, general memory and
the full standard library remain unfinished.

## Value and channel contract

The classical result has the argument's C integer type, including promotions and explicit casts.
`measure(x++)` produces the old value and executes the update once before observation. Prefix,
assignment and comma effects retain their [ordered value contract](c11-effect-values.md). Type
inspection through `_Generic` and `typeof` does not evaluate an integer operand. Fixed helper return
types preserve its representation despite GCC's ABI argument promotions.

A compatible full-width storage reference uses the existing storage measurement. A computed value
uses the input-preserving embedding `|x>|0> -> |x>|f(x)>`, followed by the explicitly requested
measurement of the result. Conditional on observing `m`, the inputs are projected onto the basis
states for which `f(x)=m`. Relative phases and coherence **within that fiber** remain intact. An
explicit Boolean cast therefore observes a predicate, rather than each bit of its integer input.

The result temporary retains its computed definition through `qobserve`. Its unchanged input basis
values allow the inverse computation after observation, restoring the temporary to zero before
release. This inverse preserves the projected input state; it does not undo the measurement. There
is one requested channel, with no extra source measurements or resets for cleanup.

```c
#include <quantum.h>

int main(void) {
    quant uint8_t x = 3;
    int sum = measure(x + 2);
    unsigned old = measure(x++);
    unsigned now = measure(x);
    return sum != 5 || old != 3 || now != 4;
}
```

The compiler keeps explicit conversions. Narrowing followed by widening is not treated as an
identity storage reference. Generated identity/storage recognition still depends on GCC's lowered
trees; complete source-level value/effect preservation across all GCC transformations remains P.

## Storage and resources

Live inputs keep their owner and basis-value dependencies. Unconditionally dead derived inputs can
be inverted after observing the result. Dead root inputs can end as retained joint outputs: known
basis roots have checked unitary restoration, while unknown roots stay allocated until context end.
Scope cleanup does not trace out that retained state. CPU-loop and branch input lifetimes retain
their existing restrictions; repeated temporary observations clean their own storage each time.

A conservative nonnegative bound can omit provably zero high result bits. The original typed
arithmetic is still checked for definedness before narrowing the private carrier. The CPU result
retains its original C type. For example, an integer logical expression can use one observed qubit
while returning a C `int`. Unsupported bounds use the full result width. This is not a complete
range solver or a promise of minimum qubits.

Physical storage, retained histories, synthesis workspace and the native simulator's 24-qubit limit
remain explicit resource requirements R. Large circuits generated within the physical backend limit
(128 qubits in baseline `5c5a965`; expanded to 1024 in Wave 1A engine `QC_MAX_PHYSICAL`, pending
Wave 1B/2) are distinct from complete-state validation. Frontend/ABI floating/complex measurement
(beyond Wave 1A engine circuits), general computed dynamic-index reads, mixed CPU effects in lazy
arms, coherent measurement control and general function/CFG lifetimes remain P/R. Source borrows
require the existing mutable capability; arbitrary classical operands do not become quantum states
through `measure`.

Supported scalar effects in lazy arms follow the [saved-decision contract](c11-lazy-values.md),
including their use in explicit integer observations.

## Finite evidence

`tests/test_observations.inc` contributes 730 checks to the history suite:

- 260 channel contracts: full conditional amplitudes, phases inside both fibers, unchanged input
  epochs, the computed inverse, clean release, and rejection before mutation.
- 120 C executions at five GCC levels: signed types, conversions, arithmetic, old/new results, fixed
  aliases, loops, last-use histories and unevaluated type contexts.
- 320 conditional states from compiled C: integer logic and Boolean conversion of derived byte data,
  with complete projected states and zero private garbage under both synthesis policies.
- 30 diagnostics for pending encodings/control/effects, unsequenced accesses and classical-only
  operands, with crashes and infrastructure failures excluded.

Three additional [QuEST fixtures](../tests/validate_qasm.c) compare observation channels through
8192 transcripts per implementation, including interference after predicate observation and postfix
ownership. Channel comparisons complement the separately computed language/state references.
Completed full-campaign results and source hashes belong to the
[evidence archive](audit-evidence/README.md).

The formatter also rejects loss of words or inline-code content during Markdown formatting.
`scripts/test-format.py` reproduces the unescaped-OR table failure and checks repaired tables and
list formatting. This guard checks content loss; it is not a proof of Markdown semantic equivalence.
