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

Each active count bit controls a stage selecting the unchanged value or a shift by a power of two. A
variable stage writes fresh wires and retains its predecessor. Constant count bits select wire views
without an additional stage register. The compiler copies the final basis value to the requested
destination, reverses the recorded plan and releases clean private registers. The shift
implementation emits X-family reversible gates, with no implicit measurement or reset. Source-level
measurement/reset in integration programs is explicit and has its usual channel semantics.

For active working width `w` and `k` nonconstant count bits, this construction uses O(`w*k`) logical
gates and O(`w*k`) retained private workspace. It does not enumerate all input bit patterns. Right
shift may need the source's full active width even if the output is narrow; left shift can discard
high bits only in the copied result while preserving its original input.

The physical wire mask currently represents 128 qubits; full state simulation is limited to 24.
Complete 8- and 16-bit variable-shift circuits are generated without tables. The current
stage-retaining construction for complete 32-bit inputs exceeds the physical budget and diagnoses
exhaustion. This is R for the current implementation, not a proof that wider shifts are physically
impossible. Better workspace scheduling and wider backends remain P/R.

## Evidence and limits

`build/validate_shifts` runs 55 checks:

- Twenty complete joint-state comparisons across ten small domains, with finite synthesis
  enabled/disabled, nontrivial relative phase, unsigned modular truncation, signed extension, 64-bit
  promoted types, narrowing, input-correlated counters, open coherent control and a clean inverse.
- Six wide synthesis/resource checks: both shifts at 8/16/32 bits. The 8/16 cases generate circuits;
  the 32-bit cases diagnose the resource limit.
- Fourteen definedness checks in both routes: negative/out-of-width counters, signed negative left
  operands, signed overflow and superpositions containing invalid arithmetic.
- Five executions combining a typed function, a fixed array, a static loop, conversions, arithmetic,
  explicit measurement/reset and C comparison.
- Ten frontend exports of both shifts on 20 relevant input bits across `-O0`, `-O1`, `-O2`, `-O3`,
  `-Os`, with backend truth tables disabled.

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
