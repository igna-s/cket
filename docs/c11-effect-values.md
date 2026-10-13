# Value-producing quantum updates

This implements another part of C11 integer expressions. The [complete C11 roadmap](c11-roadmap.md)
remains open.

## Implemented contract

Supported scalar quantum updates now produce their C expression value when consumed by a quantum
initializer, quantum assignment or coherent statement condition. Prefix `++`/`--` return the updated
value; postfix returns the value before the update. Simple and compound assignments return the
assigned value. A comma expression evaluates its operands in order and returns the right operand's
value. Existing integer promotion, conversion, defined-domain and resource contracts still apply.

For example:

```c
#include <quantum.h>

int main(void) {
    quant uint8_t x = 3;
    quant uint8_t old = x++;
    unsigned before = measure(old);
    unsigned after = measure(x);
    return before != 3 || after != 4;
}
```

GCC's shared `SAVE_EXPR` nodes retain reads that precede a later overwrite. The frontend protects
already serialized reads until their consuming computation is emitted. An update that would
invalidate one of those reads preserves the previous owner through
[versioned history](c11-history.md). Each update executes once in the native program and appears in
the same order in QIR. Pending reads also prevent premature automatic cleanup. CPU bindings for a
consumer are kept separate from bindings for preceding updates.

The result is a computational-basis value in quantum storage. Producing an old or updated result can
correlate it with the operand and its history; it does not clone an arbitrary quantum state. No
implicit measurement or reset implements an overwrite. Unknown root history remains a joint output
when no checked inverse can clean it. The existing current/peak qubit reports include that cost.
This first implementation does not promise minimal retained history or minimal circuit size.

`_Bool` follows C conversion rules: increment yields true, including when the old value is true.
Decrement computes the promoted integer subtraction and converts back to `_Bool`; zero becomes true
and one becomes false.

## Sequencing and current limits

Conflicting quantum writes/reads in unsequenced binary operands are rejected. This includes GCC's
hoisted Boolean postfix snapshot representation. Comma sequencing remains distinct from a binary
operator's operand order. Defined operations on disjoint owners can use the frontend's selected
evaluation order; this does not give a C result to undefined behavior.

The restrictions below remain part of the unfinished objective:

| Class | Remaining restriction                                                                                                | Required work                                                                                             |
| ----- | -------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------- |
| P     | Effects in value-producing conditional selection, logical AND and logical OR are rejected                            | Schedule only selected effects, preserve branch values/history and validate combinations                  |
| P/R   | Floating/complex and general memory measurement operands remain pending                                              | Extend the [integer observation contract](c11-observations.md) with encodings, layouts and lifetime rules |
| P     | General calls, effectful return templates, dynamic-index value reads and aggregate expression results                | Complete function/CFG and memory contracts                                                                |
| P     | Arbitrary mixed CPU effects and full source-level sequencing analysis                                                | Capture CPU effects once with their C sequencing; retain information lost before the plugin               |
| P/R   | Runtime loop history and general dynamic memory                                                                      | Bounded ownership/iteration analyses and explicit allocation contracts                                    |
| R     | Retained versions and synthesis/simulator workspace                                                                  | Report and check existing budgets; exceeding the native simulator is distinct from unsupported semantics  |
| S     | An implicit CPU result from an unmeasured quantum value, or arbitrary coherent erasure without preserved information | Require explicit measurement or a valid information-preserving transformation                             |

The language reference is the
[WG14 N1570 C11 draft](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf), sections 6.5,
6.5.2.4, 6.5.3.1, 6.5.16 and 6.5.17. This is finite validation on the selected GCC target, not a
claim that GCC exposes every source-level sequencing violation to the plugin.

## Verification

`tests/test_effect_values.inc`, included by `tests/validate_history.c`, checks native old/new
values, every assignment form, comma ordering and fixed array/pointer aliases at five GCC
optimization levels. Complete joint-state comparisons cover Boolean prefix/postfix, assignment and
comma results, retained history, correlations, relative phase and private workspace, with table
synthesis enabled and disabled. Negative cases distinguish unsequenced conflicts from pending lazy
effects and classical-only measurement operands. Supported computed integer observations have a
[separate channel contract](c11-observations.md).

`tests/validate_qasm.c` also exports postfix, prefix and comma fixtures for independent QuEST
simulation. These export comparisons complement the separately computed language/state references;
circuit generation alone is not counted as a complete-state comparison. Completed campaign counts
and source identities belong to the [current evidence archive](audit-evidence/README.md).

Run `CC=gcc-11 CXX=g++-11 bash scripts/verify.sh` for the normal and ASan/UBSan campaigns.
