# C11 character classification and case conversion

`ctype.h` now has partial quantum adapters for all fourteen C11 interfaces: `isalnum`, `isalpha`,
`isblank`, `iscntrl`, `isdigit`, `isgraph`, `islower`, `isprint`, `ispunct`, `isspace`, `isupper`,
`isxdigit`, `tolower` and `toupper`. This is a stage of M2/M7; the complete library and C11 goal
remain open.

## Declared profile and C semantics

The current target is GCC 11 on Linux/WSL with glibc, ASCII, eight-bit bytes and 32-bit `int`. New
quantum computations require the `C` locale. The compiler emits a CPU precondition that queries
`LC_CTYPE` without changing it. Other locales are diagnosed as pending immutable locale snapshots
(P), rather than silently using the `C` rules. Concurrent locale mutation and general quantum
concurrency require a separate contract (P).

[N1570, section 7.4](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf) requires an
argument representable as `unsigned char`, or `EOF`. On this target that means `-1` or `0..255`. The
backend proves this requirement on reachable support before emitting gates. An invalid reachable
argument is diagnosed; an unreachable lazy arm does not impose an argument-domain requirement. An
explicitly converted `(unsigned char)` argument uses that C conversion. Values outside the required
domain have undefined C behavior (S), whereas failure to prove a valid domain is an analysis
limitation (P/R).

Classification results preserve glibc's `int` masks, including values such as `1024` for `isalpha`.
They are not silently normalized to one. Conversion to `_Bool` applies the ordinary zero/nonzero
rule. `tolower` and `toupper` return the original argument when the corresponding case conversion is
absent; `EOF` is preserved and has no classification. No implicit measurement or reset makes any of
these operations work.

## Coherent computation and resource contract

Calls compute a result while preserving the source. A result can therefore be entangled with its
argument; this does not clone a quantum state. Scheduled scalar consumers support quantum increments
in arguments with the ordinary old/new value rules, exactly once.

Classification intervals have a disjoint binary-prefix cover. Each prefix controls an XOR into one
predicate bit; the result's mask bits refer to that same predicate. Case conversion preserves all
input bits except the ASCII case bit, which changes only within the appropriate interval. The
interval count is fixed by the profile and the prefix cover grows with byte width. Neither synthesis
route enumerates the input domain or uses a truth table. Temporary gates are reversed and private
storage returns to zero.

Saved definitions include the selected profile. An inverse uses that original definition, even if
classical code changes locale afterward. A subsequent new call checks the locale again. This is
distinct from consulting mutable libc state during cleanup.

Native simulation is limited to 24 physical qubits; synthesis has separate physical, gate and
proof-work budgets (R). A full-width `int` result can exceed the native simulator budget while a
Boolean consumer or narrower explicit conversion fits. This is a resource restriction, not physical
incompatibility.

## Header integration and current limits

The compiler's header includes the native system header, then routes function macros through
distinct attributed declarations with native libc assembler aliases. Classical arguments still call
libc, and their side effects execute once. Including `ctype.h` alone does not give quantum
implementation credit. Ordinary function forms that survive GCC parsing are recognized by their
system declaration, including glibc inline case-conversion declarations. Private functions with
matching names are not library adapters.

Mixed CPU call operands can require an explicit classical snapshot before entering a quantum
expression. General automatic capture of CPU calls and effects remains P.

GCC may fold some builtin function forms before the plugin sees them. The tests cover macro calls
with builtins enabled and parenthesized function calls with builtins disabled. Complete
original-source provenance and every earlier GCC transformation remain P. Indirect calls remain P.
Independently compiled unitary modules currently reject CPU locale preconditions; carrying an
immutable profile through the module ABI remains P. General locale-dependent character sets, locale
descriptors and wide-character adapters remain P/R.

## Finite validation

`tests/validate_ctype.c` defines 635 checks:

- 560 complete phased states cover all 256 byte values, both call forms, five GCC optimization
  levels and both synthesis policies. Classifications are compared against the exact native libc
  mask. All eight bits of both case mappings are contrasted individually. Every private wire is
  included in the amplitude comparison; generated circuits are not counted as states.
- 50 integrated programs cover EOF, scalar effects, arrays and pointer borrows, quantum functions,
  static loops, lazy domains, explicit observation, saved inverses after locale mutation and native
  classical calls.
- 15 diagnostics contrast reachable invalid domains and the pending non-C locale contract.
  Infrastructure failures cannot satisfy these rejections.
- 10 ordinary-GCC differentials compare all fourteen classical interfaces on EOF and every byte, raw
  return values and once-only argument effects.

Two additional QuEST fixtures independently contrast exported classification and conversion
circuits, including phases and a function with a memory borrow. This finite evidence does not close
full locale support, all combinations or the complete C11 goal.

```bash
export CC=gcc-11 CXX=g++-11
bash scripts/build.sh
build/validate_ctype
bash scripts/build-validation.sh
build/validate_qasm
```

The full normal and ASan/UBSan campaign is run by `scripts/verify.sh`. Its completed transcript and
exact source hashes belong in [`audit-evidence`](audit-evidence/README.md), separately from the
fixed historical paper study.
