# Classical inputs, quantum circuits and product preparation

The compiler now captures CPU integer inputs in quantum expressions and in the four standard
integer-division adapters. This advances M2/M4/M7; it does not complete the C11 library, quantum
function ABI or memory model.

## Values and lifetime

A CPU input is a classical circuit parameter, not a register holding an unknown quantum state. Its
typed value specializes an expression before gate synthesis. No measurement or reset is introduced.
The quantum arguments remain available, and each generated computation stores the concrete
expression used to build its inverse. Changing the original CPU variable afterward does not change
that stored expression. Its classical values remain in the definition until measurement, explicit
reset or safe uncomputation clears the definition. This costs classical memory rather than extra
parameter qubits; the existing expression/cache budgets still apply.

`div`, `ldiv`, `lldiv` and `imaxdiv` snapshot each CPU argument with GCC `SAVE_EXPR`, including
ordinary function calls and increments outside coherent control. Classical leaves inside a pure
quantum argument are also snapshotted, so both result fields use the same argument values. C
argument evaluation order remains unspecified where C permits it; repeated evaluation of a single
argument is not introduced. Effectful quantum arguments still require sequenced capture (**P**).

Ordinary CPU effects cannot execute separately on unmeasured coherent branches. The adapter rejects
such effects under quantum control; a coherent effect model or an explicit measured boundary is
required. Pure arithmetic inside a coherent expression arm is lowered lazily instead of evaluated
eagerly on the CPU. This preserves inactive-branch definedness, including a guarded `1/d` when `d=0`
and the branch is never active. Coherent access to arbitrary CPU memory remains **P/R**.

GCC's C frontend inserts scalar UBSan guards before the quantum pass. Recognized division and shift
guards around pure arithmetic in coherent arms are handled by circuit-domain validation. Inactive
arms retain C short-circuit semantics; reachable invalid arithmetic is rejected. CPU callbacks,
increments and volatile accesses still require the effect contract. Ordinary CPU computations keep
their sanitizer instrumentation.

The runtime carries integer inputs in 64-bit values and converts them to the declared 1..64-bit
types. Unsigned narrowing uses low bits; signed interpretation follows the declared two's-complement
GCC target. Boolean inputs use comparison with zero. Floating/complex quantum data and the full
conversion/effect matrix remain open.

## Products from classical values

A fresh quantum integer structure can be initialized from a classical structure value, including a
function result and nested structure/array members. The classical value is evaluated once into a
temporary, then its fields are prepared in basis states. This is value preparation; it does not copy
an unknown quantum state. A quantum source requires a separately implemented ownership/copy
contract. Padding, unions, bitfields, dynamic extents and general quantum product returns remain
**P**.

This also fixes a reproduced bug in nested aggregate initializers. A two-element array initialized
by two classical structure-returning calls previously omitted both calls and prepared four zeros.
The regression now executes both calls and prepares the requested Boolean fields `1001`. The
published `d18a873` compiler produced `calls=0; fields=0000`; the corrected compiler produces
`calls=2; fields=1001`.

To reproduce the initializer regression, save this program as `initializer-regression.c`:

```c
#include <quantum.h>
#include <stdio.h>

typedef struct {
    bool a, b;
} pair;

static unsigned calls;

static pair make_pair(unsigned n) {
    ++calls;
    return (pair){n & 1, !(n & 1)};
}

int main(void) {
    quant pair p[2] = {make_pair(1), make_pair(2)};
    unsigned a = measure(p[0].a), b = measure(p[0].b);
    unsigned c = measure(p[1].a), d = measure(p[1].b);
    printf("calls=%u; fields=%u%u%u%u\n", calls, a, b, c, d);
    return calls != 2 || a != 1 || b != 0 || c != 0 || d != 1;
}
```

With a built compiler, `build/cket initializer-regression.c --run -O2` must print
`calls=2; fields=1001` and exit successfully. The same source run with the published `d18a873`
compiler prints `calls=0; fields=0000` and exits with failure. The committed binding suite checks
the corrected evaluations and fields at all five optimization levels, with and without builtins.

`quant div_t result = div(6,2)` now compiles as classical result capture followed by quantum basis
preparation. Its two 32-bit fields exceed the 24-qubit native statevector limit (**R**). Two 64-bit
fields plus a live quantum input also exceed the current 128-qubit representation (**R**). These
limits are not semantic prohibitions on the types.

## IR, validation and export

`qbind` declares typed CPU parameters and an instruction template. The native runtime binds their
values before applying the ordinary validated instruction. `quncompute` with `saved:true` uses the
stored concrete expression; it requires an existing definition and unchanged quantum dependencies
and controls.

An unbound template receives structural validation of types, storage and schema. It has no numerical
domain or circuit-correctness certificate. Template contexts cannot produce QASM or resource reports
and cannot simulate a state. Numerical definedness and reversible synthesis are checked after
binding. Static exports without supplied values fail explicitly.

The C APIs `qc_bind_operation`, `qc_apply_bound` and `qc_bind_document` support specialization. The
backend accepts `--bindings values.json`. Its file is an ordered array with one row for every
`qbind` instruction; instruction indices start at zero:

```json
[
  {
    "instruction": 3,
    "values": [
      2
    ]
  },
  {
    "instruction": 5,
    "values": [
      2
    ]
  }
]
```

Each row follows that instruction's parameter-descriptor order. Missing, unused, duplicate,
unordered and mistyped rows/descriptors are rejected. Indices depend on the emitted IR, not source
line numbers. Supplied values must match the corresponding CPU evaluation; specialization does not
execute or prove the behavior of host callbacks. CPU loops and control markers still require native
execution; binding values does not flatten them into an equivalent circuit.

```bash
export CC=gcc-11 CXX=g++-11
bash scripts/build.sh
build/cket program.c --run
build/cket program.c --emit-ir -o program.json
build/cket_backend --emit-qasm program.json --no-tables --bindings values.json
build/validate_bindings
```

Each operation accepts at most 128 parameters. Encodings are at most 64 bits; specialization is
bounded to 100,000 JSON tokens, nesting depth 128 and 64 MiB of text. These are resource contracts,
not coverage of every C11 construction.

## Evidence

The binding suite distinguishes complete phased states with saved inverse, typed endpoint
conversions, rejection before mutation, C executions/diagnostics at five GCC levels with both
builtin settings, and complete-state comparisons of specialized C documents. Measurements in C
execution fixtures are explicit. The external QASM corpus adds CPU-bound division and cleanup after
CPU mutation with pinned QuEST 3.7.0. Current corpus totals are recorded in the
[campaign archive](audit-evidence/README.md). Neither structural validation nor wide generation is
counted as externally validated hardware execution.
