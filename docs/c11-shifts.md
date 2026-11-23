# Variable integer shifts: contract and verification

This is partial progress toward M2, not closure of complete integer expressions or C11
compatibility. The implementation is in `src/engine/arithmetic.inc`; the combined validator is
`tests/validate_shifts.c`.

## Value contract

The frontend keeps C integer promotions and the left operand's result type. The shift count has its
independently promoted type. It must be nonnegative and strictly less than the promoted left
operand's width, even when an outer cast or destination has fewer bits. The retained expression must
be defined over its reachable quantum support.

Unsigned left shift is modular. Signed left shift requires a nonnegative operand and a result
representable in the signed type. Negative signed right shift uses arithmetic sign extension in the
declared GCC/two's-complement profile. This is a target choice for C11's implementation-defined
behavior, not a claim that every C implementation makes that choice. These rules follow
[N1570, 6.5.7](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf).

Both inputs remain available. The circuit creates the computational-basis correlation
`|x,n,0> -> |x,n,shift(x,n)>`; it does not clone an arbitrary unknown state. The final value can be
narrowed or converted to `_Bool` under the existing cast rules. Right shift reads the original high
bits before that conversion; signed extension likewise uses the original sign.

## Circuit and resources

Variable shifts now use a reversible bit multiplexer. For each representable shift position below
the declared promoted operand width, controlled X gates XOR the selected source bits into a private
result. There are at most 64 positions; the construction enumerates positions, not all input word
values. Unrepresentable positions in a compressed count view are skipped, preventing low-bit
aliases. Fixed counts retain the existing wire-view path.

For promoted width `w` and requested output width `r`, the new path uses O(`w*r`) logical
multi-controlled X gates and an `r`-bit private result, in addition to workspace already needed to
lower its operands. The compiler copies this result to the destination, reverses the plan and
releases clean private storage. It no longer allocates a full word per variable count bit. Hardware
decomposition/connectivity costs of multi-controlled gates are separate from these logical counts.

The inputs and relative phases are preserved. Right shift still reads the original high bits and
sign before narrowing. The implementation introduces no measurement/reset. Invalid inactive counts
have a total XOR extension; this does not permit reachable undefined C shifts.

The gate representation remains limited to 128 physical qubits in baseline `5c5a965` (expanded to
1024 in Wave 1A engine `QC_MAX_PHYSICAL`, pending Wave 1B/2). Default dense simulation is limited to
24; [sparse execution](simulacion-dispersa.es.md) supports wider bounded-support states. Complete
8/16/32-bit variable-shift circuits generate without tables. In baseline `5c5a965`, two public
64-bit words plus a separate counter already exceed 128 wires, independently of this workspace
improvement; narrow results and the Wave 1A/1B/2 resource expansion retain their separate contracts.

## Boolean consumers and integer conversions

Direct truth conversion, logical negation, and equality/inequality with zero in either operand order
now test the surviving source bits without constructing a full shifted word. Count patterns are
disjoint; controlled zero tests compute one private predicate bit, then the existing inverse cleans
it. Left shift tests the low bits that survive modular truncation. Right shift includes the original
high bits and the declared arithmetic sign extension. Definedness is still checked first.

Non-Boolean integer casts now request only their needed result bits from the child expression. An
intervening byte conversion therefore retains its own zero/nonzero meaning; it is not replaced with
truth of the unconverted full word. Logical negation must inspect the complete converted operand
even though C gives `!` an `int` result with values zero or one. The arithmetic fallback previously
narrowed such an operand to one bit in some nested shift/cast expressions; that numeric bug is
corrected and its exact no-table reproduction is retained.

`shift_predicate` in block 02 uses a 64-bit source and a count in superposition, computing both
`(value << count) != 0` and `(bool)(uint8_t)(value << count)`. Its circuit uses 85 physical wires.
The [current evidence](audit-evidence/shift-truth-v3/README.es.md) separates those executions from
the three previous frontend resource failures and the numeric fallback regression.

## Evidence and limits

`build/validate_shifts` runs 203 checks:

- Twenty complete joint-state comparisons across ten small domains, with finite synthesis
  enabled/disabled, nontrivial relative phase, unsigned modular truncation, signed extension, 64-bit
  promoted types, narrowing, input-correlated counters, open coherent control and a clean inverse.
- Six wide synthesis checks: both shifts at 8/16/32 bits now generate circuits without tables; their
  pure-register cases fit at most `3*w + count_bits` physical wires.
- Sixteen additional complete sparse joint-state domains across both synthesis policies: all 256
  byte labels and eight shift amounts; selected 256-label 32/64-bit source domains with every
  applicable 32/64-bit shift amount; full 32-bit and narrow 64-bit results, signed right extension,
  coherent controls, source/result phases, source preservation and a clean inverse. Their finite
  source domains are explicit in `tests/shift_state.inc`; they do not cover every wide input value.
- Another 132 sparse joint-state domains cover six truth forms, four declared source labels at
  8/32/64 bits, all counts of the promoted 32/64-bit word, byte conversions, both synthesis
  policies, coherent controls, phases, source preservation and a clean inverse. The signed labels
  are -2, -1, 0 and 1; unsigned labels use the corresponding width's modular encoding. Eight-bit
  unsigned left inputs are explicitly promoted to unsigned32 so every selected count is defined.
- Fourteen definedness checks in both routes: negative/out-of-width counters, signed negative left
  operands, signed overflow and superpositions containing invalid arithmetic.
- Five executions combining a typed function, a fixed array, a static loop, conversions, arithmetic,
  explicit measurement/reset and C comparison.
- Ten frontend exports of both shifts on 20 relevant input bits across `-O0`, `-O1`, `-O2`, `-O3`,
  `-Os`, with backend truth tables disabled.

The [previous targeted evidence](audit-evidence/shift-mux-v1/README.es.md) separates source
identities, normal/sanitizer builds and the actual previous compiler rejection. The
[variable_shift example](../examples/blocks/02/variable_shift.c) now compiles and executes with 105
physical wires in sparse mode; the same source exceeds the physical budget on the previous 159-input
compiler. Run it through `bash scripts/run-block.sh 02` after building.

Two additional QASM cases compare small generated states independently with QuEST: an unsigned
variable left shift with relative phase and a signed variable right shift followed by Boolean
conversion. The complete external state/channel counts are recorded in the
[current campaign archive](audit-evidence/README.md). Wide circuit generation is labelled separately
from state validation.

Run `CC=gcc-11 CXX=g++-11 bash scripts/verify.sh` for normal and ASan/UBSan campaigns. The archive
under `docs/audit-evidence/` identifies the tested working tree and results; the paper's older
eleven-case campaign is preserved.

The range proof is conservative: correlated expressions can be defined even when interval bounds
cannot prove that fact. Such cases need stronger analysis or another valid path; rejection by this
path is an implementation limitation. General expression-valued side effects and noninjective
`<<=`/`>>=` still need sequencing and retained-history work. They are not made valid by inserting a
hidden destructive channel, and this update does not declare M2 complete.
