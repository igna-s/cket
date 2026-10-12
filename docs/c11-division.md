# C11 integer division: quantum products and table-free synthesis

`div`, `ldiv`, `lldiv` and `imaxdiv` have partial quantum adapters for direct calls with pure
integer arguments. This advances [M2/M4/M7](c11-roadmap.md). General quantum product ABIs and
complete C11 library compatibility remain open.

## Standard values and reversible meaning

The reference contracts are N1570
[7.22.6.2 and 7.8.2.2](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf). The result has
`quot` and `rem` fields of the corresponding signed argument type; their order is not assumed. The
quotient truncates toward zero, and `numerator = quotient * denominator + remainder`. A nonzero
remainder has the numerator's sign. Division by zero and an unrepresentable quotient are undefined C
behavior. `MIN / -1` is rejected even when only `.rem` is projected.

The quantum extension preserves the arguments and computes result basis values into fresh storage:

```text
sum_(a,b) alpha_(a,b) |a,b>|0,0>
    -> sum_(a,b) alpha_(a,b) |a,b>|quot(a,b),rem(a,b)>
```

The reversible extension XORs each encoded output into its destination. It preserves relative
amplitudes and creates correlations; it performs no implicit measurement or reset. Output
conversions use C integer rules and Boolean truth conversion. A derived result retains dependencies
on its inputs until use or uncomputation permits cleanup. Explicit measurement must respect those
lifetimes.

Actual external system declarations are recognized. Private functions with the same name retain
their own behavior. Calls with entirely classical arguments continue through GCC/libc, including
ordinary once-only evaluation of argument side effects. Quantum calls require pure quantum
expressions and [captured CPU integer values](c11-bindings.md). CPU arguments may include increments
or ordinary calls outside coherent control; their values are evaluated once and retained for both
fields. Effectful quantum arguments still need sequenced capture (**P**).

The adapter is selected when at least one argument carries quantum data. Importing an entirely
classical product result into fresh quantum product storage now snapshots that result and prepares
its fields, including `quant div_t result = div(6,2)`. Resource limits still apply to the full
result; classical calls remain available.

A quantum product or projected value cannot initialize CPU storage implicitly. Its quantum fields
must be measured explicitly before their values can be used as classical scalars or assembled into a
classical structure. Indirect calls and general quantum product returns remain part of the pending
ABI.

For each field, the adapter restores the standard parameter type before constructing the division
expression. For example, narrowing `div(a,b).quot` to `uint8_t` does not change the call's signed
`int` domain or permit an undefined quotient. Initializing a fresh quantum `div_t` product and
projecting fields from the other three product types use the existing checked product machinery.
General product returns across separately compiled quantum functions remain **P**, independent of
these standard-call adapters.

## Power-of-two path

The generic table-free divider remains available under its existing definedness and resource
contracts. A constant divisor with magnitude `2^k` now has a separate path. Unsigned quotient bits
are a shifted view; remainder bits are the low `k` bits with zeros above them. These views need no
private arithmetic output word.

For a signed numerator, let `floor` denote an arithmetic right shift and `low` the discarded low
bits. For a positive divisor:

```text
quotient = floor + (numerator < 0 && low != 0)
remainder = low - (numerator < 0 && low != 0 ? 2^k : 0)
```

A negative divisor negates the quotient and leaves the remainder unchanged. This implements
truncation toward zero for negative nonexact division. Only the requested result bits need a private
output word, with a low-bit predicate when required. All workspace is uncomputed; the numerator is
preserved. The logical network scales with encoded width and uses no truth table. Multi-controlled
gates still need hardware decomposition and connectivity accounting.

Definedness is checked before this optimization. The path does not turn undefined signed overflow
into modular division. Range proofs are conservative: stronger correlated proofs for defined
programs remain **P**. These pure integer interfaces have no inherent incompatibility with
reversible computation.

## Resources and open cases

On the tested GCC 11.4.0 LP64 Linux target, `div_t` contains two 32-bit fields; `ldiv_t`, `lldiv_t`
and `imaxdiv_t` contain two 64-bit fields. A live one-bit input plus the latter complete products
already requires 129 public qubits. The current 128-qubit representation rejects that case (**R**).
Scalar projections with narrow destinations can fit and now generate power-of-two circuits for full
32/64-bit sources. Wider representations are a required future implementation, not a semantic
prohibition.

Native state simulation is limited to 24 qubits (**R**). Full `div_t` products therefore have
circuit-generation evidence, while small projections have complete state comparisons. General 64-bit
variable division still needs a wider remainder workspace and resource expansion (**P/R**). Sharing
the two field computations, general retained-history composition, effectful quantum argument capture
and full interprocedural product composition remain **P**. No hidden destructive operation is used
to reduce these costs.

[Unconditional scalar assignments](c11-history.md) can now version division/remainder results and
preserve old dependents. This does not extend the general product ABI or versioned branch/loop
contracts.

## Reproducible validation

`tests/validate_division.c` runs 990 checks per configuration:

- 300 complete phased arithmetic domains, with clean inverse and both table policies: 2/3/4-bit
  synthetic IR words, signed/unsigned divisors of magnitude 1/2/4/8, both control values, and full
  signed/unsigned eight-bit domains divided by two. Synthetic narrow words do not certify C bitfield
  support.
- 240 library joint states: four functions, three control profiles, five GCC levels and both builtin
  settings, each with both table policies. A quantum denominator ranges over 1/2; both fields and
  all private wires are compared with ordinary C references and relative phases.
- 80 signed numeric executions, 40 classical libc executions with argument side effects, 40 Boolean
  conversions and 10 private-homonym executions. Measurements and resets in numeric fixtures are
  explicit; they are separate evidence from the coherent state tests.
- 80 wide scalar circuit generations, 40 complete-product resource contracts and 160 negative
  diagnostics for zero, overflow and unbound CPU inputs during static export. Negative cases require
  the expected semantic diagnostic and reject infrastructure faults.

Three additional exported QASM circuits are compared with pinned QuEST 3.7.0: constant division with
both projections, coherent variable-denominator division, and signed rounding/remainder. These
extended the finite external corpus to 25 state comparisons and three sampled channels. The
[CPU-binding stage](c11-bindings.md) adds two state comparisons. The current corpus totals are
recorded in the [campaign archive](audit-evidence/README.md). Wide generation is not counted as
externally validated execution or hardware execution.

```bash
export CC=gcc-11 CXX=g++-11
bash scripts/build.sh
build/validate_division
bash scripts/build-validation.sh
build/validate_qasm
export CKET_FUZZ_CASES=128 CKET_FUZZ_SEED=20261012
bash scripts/verify.sh
```

The complete normal/ASan/UBSan campaign and source hashes are published in
[audit-evidence](audit-evidence/README.md). The historical paper study retains its own fixed
revision and measurements.
