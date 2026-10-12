# In-place integer updates: contracts and evidence

This implements another part of [M2](c11-roadmap.md). Complete expression-valued effects,
destructive history, general memory and library compatibility remain required. This is not a
completion claim for C11.

## Two table-free certificates

An assignment may update its existing scalar register when the engine proves one of the following
transformations before emitting gates:

- A permutation of all destination bits, with optional bit inversions. Every output must refer to
  exactly one input bit, and every input must appear once. This covers complement, fixed word
  rotations and supported bit rearrangements.
- An affine word transformation `f(x) = a*x + b (mod 2^w)`, for `2 <= w <= 64` and odd `a`.
  Supported expression trees use integer constants, the destination, suitable casts,
  addition/subtraction, negation, complement and multiplication by a constant expression. Combining
  these expressions does not require a table.

Bit certificates are derived with the input's basis/zero shortcuts disabled: a known preparation is
not a certificate of a permutation on the whole word. The engine also checks typed arithmetic
definedness on reachable input support. The resulting circuit can be a total reversible extension
outside that support; it does not assign C results to undefined arithmetic there.

The destination remains its only quantum value source. Coherent controls use separate Boolean
storage, and existing ownership, dependency and inverse certificates remain enforced. Updating a
derived result still needs a composable inverse certificate and remains pending (**P**).

## Why odd multiplication is reversible

An affine map is invertible modulo `2^w` when `a` is odd: its coefficient has a modular inverse. Its
inverse is `a^-1*y - a^-1*b` modulo that same word size. An even coefficient is not injective on the
complete word space. A useful restricted-support extension or retained history may still implement
the C assignment; these mechanisms remain pending, rather than physically impossible.

The generated multiplier writes no copy. Let `delta = a - 1`, which is even. For input bit `i`,
taken in descending order, conditionally add `delta*2^i`. That addition affects only bits above `i`,
so its control still contains the original input bit. The total added value is `delta*x`, producing
`a*x`. Finally add `b`. Each component is a controlled reversible addition.

For coefficient minus one, bitwise complement followed by addition of `b+1` implements `-x+b`. Bit
permutations use controlled swaps and inversions. Both routes use **zero private qubits**, preserve
arbitrary amplitudes by their unitary gate construction and insert no measurement or reset.

The current carry-prefix implementation has at most cubic logical-gate growth in word width for the
affine path; bit rearrangement needs at most three gates per swap and one per inversion. Hardware
decomposition, connectivity and noise are separate costs. Gate and proof budgets remain explicit
resource restrictions (**R**), and the 24-qubit native simulation limit remains distinct from the
128-qubit circuit representation limit.

## C types and Boolean normalization

Promotions and conversions are checked before the word certificate. A signed operation may not
overflow on evaluated support. Conversion to a narrower signed type follows the declared GCC target
profile; it is not a permission to overflow the wider signed arithmetic. GCC documents word
reduction for these conversions in its
[integer implementation contract](https://gcc.gnu.org/onlinedocs/gcc-11.2.0/gcc.pdf). The tested
target is GCC/G++ 11.4.0 on the declared Linux/WSL configuration.

`_Bool` requires its own truth certificate. It is not a one-bit modular integer for C expression
semantics. For example, `b = -b` preserves its truth value, `b = !b` inverts it, and `b *= 2`
preserves it after integer promotion and truth conversion. Logical operations, suitable casts and
comparisons can certify a single input bit or its complement. Nonzero constant multiplication of an
operand proved to be zero or one preserves its truth value; typed definedness must still hold. An
intermediate narrowing cast cannot be silently discarded.

GCC can rewrite Boolean XOR into an integer comparison. The certificate covers zero comparisons and
equality/inequality between proved zero-or-one operands, including a literal on the left. This
preserves valid array/function programs that use `b ^= true`.

## Corrected no-tables policy

The old in-place permutation path ignored `--no-tables` and enumerated up to `2^w` inputs even when
the caller had disabled that route. The corrected policy first tries the certificates above. If
neither applies, disabled tables cause an explicit diagnostic; enabled tables retain the bounded
fallback.

The reproduced eight-bit `x *= 3` case previously emitted 1045 multicontrolled X gates through
enumeration. Its structured update now uses 28 logical gates: seven CNOT, six Toffoli and fifteen
larger multicontrolled X gates, with no private qubits. Preparation and explicit measurement are
additional operations. These are logical counts, not a physical gate or timing comparison.

## Finite validation

`tests/validate_inplace.c` runs **271 checks** per configuration:

- 192 complete joint-state comparisons: 12 transformations at widths 2, 3, 4 and 8, two synthesis
  settings, and both open and closed coherent controls. Nontrivial phase, ordinary-C value
  references, inactive branches, absence of private storage/destructive channels, and an inverse are
  checked. Signed words are prepared through Fourier from zero to cover their entire representation.
- 48 complete Boolean-state comparisons: 12 truth, logical, comparison and multiplication cases, two
  settings and two control patterns, with phase and inverse. Cases include even multiplication,
  signed `INT32_MIN` and `UINT64_MAX`.
- Two regression checks for the enumeration policy on a valid permutation that currently needs the
  finite fallback; six rejections of noninjective or undefined arithmetic; and three generated
  16/32/64-bit resource cases.
- Five C executions combining a mutable quantum procedure, pointers to fixed array elements, a
  static loop, arithmetic, rotation and Boolean control at the five GCC optimization levels.
- Fifteen wide C exports at those levels, combining multiplication, affine composition, complement
  and negation on 16/32/64-bit words with tables disabled.

Wide generation checks do not validate those complete large states. The separate QASM campaign adds
independent QuEST states for controlled affine arithmetic, rotation/complement and signed affine
updates after Fourier. Its current suite has 22 state comparisons and three sampled
measurement/reset channels.

Run `CC=gcc-11 CXX=g++-11 bash scripts/verify.sh` for normal and ASan/UBSan checks. The
[archive](audit-evidence/README.md) identifies completed campaigns by source hash. The papers keep
their separate historical evaluation.

## Remaining coverage

General nonlinear injective functions, variable quantum/CPU factors, full dynamic indexed updates,
all signed range correlations, expression-valued assignments and increments, and automatic
retained-history transformations remain **P**. Wider representations and proof/gate/storage budgets
are **P/R**. The rejection of a noninjective full-space map does not rule out a proven support
extension. Erasing distinguishable inputs without retained information or an explicit channel is an
incompatible context (**S**), not a reason to remove C assignment or a whole library family from the
final objective.
