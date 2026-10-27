# `|c>`: C with high-level quantum data

**A new quantum variant of C for GCC-based compilers.**

[![Verification](https://github.com/igna-s/cket/actions/workflows/verify.yml/badge.svg)](https://github.com/igna-s/cket/actions/workflows/verify.yml)
[![Version](https://img.shields.io/badge/version-1.0.0--rc.3-orange)](docs/releases/v1.0.0-rc.3.es.md)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

`|c>` is an experimental C11 extension. Add `quant` to supported integer declarations, write
expressions and coherent conditions, and let the compiler build reversible circuits and clean
temporary workspace. Classical C code still runs on the CPU; quantum operations run in a linked C
statevector simulator, with dense and opt-in sparse execution modes.

**Ready-to-run compiler package:** the
[updated Windows/WSL SDK](output/sdk/cpu-returns/README.es.md) includes the corrected compiler and
37 rebuilt programs. Run `output\sdk\cpu-returns\run_sdk_demo.cmd`, or compile a C file with
`output\sdk\cpu-returns\compile_and_run.cmd program.c`. The
[read-only borrow correction](docs/c11-readonly-borrows.md) allows shared read-only aliases and
rejects writes through casts. Its example checks phase interference. The
[delivery guide](docs/entrega-utilizable-2026-10-26.es.md) lists working examples and open limits.
This remains an experimental candidate with incomplete everyday-C integration.

The [checked function-entry correction](docs/c11-function-entries.md) rejects classical prototypes
and pointers that could reach an empty quantum template stub. A remaining native entry reports an
explicit error when called through a foreign/dynamic ABI. Ordinary CPU callbacks keep their
classical behavior. The general integration gate remains open.

[Structured CPU early returns](docs/c11-procedure-returns.md) now work in void quantum procedures,
including CPU loops. They finish lexical cleanup and continue the caller. Run
`output\sdk\cpu-returns\run_return_demo.cmd` for the new example with phase interference. A separate
correction removes path-local known-value claims that caused valid CPU branches to fail structural
verification. General CFG and owned early results remain pending.

The compiler uses GCC's real C frontend. It can produce native executables, typed Quantum IR,
OpenQASM 3 circuits, and resource reports. The current version is **1.0.0-rc.3**, with **GCC
11.4.0** as the reference configuration. This is an experimental candidate; the stable 1.0
requirements remain open. The [release guide in Spanish](docs/releases/v1.0.0-rc.3.es.md) lists what
works, what is incomplete, the finite coherence correction and commands to run the delivery.

The current objective is a **fully usable everyday C core**: common types, functions, control,
arrays, pointers, vectors and the library operations those programs need. The
[agreed scope and acceptance criteria](docs/perfil-c-cotidiano.es.md) distinguish implemented
capabilities from pending work. The full [C11 inventory](docs/c11-inventory.md) and
[extended roadmap](docs/c11-roadmap.md) remain technical references; completing every C11 interface
is outside the current completion gate. Ordinary libc support is not evidence that libc functions
operate on unmeasured quantum data.

A [general status overview in Spanish](docs/estado-general-del-proyecto.es.md) explains what works,
what remains partial or pending, and what the available tests demonstrate.

The development block after RC3 corrects untouched-byte coherence in fixed-word `strcpy`, `strncpy`,
`strcat` and `strncat`, using quantum write masks when the terminator is quantum. The
[string-write note](docs/paper/string_writes_note.es.md) explains retained history, cleanup, finite
validation and resource exclusions. This does not qualify every string writer or the full everyday C
profile. The [selected-effects follow-up](docs/paper/lazy_values_note.es.md) corrects the old
history references and pending old operands across conditional updates. The subsequent
[priority C campaign](docs/audit-evidence/c-priority-integration-v1/README.es.md) passes all 2332
history checks in normal and ASan/UBSan configurations. Its 17 normal C validators pass; ten
complete their sanitized run, while seven remain incomplete or pending in that configuration. Its
normal export pass independently validates 64 states and six channels with QuEST. The full
verification gate remains open. A [Windows/WSL demo](output/demo/README-lazy.es.md) runs the known
basis regression and a coherent example without rebuilding.

**Partial fixed-word writes now preserve untouched bytes in the published counterexample.** The
[new evidence](docs/audit-evidence/fixed-patches-v1/README.es.md) records 1920 semantic public
density comparisons, 1920 independently evolved joint states and 912 complete inverses, with normal
O0/O2 and ASan/UBSan O1. It includes signed/unsigned 16/32-bit roots and reversible producers, CPU
lengths, open/nested controls, alias fills, overlapping moves, heap sources and scoped phases. The
[fixed-buffer example](examples/blocks/05/fixed_coherence.c) and
[technical note](docs/paper/fixed_patches_note.es.md) explain the correction. General lifetime,
alias/ABI, all string writers, large 64-bit profiles and current full CI/papers remain unqualified.

The [heap coherence evidence](docs/audit-evidence/heap-coherence-v1/README.es.md) corrects the
previous inactive-sector heap-copy counterexample and protects untouched heap bytes. It records 2670
public density comparisons, 2670 independently evolved joint gate states and 852 complete inverses,
with separate scopes, normal O0/O2 and ASan/UBSan O1. It includes overlapping moves, quantum
selected writes, negative words and scoped borrows. General lifetimes, repeated writes, alias/ABI
combinations and 64-bit resource-heavy profiles remain unqualified. The
[new example](examples/blocks/05/heap_coherence.c) and
[technical note](docs/paper/heap_coherence_note.es.md) document this finite correction.

Start with [usable delivery 01](docs/entregas-incrementales.es.md): after building, run
`bash scripts/run-block.sh 01` to compile eight small examples and produce native executables,
OpenQASM circuits and resource reports. Deliveries track a defined incremental plan, not a claimed
percentage of complete C11 support.

The [controlled-operand contract](docs/c11-controlled-operands.md) covers supported pure integer
operands in coherent assignments, XOR, addition, standard integer absolutes and byte fill. Its
[audit report](docs/re-audit-2026-10-18-controlled-operands.md) records complete normal/sanitizer
regression and separate installation checks for its historical 138 inputs. The
[previous 144-input integration](docs/audit-evidence/strings-integration-v6/README.es.md) adds
guarded CPU loads, deferred supported `phase` angles, and partial
[`strlen`/`strcmp`](docs/c11-strings.md) adapters. Its targeted normal/sanitizer checks and
independent string states passed. Its
[remote CI](docs/audit-evidence/integrated144-remote-ci/README.es.md) also passed all 28
normal/sanitizer suites on the same compiler inputs. The larger local campaign with 665 fuzz
compilations per configuration also
[completed in two segments](docs/audit-evidence/common-remainder-v2/README.es.md): 80 closed
commands, with the interrupted prefix and continuation preserved. That historical result does not
certify later compiler inputs. Complete quantum C11 remains open.

The [previous 146-input integration](docs/audit-evidence/known-fill-cleanup-v2/README.es.md) fixes
automatic cleanup of unobserved known-basis `memset` temporaries. Its targeted normal/sanitizer
suites passed, including 940 fill checks per configuration and 3528 separate complete-state checks
of compiled scope cleanup. A fresh Linux root build passed 20 exact contract executions. The full
regression and remote CI for these new inputs remain pending; the earlier CI verifies 144 inputs.

The [previous 148-input integration](docs/audit-evidence/common-remainder-v2/README.es.md) reduces
workspace for integer `%` with constant divisor magnitude 2 through 16. Its fresh normal/sanitizer
regressions pass 907 expression, 990 division, 393 native and 271 in-place checks each. Separate
compiled-circuit proofs check 1968 complete states, with source identities and validation limits
recorded explicitly. Full regression and remote CI for these inputs remain pending. General 64-bit
conditional preparation, wide results and the remaining C11 features still need work.

The [previous 151-input integration](docs/audit-evidence/selection-views-v5/README.es.md) reduces
private workspace for [integer conditional expressions](docs/c11-selections.md). Direct selection
between signed 64-bit limits now fits. Separate compiled-program proofs pass 768 complete states
with phases and compiler scope cleanup. Normal/sanitizer targeted runs pass 1195 expression, 55
shift, 990 division, 393 native and 271 in-place checks each; tool reuse and earlier test failures
are identified explicitly. A separate fresh broad regression is in progress. General wide
expressions and complete C11 remain open.

The current
[small-constant quotient implementation](docs/c11-division.md#quotient-by-a-small-constant) adds
reversible `/` for divisor magnitude 2 through 16 and direct Boolean consumers. Its maintained
finite regressions check 2310 additional domains; the runnable
[quantum buckets example](examples/quantum_buckets.c) computes correlated groups from a byte
superposition before explicit measurement. General division and complete C11 remain partial.

The native runtime now offers [sparse simulation](docs/simulacion-dispersa.es.md) for circuits with
up to 128 physical wires and bounded occupied support. `bash scripts/run-block.sh 02` runs an
`int32` quotient and a `size_t64` example that exceed the default dense limit. This is a usable
advance within block 02; the everyday integer profile remains incomplete.

The [variable-shift implementation](docs/c11-shifts.md) now selects source positions without
retaining one full temporary word per count bit. Full 32-bit shifts that previously exceeded the
wire budget now generate circuits; the block 02
[variable_shift program](examples/blocks/02/variable_shift.c) executes in sparse mode with 105
wires.

The [Boolean shift consumers](docs/c11-shifts.md#boolean-consumers-and-integer-conversions) now
avoid a full shifted word when only its truth is needed, and integer conversions restrict private
output width. The block 02 [shift_predicate example](examples/blocks/02/shift_predicate.c) executes
64-bit shift/byte predicates with 85 wires. A numeric logical-negation bug in the arithmetic
fallback is also corrected; its previous wrong joint state is preserved as regression evidence.

## Install and build

Use Linux or WSL. On Windows, open a WSL terminal and work inside the repository there. The
reference Linux environment is Ubuntu 22.04.

Start in a local checkout, then install the build dependencies. The historical source URL
`https://github.com/igna-s/cket` returned HTTP 404 without authentication on October 14, 2026;
public download availability could not be verified. For a new checkout, obtain an accessible source
URL from the maintainer.

```bash
sudo apt-get update
sudo apt-get install -y gcc-11 g++-11 gcc-11-plugin-dev binutils
export CC=gcc-11
export CXX=g++-11
bash scripts/build.sh
build/cket --version
```

Run these commands in the repository root. Keep `CC` and `CXX` set in the terminal where you compile
or test.

The driver, runtime, backend, engine, and test programs are **C**. The GCC plugin is **C++17**
because GCC's internal API requires C++; shell scripts coordinate commands. The build, runtime and
compiler/runtime test suite use C/C++ and Bash. Optional formatting, inventory, paper and
supplementary evidence audits use Python. No QPU or external quantum SDK is needed to run a compiled
program.

GCC plugin headers must match the compiler. For a custom installation, set `GCC_PLUGIN_INCLUDE` to
the directory containing `gcc-plugin.h`. The build also recognizes headers extracted under
`.deps/gcc/`. The driver checks that the GCC version used to compile a program matches the plugin
build.

## Your first program

[examples/hello.c](examples/hello.c) prepares a basis value, adds five reversibly, and measures it:

```c
#include <quantum.h>
#include <stdio.h>

int main(void) {
    quant uint8_t value = 7;
    value += 5;
    printf("%u\n", (unsigned)measure(value));
    return 0;
}
```

Compile and run it:

```bash
build/cket examples/hello.c --run -o build/hello
# Output: 12
```

To compile first and execute later:

```bash
build/cket examples/hello.c -O2 -o build/hello
./build/hello
```

The output is a native Linux executable with its C runtime linked in. Running it does not invoke
GCC, parse the source again, or require a Python process. Keep the compiler's build files together
when using `cket`; the driver locates its plugin, header, runtime, and library relative to its own
executable.

The root `cket` file is a shell launcher. `bash cket examples/hello.c --run -o build/hello` is
equivalent to calling `build/cket`.

## Prepare, measure, and reset

Use small register widths when simulating:

```c
quant uint8_t basis = 7;                // A basis state.
quant uint8_t x = superposition(0, 15); // Uniform over 0 through 15.
uint8_t result = measure(x);            // An explicit classical result.
reset(basis);                           // Destroy information and leave zero.
```

An uninitialized quantum local starts at zero. Uniform preparation accepts constant intervals
`[0, 2^k - 1]`; arbitrary or CPU-computed interval bounds are rejected. A basis initializer may use
a CPU value evaluated once.

`measure(x)` collapses the register and returns its underlying C integer type. It preserves
correlations and leaves the measured state available for supported later operations. It does not
clear a measured one. `reset(x)` explicitly destroys information and leaves zeroed storage.

Every owner must eventually be measured, reset, transferred, or safely uncomputed. The compiler
never inserts hidden measurements to make coherent code work.

[examples/prepare_measure.c](examples/prepare_measure.c) mixes ordinary CPU values and a quantum
measurement:

```bash
build/cket examples/prepare_measure.c --run --seed 7 -o build/prepare
# Prints classical=10 quantum=<a value from 0 through 255>
```

Measurements are probabilistic. To reproduce a run, use `--seed N` with `--run`, or set
`CKET_SEED=N` when executing the binary directly:

```bash
CKET_SEED=7 ./build/prepare
```

A fixed seed is a reproducibility aid, not a success guarantee for a quantum algorithm.

## Expressions and ownership

Arithmetic expressions support `+ - * / %`, comparisons, bit operators, constant and variable shifts
with proven definedness, and pure Boolean/conditional expressions. Variable shifts have a table-free
barrel network with explicit workspace limits; see [contracts and evidence](docs/c11-shifts.md).
Pure negation, complement and logical/conditional expressions also have
[table-free circuits and branch-local definedness proofs](docs/c11-expressions.md). GCC's promotions
and explicit casts remain significant: with `uint8_t x`, `x * x` is promoted to `int`; it is not
truncated to eight bits before a comparison.

A computed result preserves its inputs and may become entangled with them. For example,
`quant uint8_t parity = x % 2;` computes a reversible result, while `quant uint8_t next = x;` with
the same compatible type **transfers ownership**: `x` cannot then be used. This is not an
independent copy.

Unsigned `+=`, `-=`, and supported XOR updates are reversible. A distinct quantum source may be
preserved during addition/subtraction. Scalar overwrites can use
[versioned storage and retained history](docs/c11-history.md): old dependent values keep their
original inputs, and the new owner receives freshly computed storage. Unknown old states remain part
of the joint circuit output unless a valid cleanup is available.
[Structured coherent branches](docs/c11-control.md) retain their branch decisions. Direct fixed
overwrites use a controlled exchange to preserve an inactive destination; general borrow cleanup and
coherent heap copies still have the [open findings](docs/re-audit-2026-10-24-coherence.md).
[Classical scalar branch joins](docs/c11-cpu-control.md) transfer the selected owner's physical
storage without adding gates or copying its state, including nested branches and saved inverses.

In-place bit permutations and affine word updates also have
[table-free certificates](docs/c11-inplace.md). These include fixed rotations, complement, defined
negation and multiplication by an odd constant, with no private qubits. Boolean updates retain C
truth conversion. `--no-tables` also applies to the remaining in-place fallback. General CFG joins,
general memory/exit lifetimes and remaining expression effect contexts remain pending.

[Ordered scalar expression effects](docs/c11-effect-values.md) now preserve postfix old values,
return prefix/assignment new values and sequence comma operands with retained history. Mixed CPU
effects and general memory/float measurement operands remain pending. History has an explicit qubit
cost; no measurement or reset is inserted to erase it.

[Selected scalar effects](docs/c11-lazy-values.md) extend conditional expressions and logical short
circuits. A saved decision chooses the effects and value even when an arm overwrites its condition;
checked inverses clean decisions and intermediate history.

`memset` has a [checked quantum byte-fill adapter](docs/c11-memset.md) for fixed integer storage,
with retained overwrite history and once-only value evaluation. Discarded memory calls can compose
in [comma and lazy expressions](docs/c11-memory-sequences.md).
[Local checked destination returns](docs/c11-memory-returns.md), constant pointer distances and pure
CPU predicates inside coherent branches are supported. General returned-pointer consumers, runtime
counts, dynamic allocation and general layouts remain pending.

`memcpy` and `memmove` have [checked quantum memory adapters](docs/c11-memcopy.md) for whole
integers and a 64-byte object-byte path. Known-basis destinations retain their fast copy gates;
unknown overwrites retain previous owners. Partial words, different integer sizes and known CPU
source objects are supported. Read-before-write ordering preserves supported overlap, and checked
return borrows preserve destination identity. General layouts, dynamic extents and unrestricted
returned-pointer consumers remain pending. Complete qualification of the new extension is deferred.

`strncmp` has a [bounded coherent character adapter](docs/c11-strncmp.md): it stops at a first
difference or an equal NUL while preserving input bits and relative phases. Constant checked ranges,
integer object bytes, immutable constants and
[current CPU byte snapshots](docs/c11-byte-snapshots.md) are supported. Known CPU storage is
captured once, with disabled suffix loads after its own NUL. Dynamic counts/addresses and general
quantum layouts remain pending.

`memcmp` has a [coherent byte-comparison adapter](docs/c11-memcmp.md) for checked integer storage,
constant byte views, immutable constants and current bytes of known CPU objects. It preserves
inputs, relative phases and saved inverses, with no implicit measurement/reset. Dynamic ranges,
volatile/atomic CPU accesses, general quantum layouts and indirect quantum calls remain pending.

The new [string span and search adapters](docs/c11-string-searches.md) add `strspn`/`strcspn` length
expressions, direct null/address predicates and same-object differences for `memchr`, `strchr`,
`strrchr`, `strpbrk` and `strstr`. Local immutable search pointers support aliases, character
dereference and constant indexing. Run `CC=gcc-11 bash scripts/run-block.sh 06` for the ten included
examples. The latest additions await runtime qualification. General pointer writes/ABI and full
state/inverse, sanitizer and GCC-context qualification remain pending.

Checked [`strcpy` and `strncpy`](docs/c11-string-copy.md) now copy strings into quantum integer
storage using retained overwrite history. The block 06 example covers a superposed source, unchanged
tail bytes, zero padding and a partial integer write from a CPU source. Dynamic counts, general
return consumers and full qualification remain pending.

Checked concatenation with [`strcat` and `strncat`](docs/c11-string-concat.md) snapshots the
original lengths before retained-history updates. The block 06 example covers a superposed
destination length and bounded append from a nonterminated source. Full coherent/inverse and
compiler-context qualification remains deferred.

The controlled concatenation example adds strcat/strncat inside a quantum Boolean branch and a
phase/Fourier sequence with expected output 1. Compiler-private zero captures are allocated
independently of the branch; general user allocation and dynamic-loop lifetimes retain their
restrictions. Complete state/inverse qualification remains deferred.

Implementation now also includes checked [`atoi`/`atol`/`atoll`](docs/c11-decimal.md) and saved
search-pointer character stores and value-producing increments. The
[ownership campaign](docs/audit-evidence/ownership-contracts-v1/README.es.md) adds finite native,
full-state, phase and inverse evidence; general strings, domains and consuming contexts remain open.

Fixed scalar arrays now have partial [quantum-index reads and writes](docs/c11-coherent-index.md),
including assignment and increment values. Bounds are checked before narrowing the selector; writes
retain previous owners. The source example executes and finite coherent states/inverses are checked
in the ownership campaign; this does not close block 04.

Checked [buffer functions](docs/c11-buffer-functions.md) now compose those reads and retained writes
through pointer parameters and constant-offset aliases. The compiler checks the element index before
canonical pointer-byte scaling can wrap. Block 03 also adds CPU-indexed reads, stable-owner runtime
scans with modular sum/parity, and scalar results returned by functions. The four examples execute;
finite state/inverse evidence remains distinct from complete integration.

Runtime-sized scalar quantum heaps now support native `malloc`, `calloc`, `realloc` and `free` with
classical capacity/address metadata and retained unknown outputs. See the
[heap contract](docs/c11-heap.md) and run `bash scripts/run-block.sh 05` for a vector that actually
grows. The runner also includes `coherent_heap`, with a quantum-indexed postfix increment and its
previous value. Scheduled consumers accept assignment and prefix/postfix values, including runtime
CPU iterations; saved recipes preserve their phases and lifetimes. The
[directed heap campaign](docs/audit-evidence/heap-effects-v1/README.es.md) checks finite joint
states, phases and saved inverses. General heap consumers and static export remain pending.

Block 05 also includes `heap_bytes`: native heap-to-heap `memcpy`/`memmove` and heap `memset`, with
runtime CPU byte lengths, partial/mixed integer words, overlapping moves and captured quantum fill.
Returned destinations retain checked classical address aliases. The
[heap byte-memory evidence](docs/audit-evidence/heap-memory-v1/README.es.md) records finite complex
states, independent gate inverses and actual ASan/UBSan builds. The runner now also includes
`heap_transfer`, which copies between fixed quantum buffers and heaps in both directions. Known
nonvolatile CPU objects can supply captured source bytes to heaps. Fixed addresses are checked
statically and lengths may be CPU runtime values. `heap_iterations` combines postfix accumulation
and heap-to-fixed copies inside a CPU loop with a runtime iteration count, including zero
iterations. Its [history contract](docs/c11-runtime-history.md) retains actual previous owners and
inverses. General pointer/ABI contexts and string adapters on heaps remain pending.

`fixed_buffers` adds a fixed array passed to a function with overlapping `memmove` and `memset`
inside a CPU loop. Its self-dependent fill is captured once before any destination word changes. The
[fixed-buffer loop evidence](docs/audit-evidence/fixed-memory-loops-v1/README.es.md) separates
complete complex states, gate inversion, native contexts and sanitizer checks. The new
[runtime-length route](docs/c11-runtime-memory.md) accepts pure CPU counts over checked fixed views.
`buffer_lengths` passes its `argv` length to a function and combines copying, overlap and filling.
Addresses remain checked constants. Returned data reads require explicit quantum observation; CPU
placeholder reads and unchecked address escapes are rejected.

The [ownership and copy contracts](docs/no-clonacion.es.md) distinguish moves, borrows, correlated
basis-value copies and retained overwrites. Validation found and corrected a narrowed-addition bug
that hid signed overflow. Unknown quantum states are not independently cloned.

Temporary expressions retain the concrete input versions needed for their inverse. Checked
overwrites preserve those versions; resizing or freeing a heap retires its public access path while
saved borrows remain valid. Measurements/resets that invalidate a pending inverse are diagnosed.
Supported derived predicates can be uncomputed after their last use; the current audit still
reproduces a scoped-borrow cleanup failure after a controlled overwrite.

`div`, `ldiv`, `lldiv` and `imaxdiv` have [partial quantum product adapters](docs/c11-division.md)
for pure quantum arguments, captured CPU arguments and fresh quotient/remainder results. Constant
power-of-two division uses table-free bit views and signed rounding corrections. General quantum
product ABI and wider resource budgets remain open.

The [immutable character-profile contract](docs/c11-ctype.md) adds partial adapters for all fourteen
`ctype.h` interfaces. Its 1815 checks cover three byte locales, exact masks, phases, saved cleanup,
functions/memory/control combinations, thread locales and immutable circuit modules. Static exports
require an explicit locale or frozen profile artifact. Wide characters, indirect calls, automatic
locale traces and concurrency remain open in the extended C11/library roadmap.

[CPU integer bindings and classical product preparation](docs/c11-bindings.md) capture values once
and retain them for cleanup. Changing a CPU variable afterward does not change a saved inverse.
Unbound templates receive structural validation; numerical checks and static exports require
concrete values. Classical record-returning calls initialize nested quantum fields without dropping
their side effects.

Division requires a proven nonzero divisor. Signed arithmetic must have supported range/overflow
proofs; division truncates toward zero and remainder follows the dividend's sign. Accepted type
widths do not imply every expression can be synthesized or simulated. See the
[language contract](docs/language.md) for precise restrictions.

## Coherent conditions and entanglement

An `if` whose condition is quantum controls operations coherently; it does not measure to select a
branch. [examples/entanglement.c](examples/entanglement.c) creates correlated bits:

```c
#include <quantum.h>
#include <stdio.h>

int main(void) {
    quant bool a = 0;
    quant bool b = 0;
    fourier(a);
    if (a)
        b ^= 1;
    unsigned first = measure(a);
    unsigned second = measure(b);
    printf("%u %u\n", first, second);
    return first != second;
}
```

```bash
build/cket examples/entanglement.c --run --seed 7 -o build/bell
```

It prints `0 0` or `1 1`. Fourier on one bit prepares its equal superposition, and the controlled
XOR entangles the two registers.

Coherent branches accept supported quantum operations and phases. Printing, CPU writes, measurement,
reset, and classical jumps inside a quantum condition are rejected. Measure first if you need a CPU
decision. The condition must also be safely uncomputed: changing a register it depends on can
prevent cleanup.

CPU `for`, `while`, and `do` loops support operations on stable existing owners, including coherent
conditions and measurement followed by classical decisions. A loop does not prove it ran even once;
lifetime obligations must hold on every path. General coherent loops, dynamic owner creation, and
unsafe early returns remain unsupported.

```bash
build/cket examples/hybrid_loop.c --run -o build/loop
# Output: 29
```

Dynamic CPU loops cannot be flattened into a static QASM circuit.

## Search for an answer

[examples/find.c](examples/find.c) searches a 16-element uniform domain:

```c
#include <quantum.h>
#include <stdio.h>

int main(void) {
    quant uint8_t x = superposition(0, 15);
    quant uint8_t answer = find(x, x * x == 9);
    printf("%u\n", (unsigned)measure(answer));
    return 0;
}
```

```bash
build/cket examples/find.c --run --seed 7 -o build/find
# Output with this seed: 3
```

`find(x, predicate)` promises **exactly one solution**. `find_known(x, predicate, M)` accepts
another known count, with `1 <= M <= N`. The caller supplies the count; the compiler does not
enumerate the domain to verify it. Both consume the original owner and transfer the amplified
register to the result. Success remains probabilistic; a wrong promise invalidates the guarantee.

For an **unknown count**, including zero, use [examples/search.c](examples/search.c):

```c
#include <quantum.h>
#include <stdio.h>

int main(void) {
    quant uint8_t x = superposition(0, 15);
    bool found = search(x, x * x == 9, 80);
    if (found)
        printf("%u\n", (unsigned)measure(x));
    else {
        reset(x);
        puts("Search budget exhausted");
    }
    return 0;
}
```

```bash
build/cket examples/search.c --run --seed 7 -o build/search
# Output with this seed: 3
build/cket examples/search_empty.c --run --seed 7 -o build/search-empty
# Output: Search budget exhausted
```

`search` measures and verifies candidates, then retries within a constant budget of 1 to 4096
attempts. `true` identifies a valid candidate; `false` means **budget exhausted**, not proof of
nonexistence. With a solution and independent uniform randomness, failure is bounded by
`(7/8)^attempts`; the PRNG seed is not a per-run guarantee. This strategy needs CPU feedback and
cannot export as flat QASM.

All search forms require an intact uniform input and a pure predicate. Oracle synthesis, simulator
size, and complexity limits apply independently.

## Functions, arrays, and circuit modules

Quantum functions currently compose within one C translation unit. Put definitions before their
calls. Read-only parameters borrow inputs; mutable parameters may update an existing register but
cannot overlap or consume borrowed owners.

[examples/functions.c](examples/functions.c) defines a reusable predicate and update:

```c
quant bool square_is_nine(quant const uint8_t value) {
    return value * value == 9;
}

void increase(quant uint8_t value, unsigned amount) {
    value += amount;
}
```

```bash
build/cket examples/functions.c --run --seed 7 -o build/functions
# Output with this seed: 4
```

Pure conditional results and some terminal owned scalar returns are supported. Recursion, indirect
quantum calls, arbitrary quantum linking across C files, and general product returns are pending.

Fixed arrays and integer structures are supported; classical pointers borrow existing quantum
storage. [examples/products.c](examples/products.c) uses both:

```bash
build/cket examples/products.c --run -o build/products
# Output:
# 9 2
# 3 7
build/cket examples/vector_function.c --run -o build/vector-function
# Output: 10 1
```

For a CPU-selected array element, use [examples/dynamic_vector.c](examples/dynamic_vector.c):

```bash
build/cket examples/dynamic_vector.c -o build/vector
printf '0\n' | ./build/vector
# Output: 3
printf '1\n' | ./build/vector
# Output: 9
```

An index outside the two-element array, malformed input, or EOF returns exit status 1. The program
explicitly resets both owners; measuring one dynamically chosen element cannot discharge every array
lifetime.

Independent unitary QIR modules are embedded at compile time. Their paths resolve relative to the C
source file, so the example can load [increment.qir.json](examples/increment.qir.json):

```bash
build/cket examples/circuit_module.c --run -o build/module
# Output: 7
```

`circuit("module.qir.json", value)` borrows a register; `inverse_circuit` applies its inverse.
Supported quantum conditions also provide controlled versions. See the
[circuit ABI](docs/circuit-abi.md) for module validation and aliasing requirements.

## Phases and Fourier transforms

`phase(theta)` applies a finite classical angle in radians to the active controls. Without controls
it is a global phase. `phase_flip()` applies pi. Relative phases affect interference even when basis
probabilities initially look unchanged.

`fourier(x)` implements the positive quantum DFT on the entire little-endian register.
`inverse_fourier(x)` reverses it. Both support coherent controls and preserve correlations.

```bash
build/cket examples/fourier.c --run -o build/fourier
# Output: 7, after Fourier and its inverse
build/cket examples/phase_estimation.c --run --seed 7 -o build/phase-estimation
# Output: 3
```

The phase-estimation example uses a known eigenphase of `3/256`, controlled phases, and inverse
Fourier. CPU-computed angles work in native execution but require static values for QASM export.

## Export and inspect a circuit

Each command selects one output action:

`build/cket --help` lists driver options. Use `--gcc gcc-11` to select a matching compiler
explicitly. Additional GCC options must be single tokens, such as `-O2`, `-I/path/to/headers`, or
`-DNAME=1`; pass one C source file per invocation.

```bash
build/cket examples/find.c --emit-ast -o build/find.ast
build/cket examples/find.c --emit-ir -o build/find.qir.json
build/cket examples/find.c --emit-qasm -o build/find.qasm
build/cket examples/find.c --resources -o build/find.resources.txt
build/cket_backend --validate build/find.qir.json
build/cket_backend --resources build/find.qir.json --no-tables
```

| Output                            | Use                                                                       |
| --------------------------------- | ------------------------------------------------------------------------- |
| `--emit-ast`                      | Inspect the GCC tree used by the frontend.                                |
| `--emit-ir` / `--emit-quantum-ir` | Inspect typed `cket.qir` version 1, ownership operations and CPU markers. |
| `--emit-qasm`                     | Export the supported static quantum circuit as OpenQASM 3.                |
| `--resources`                     | Report logical/physical qubits, ancillas, gate counts and depth.          |

The custom IR is **not LLVM QIR**; `--emit-llvm` is not implemented. QASM contains the quantum
circuit, not ordinary C I/O. Runtime input, dynamic selection/control, unknown-count search, and
dynamic angles need native execution.

The backend also accepts `--emit-qasm` and `--state`. State inspection is simulator debugging before
document measurements; it does not change program semantics. `--no-tables` requires synthesis
without the finite enumeration fallback.

Resource counts precede device decomposition. Larger controlled operations count as MCX; exported
open controls add X gates, so logical and exported gate totals may differ.

## Example reference

The top-level collection now contains 37 C examples, including loops, sums, Fibonacci, dot products,
extrema, interactive input and measurement feedback. See the [examples guide](examples/README.md)
for the complete catalog, expected results and input formats. Run all 35 simulator-compatible
examples with:

```bash
bash scripts/run-examples.sh --all
```

All C examples are checked at `-O0`, `-O1`, `-O2`, `-O3`, and `-Os`. Seed-dependent entries below
refer to seed 7.

| File in `examples/`                               | Behavior                                                                        | Static QASM |
| ------------------------------------------------- | ------------------------------------------------------------------------------- | ----------- |
| [hello.c](examples/hello.c)                       | Basis arithmetic; prints 12.                                                    | Yes         |
| [entanglement.c](examples/entanglement.c)         | Correlated bits, 0 0 or 1 1.                                                    | Yes         |
| [prepare_measure.c](examples/prepare_measure.c)   | CPU value 10 and quantum result in 0..255.                                      | Yes         |
| [find.c](examples/find.c)                         | Known single-solution Grover search; prints 3.                                  | Yes         |
| [search.c](examples/search.c)                     | Verified unknown-count search; prints 3.                                        | No          |
| [search_empty.c](examples/search_empty.c)         | Empty domain result; reports budget exhaustion.                                 | No          |
| [coherent.c](examples/coherent.c)                 | Coherent square predicate, phase and addition; result in 5..20.                 | Yes         |
| [functions.c](examples/functions.c)               | Borrowed predicate/update composition; prints 4.                                | Yes         |
| [fourier.c](examples/fourier.c)                   | Fourier followed by inverse; prints 7.                                          | Yes         |
| [phase_estimation.c](examples/phase_estimation.c) | Controlled phases and inverse QFT; prints 3.                                    | Yes         |
| [products.c](examples/products.c)                 | Array and structure updates; prints 9 2, then 3 7.                              | Yes         |
| [vector_function.c](examples/vector_function.c)   | Array borrow and fixed loop; prints 10 1.                                       | Yes         |
| [dynamic_vector.c](examples/dynamic_vector.c)     | Reads CPU index; prints 3 or 9.                                                 | No          |
| [hybrid_loop.c](examples/hybrid_loop.c)           | CPU loop and quantum updates; prints 29.                                        | No          |
| [circuit_module.c](examples/circuit_module.c)     | Controlled module and inverse; prints 7.                                        | Yes         |
| [types.c](examples/types.c)                       | 32-bit `quant int` type example; export succeeds, simulation exceeds its limit. | Yes         |

## Current limits and troubleshooting

| Limit                     | Current contract                                                    |
| ------------------------- | ------------------------------------------------------------------- |
| Quantum integer width     | Up to 64 bits.                                                      |
| Gate storage              | 128 physical qubits.                                                |
| Dense simulation          | 24 physical qubits; default runtime mode.                           |
| Sparse simulation         | Up to 128 physical wires and a configured occupied-support budget.  |
| Finite synthesis fallback | At most 12 relevant input bits; disable with backend `--no-tables`. |
| Symbolic synthesis        | 256 terms per bit and two million work steps.                       |
| IR expressions            | 100,000 tokens, 128 levels; input files up to 64 MiB.               |
| Fixed loop expansion      | At most 4096 checked iterations.                                    |

The 24-qubit vector alone uses 256 MiB. Full declared widths and temporary ancillas count even when
preparation uses a smaller support. Released clean wires can be reused, but vector dimension does
not shrink. A 32-bit `quant int` can therefore export while failing in dense mode. Opt-in sparse
execution supports wider low-support states, with 65536 occupied labels by default and an explicit
diagnostic on support exhaustion. See the [sparse guide](docs/simulacion-dispersa.es.md).

Register synthesis covers addition, subtraction, multiplication, division, remainder, and
comparisons beyond 12 relevant bits. Growth tests include two 16-bit inputs, or 32 relevant bits.
Complexity budgets can still reject large compositions; 64-bit division needs unsupported
extra-width workspace.

If a command fails:

- **Missing plugin headers or GCC version mismatch:** install matching `gcc-plugin-dev`, export
  matching `CC/CXX`, and rebuild.
- **Quantum value used as classical data:** call `measure` explicitly; keep I/O outside quantum
  conditions.
- **Consumed owner or pending dependency:** end derived lifetimes before modifying their inputs, and
  do not reuse a name after a move or `find`.
- **Simulator qubit limit:** choose smaller widths or export the circuit without executing it.
- **Dynamic operation cannot export:** run natively; static QASM cannot encode arbitrary CPU
  feedback.
- **Synthesis budget or unsupported arithmetic:** inspect typed IR and ranges; a wider type alone
  does not establish safe or scalable synthesis.

General quantum floating point, VLAs, unions, bitfields, dynamic quantum allocation, QPU execution,
and full C11 quantum coverage are outside the current subset. GCC integration uses a provisional
internal `volatile` marker and differential testing, rather than a proof of every frontend
transformation.

## Verify the installation

The native tests and every example require only the build dependencies:

```bash
bash scripts/test.sh
```

For the complete reproducible check, including independent QASM validation and sanitizers:

```bash
sudo apt-get install -y curl ca-certificates locales
bash scripts/verify.sh
```

This rebuilds the compiler, runs the native suite and all examples, checks frontend optimization
differentials, measures synthesis growth with tables disabled, validates exported circuits
independently, and repeats checks with ASan/UBSan. Logs remain in `build/verify-*.log`. To enlarge
the frontend campaign, use `CKET_FUZZ_CASES=128 CKET_FUZZ_SEED=20261012 bash scripts/verify.sh`.

Character-profile tests use `localedef` and glibc locale sources to create an isolated Latin-1
fixture inside the validation workspace, with a private `LOCPATH`.

Independent validation builds the serial **C** backend of
[QuEST 3.7.0](https://github.com/QuEST-Kit/QuEST/releases/tag/v3.7.0), pinned to commit
`d4f75f724993b4af8e43a796e3c09ce24ae11670`, with SHA-256
`1a5939798a70b749dfa57cfaa116ece03a2d36aa59c1b712cb3dc2103ab1cded`. Downloaded sources and their
license stay under `build/validation-deps/`; the compiler/runtime never link QuEST.

The controlled-operand suite adds 1270 checks per configuration: 1000 defined native executions, 200
undefined-domain diagnostics, 60 CPU-effect diagnostics and 10 private-homonym executions. Its 100
ordinary C references per configuration and independent phase/inverse proofs have separate scopes.
Diagnostic rejection does not implement the rejected C context.

Validation includes 393 native checks; 36 examples at five optimization levels; a default frontend
corpus of 29 programs at five levels; 24 growth circuits; and 70 independent QASM cases exported
without tables. QASM cases check 64 complete states/final probabilities and six
intermediate-measurement/reset channels with 8192 transcripts each, including integer absolute
value, variable shifts and combined function/array/control programs. The separate shift suite checks
55 cases across complete small domains, invalid arithmetic, resources and C composition. The pure
expression suite has 1195 checks: the original 97, 810 finite remainder checks and 288 finite
selection domains. Complete-state comparisons, finite basis actions and wide exports retain their
separate scopes. The in-place suite adds 271 checks of affine maps, bit permutations, Boolean
normalization, controls, inverse, policy and C composition. The memory-copy suite adds 414 checks of
known-basis extensions, complete states/inverse, overlap, contracts, C composition and wide
generation. The division suite adds 990 checks of products/projections, signed rounding, Boolean
conversion, coherent states/inverse, GCC/libc preservation, definedness and resource contracts. The
binding suite adds 519 checks of complete states and saved inverse, typed integer endpoints,
malformed inputs, CPU/classical-product evaluation and C document specialization. The history suite
adds 2332 checks of retained states, inverse lifetimes, combined assignments, C composition, CPU
snapshot cleanup, coherent and CPU scalar joins, old/new expression results, sequencing,
Boolean/word states, deferred ownership cleanup and explicit pending-contract diagnostics. The
native suite also exhausts selected eight-bit expressions, 3720 adder basis pairs and 65,536-input
predicates, checks 126 Fourier bases against the DFT, and exercises 1024 generated typed expressions
plus 4096 IR mutations.

The shared [validation runner](docs/audit-validation.md) has 18 fault-injection checks. Signals,
timeouts, setup failures, captured sanitizer reports and compiler internal errors cannot satisfy an
expected semantic rejection. Negative memory cases require their specific diagnostic; the native
suite uses the same bounded runner. These checks strengthen finite evidence without certifying the
entire language or library.

`bash scripts/test-c11-classical.sh` additionally compares finite integration programs using the 29
classical C11 header families against ordinary GCC at five optimization levels. Its quantum code
uses an explicit measurement boundary; the test does not certify quantum adapters for the whole
library. The [interface index](docs/c11-library.json) and [roadmap](docs/c11-roadmap.md) record the
remaining work. The [repository and paper audit](docs/repository-audit.md) records reproduced bugs,
fixes, campaign scopes and the distinction between historical and current evidence.

`build/validate_library` additionally checks unsupported contexts across 210 libc interfaces: each
fixture must first compile as ordinary C, then receive a quantum semantic diagnostic at all five
optimization levels with builtins enabled and disabled (2100 checks). These rejections establish
boundaries, not implemented adapters. `build/validate_profiles` has 96 branch-proof regressions,
including distinct immutable character profiles, guarded division, complete phased states and
inverse cleanup. The frontend generator compares every generated expression with C over all 256 byte
inputs at each optimization level; basis-value coverage is separate from coherent amplitude
validation.

`build/validate_memset` adds 800 checked fill tests: dense states and inverses, complete labeled
paths, sequenced value effects, native programs, rejected boundaries and wide generation.
`build/validate_memory_sequences` adds 360 checks of discarded memory calls in comma and lazy
expressions. Both suites run in the normal and sanitizer pipelines. The
[memory follow-up](docs/re-audit-2026-10-17-memory-expressions.md) records the complete isolated
134-input campaign, completed in segments, and separate fresh main checks. These remain partial
memory contracts.

`build/validate_byte_snapshots` adds 650 checks of current CPU bytes, actual captures and saved
inverses, protected-page/NUL guards, native C, rejected boundaries and template/export contracts.
The [snapshot follow-up](docs/re-audit-2026-10-17-byte-snapshots.md) records the fresh complete
normal/ASan/UBSan campaign, exact source identity and separate main-tree smoke checks. Cybersecurity
operations remain classical and outside this quantum adaptation.

`build/validate_strings` adds 460 bounded `strlen`/`strcmp` checks: 150 native executions, 130
diagnostics, 60 wide exports and 120 complete phased states with saved inverses. A separate compiled
proof checks 640 complete complex states across normal and ASan/UBSan builds, including full 64-bit
`size_t` and 32-bit `int` results. See the [contract](docs/c11-strings.md) for bounds and remaining
string work. `build/validate_phase` adds 3380 checks per configuration for supported deferred
classical angles; quantum floating registers remain unimplemented.

`build/validate_strncmp` adds 246 bounded string checks: complete phased states/inverses, native C,
diagnostic contracts and separate wide generation. The
[string follow-up](docs/re-audit-2026-10-15-strings.md) distinguishes the source-identical isolated
integration from its main-tree checks. Full signed 32-bit comparison results also have separate
sparse-circuit and labeled-basis evidence beyond the 24-qubit dense simulation limit.

`build/validate_memcmp` checks byte comparisons with complete phased states and inverses, all 256
byte inputs in native executables, contract rejections and separate 16/32/64-bit no-table
generation. The [comparison contract](docs/c11-memcmp.md) defines object byte order, aliases, bounds
and the remaining memory/ABI work.

`build/validate_numeric_locale` adds 70 checks of JSON phase numbers, controlled/global QASM
decimals, phase interference/inverses and locale restoration after output errors. Global and
per-thread comma-decimal locales work without changing the caller's numeric conventions. These
checks were repeated with the native/example, character, profile and QuEST suites in normal and
ASan/UBSan builds. The [evidence guide](docs/audit-evidence/README.md) distinguishes the broad
snapshot, the targeted correction and the latest byte-comparison verification segments, preserving
the exact sources of each. The [follow-up report](docs/re-audit-2026-10-15-memory.md) records the
normal importer capacity failure, its correction and subsequent normal/ASan/UBSan evidence.

For an optional machine-readable inventory check, run `python3 scripts/inventory_c11.py`. Its
`--require-complete` mode deliberately fails while any interface remains pending or partial. The
current index contains 1224 interfaces: 762 pending, 94 partial and 368 metadata facilities. The
index covers the named C11 profile, including optional facilities; it is not an implementation or a
test result for each entry.

Independent state comparisons allow one common global phase and use amplitude tolerance `1e-10`.
These finite checks provide evidence for the accepted subset; they are not a formal proof or
exhaustive coverage of arbitrary C programs. The QASM reader supports this exporter's static subset.

See [CONTRIBUTING.md](CONTRIBUTING.md) for individual commands and sanitizer details. CI runs the
same suites and stores verification evidence.

## Repository layout

All production implementation now lives under `src/`:

```text
src/
    cket.c                    Compiler driver
    cket_plugin.cpp           GCC plugin
    quantum_frontend.inc      Function, product and loop expansion
    quantum_semantics.inc     Types, ownership, effects and coherent control
    cket_runtime.c            Runtime linked into generated executables
    cket_backend.c            Standalone IR validation/export tool
    engine/                   IR parser, verifier, synthesis and simulator
include/quantum.h             Public language header
examples/                     Runnable C programs and a source QIR module
tests/                        C suites, generators and independent QASM reader
scripts/                      Shell build and verification entry points
docs/                         Detailed language and implementation contracts
.github/                      CI and contribution templates
```

The former root `native/`, `runtime/`, and `backend/` directories are consolidated here. Internal
engine headers stay separate from public `include/quantum.h` because they describe different APIs.
`.inc` files split large translation units while sharing internal helpers; they are not separate
programs.

`build/` contains generated binaries, logs, circuits, and validation dependencies. `.deps/` contains
optional local GCC headers. Both are excluded from Git and are not additional source modules.
Examples teach usage; tests enforce behavior; documents specify contracts.

For implementation details, read the [language contract](docs/language.md),
[C11 coverage matrix](docs/c-coverage.md), [IR verification contract](docs/ir-verification.md),
[circuit ABI](docs/circuit-abi.md), and [1.0 plan](docs/v1-plan.md).
[Current release notes](docs/releases/v1.0.0-rc.3.es.md) describe the experimental candidate;
[0.4.0 notes](docs/releases/v0.4.0.md) preserve the historical snapshot.

The project uses the [MIT license](LICENSE). Contributions follow
[CONTRIBUTING.md](CONTRIBUTING.md); vulnerability reports follow [SECURITY.md](SECURITY.md).
Repository documentation is primarily in English.
