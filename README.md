# `|c>` (CKet): High-Level Quantum C11 Compiler for GCC

**A C11 compiler extension for GCC that compiles high-level quantum data, reversible expressions,
and hybrid CPU/quantum control into native executables and OpenQASM 3 circuits.**

[![Verification](https://github.com/igna-s/cket/actions/workflows/verify.yml/badge.svg)](https://github.com/igna-s/cket/actions/workflows/verify.yml)
[![Version](https://img.shields.io/badge/version-1.0.0--rc.3-orange)](docs/releases/v1.0.0-rc.3.es.md)
[![Language](https://img.shields.io/badge/language-C11%20%2F%20C%2B%2B17-blue)](docs/language.md)
[![GCC 11 Plugin](https://img.shields.io/badge/frontend-GCC%2011%20Plugin-informational)](src/cket_plugin.cpp)
[![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

______________________________________________________________________

## 📋 Project Overview

`|c>` (`cket`) adds a `quant` type qualifier directly to C11 via a native GCC 11 compiler plugin
([`src/cket_plugin.cpp`](src/cket_plugin.cpp)). You write ordinary C declarations, expressions,
IEEE-754 floating-point math, coherent `if`/`switch` branches, loops, functions, arrays, structures,
dynamically sized buffers, and Grover queries over superposed registers. The compiler lowers
post-frontend GCC GIMPLE into a typed quantum IR (`cket.qir`), enforces single-ownership and
no-cloning rules without inserting hidden measurements, synthesizes reversible gate networks with
automatic ancilla uncomputation, and links a self-contained C runtime supporting both dense (up to
24 physical qubits) and opt-in sparse (up to 1024 physical wires) statevector simulation.

> **Scope Note:** `|c>` is an experimental research compiler for an explicit C11 + everyday C
> profile (`A1`–`A12`), not a drop-in compiler for arbitrary full C11 libc or physical QPU
> execution. Classical control and I/O run on the host CPU while quantum regions execute on the
> linked C statevector engine or export to static OpenQASM 3. Physical QPU compilation, LLVM QIR
> (`--emit-llvm`), transcendental `<math.h>` (`sin`/`cos`/`exp`/`log`), `long double`/`_Complex`,
> unrestricted CFG/ABI/lifetimes, and a formal proof of all GCC optimization passes remain outside
> scope.

**Verification & Release Baseline:**

- **Published CI Archive (`5c5a965`, run `42507019584`):** archived in
  [`docs/audit-evidence/full-ci-everyday-profile-v1/`](docs/audit-evidence/full-ci-everyday-profile-v1/README.es.md)
  (`408` native checks, `1509` checks across `37` examples, normal and ASan/UBSan builds, plus
  independent QuEST 3.7.0 QASM comparison).
- **Current Wave 1A + Wave 1B + Wave 2 + Wave 3 Branch (locally verified normal + ASan/UBSan):**
  expands `QC_MAX_PHYSICAL` from `128` to `1024` wires via multi-word `qc_mask`, raises
  `QC_HEAP_MAX_ELEMENTS` to `1024`, widens the integer divider scratch to `128` bits, integrates
  end-to-end IEEE-754 `float`/`double` and reversible `<math.h>` + float `++`/`--` (`68469` checks
  in `validate_float`), completes the everyday C profile `A1`–`A12` with `7` new integrated examples
  (`3667` checks across `76` examples in `test_examples`, `408` native checks in `cket_tests`, `89`
  sparse 1024-wire checks in `validate_sparse`), and passes
  [`scripts/validate-everyday-profile.py`](scripts/validate-everyday-profile.py) (`O0`/`O2` normal
  and `O1` ASan/UBSan), [`scripts/test.sh`](scripts/test.sh),
  [`scripts/sanitize.sh`](scripts/sanitize.sh),
  [`scripts/test-c11-classical.sh`](scripts/test-c11-classical.sh), and
  [`scripts/inventory_c11.py`](scripts/inventory_c11.py).

**Full Technical Manuscripts & Notes:**

- **English Paper:** [c_ket_paper_en.md](docs/paper/c_ket_paper_en.md) ·
  [PDF](output/pdf/c_ket_paper_en.pdf)
- **Spanish Paper:** [c_ket_paper.md](docs/paper/c_ket_paper.md) · [PDF](output/pdf/c_ket_paper.pdf)
- **Runtime Memory Technical Note (2 pages, ES):**
  [PDF](output/pdf/cket_memoria_runtime_nota_tecnica.pdf) ·
  [Audit Notes Index](docs/paper/README.md)

______________________________________________________________________

## 🏗 Architecture & Compilation Pipeline

Every program passes through real GCC parsing, optimization, and type/promotion semantics before
quantum IR extraction, verification, reversible synthesis, and linking:

```text
  C11 Source (#include <quantum.h>)
       │
       ▼
  ┌─────────────────────────────────────────────────────────────────────┐
  │ cket Driver (src/cket.c)                                            │
  │  Invokes GCC 11 with -std=c11 -D__CKET__=1 and cket_plugin.so       │
  │  Coordinates multi-TU sidecars (.cket_tu) via -c / link inputs      │
  └────────────────────────────┬────────────────────────────────────────┘
                               │
                               ▼
  ┌─────────────────────────────────────────────────────────────────────┐
  │ GCC 11 Plugin Frontend (src/cket_plugin.cpp + src/quantum_*.inc)    │
  │  • Extracts post-frontend GIMPLE (honoring C promotions & casts)    │
  │  • Enforces single ownership, no-cloning, borrows & uncomputation   │
  │  • Lowers control flow, structs/arrays/VLAs, heaps & libc adapters  │
  └────────────────────────────┬────────────────────────────────────────┘
                               │
                               ▼
  ┌─────────────────────────────────────────────────────────────────────┐
  │ Typed Quantum IR: cket.qir v1 JSON (validated by src/engine/verify.c│
  │  & build/cket_backend --validate)                                   │
  └──────────┬───────────────────────────────────────────┬──────────────┘
             │                                           │
             ▼                                           ▼
  ┌─────────────────────────────┐         ┌─────────────────────────────┐
  │ Reversible Synthesis Engine │         │ Static Analysis & Export    │
  │ (src/engine/*.inc, .c)      │         │  • --emit-qasm (OpenQASM 3) │
  │  • Table-free + - * / %     │         │  • --resources (gate/depth) │
  │  • 128-bit divider scratch  │         │  • --emit-ir / --emit-ast   │
  │  • Barrel variable shifts   │         │  • cket_backend --state     │
  │  • IEEE-754 float/double +  │         └─────────────────────────────┘
  │    reversible <math.h>      │
  │  • QFT, phases, QIR modules │
  └──────────────┬──────────────┘
                 │
                 ▼
  ┌─────────────────────────────────────────────────────────────────────┐
  │ Linked Native Linux Executable (src/cket_runtime.c)                 │
  │  • Dense statevector simulator (default, up to 24 physical qubits)  │
  │  • Sparse statevector simulator (CKET_SIMULATOR=sparse, 1024 wires) │
  │  • Zero runtime GCC / Python / external SDK dependency              │
  └─────────────────────────────────────────────────────────────────────┘
```

### Core Components

1. **GCC 11 Frontend Plugin ([`src/cket_plugin.cpp`](src/cket_plugin.cpp) & `src/quantum_*.inc`):**
   Attaches inside GCC rather than re-implementing a toy C parser, so standard header types, integer
   promotions (e.g., `uint8_t x; x * x` promotes to `int`), casts, bitfields, pointers, and
   expressions preserve C11 rules. Frontend integration relies on a provisional internal `volatile`
   marker and differential testing across `-O0`, `-O1`, `-O2`, `-O3`, and `-Os` rather than a formal
   verification of every GCC pass.
1. **Ownership, No-Cloning & Automatic Uncomputation:** Unknown quantum states cannot be implicitly
   duplicated. Initializing `quant uint8_t next = x;` moves ownership out of `x`, while
   `const quant` parameters form checked read-only borrows. Scalar overwrites, checked memory
   updates (`memcpy`, `memmove`, `memset`), and quantum-indexed writes retain versioned history
   needed for inverses and clean derived ancillas automatically without inserting hidden
   measurements. See [`docs/no-clonacion.es.md`](docs/no-clonacion.es.md),
   [`docs/c11-history.md`](docs/c11-history.md), and
   [`docs/c11-readonly-borrows.md`](docs/c11-readonly-borrows.md).
1. **Typed Quantum IR (`cket.qir` v1):** A self-contained JSON format (explicitly **not** LLVM QIR;
   `--emit-llvm` is not implemented) carrying register widths, signedness, ownership transfers,
   coherent controls, CPU bindings, deferred angles, and modular circuit calls. Audited
   independently by [`src/engine/verify.c`](src/engine/verify.c) and `cket_backend --validate`; see
   [`docs/ir-verification.md`](docs/ir-verification.md).
1. **Reversible Synthesis Engine (`src/engine/`):** Generates gate networks using table-free
   arithmetic synthesis (`+`, `-`, `*`, `/`, `%`, bit permutations, affine in-place updates via
   `--no-tables`), barrel shifters without per-count-bit word copies
   ([`docs/c11-shifts.md`](docs/c11-shifts.md)), 128-bit integer divider scratch
   ([`docs/c11-division.md`](docs/c11-division.md)), and IEEE-754 `float`/`double` + reversible
   `<math.h>` circuits ([`src/engine/float_arithmetic.inc`](src/engine/float_arithmetic.inc)). Small
   expressions (`<= 12` relevant input bits) also have an optional finite synthesis fallback unless
   `--no-tables` is passed.
1. **Dense & 1024-Wire Sparse Runtime ([`src/cket_runtime.c`](src/cket_runtime.c),
   [`src/engine/simulator.c`](src/engine/simulator.c),
   [`src/engine/sparse_simulator.inc`](src/engine/sparse_simulator.inc)):** Compiled executables
   link `libcket_quantum.a` and run standalone on Linux/WSL with zero Python or external SDK runtime
   dependencies. Dense mode handles up to `24` physical qubits (`256 MiB` for the statevector
   alone). Opt-in sparse mode (`CKET_SIMULATOR=sparse`) tracks up to `1024` physical wires
   (`QC_MAX_PHYSICAL`) over bounded occupied support (`CKET_SPARSE_STATES`, default `65536`),
   emitting an explicit diagnostic if support capacity is exceeded. See
   [`docs/simulacion-dispersa.es.md`](docs/simulacion-dispersa.es.md).

______________________________________________________________________

## 💡 Language Tour & Code Examples

### 1. First Program & Register Lifecycle

[`examples/hello.c`](examples/hello.c) prepares a basis state, adds `5` reversibly, and measures it:

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

```bash
build/cket examples/hello.c --run -o build/hello
# Output: 12

# Or compile once with -O2 and run the native binary directly:
build/cket examples/hello.c -O2 -o build/hello
./build/hello
```

Register lifecycle primitives enforce explicit observation and cleanup:

```c
quant uint8_t basis = 7;                // Basis state |7> (CPU value evaluated once).
quant uint8_t x = superposition(0, 15); // Uniform superposition over 0..15 (dyadic [a, a+2^k-1]).
uint8_t result = measure(x);            // Collapses and returns a classical C uint8_t.
reset(basis);                           // Destroys state and leaves zeroed storage.
```

For reproducible runs across probabilistic measurements, use `--seed N` with `--run` or set
`CKET_SEED=N`:

```bash
build/cket examples/prepare_measure.c --run --seed 7 -o build/prepare
CKET_SEED=7 ./build/prepare
```

### 2. Entanglement, Coherent Conditions & Generated OpenQASM 3

A classical-looking `if` on a `quant` condition applies coherent quantum controls without measuring
the predicate ([`examples/entanglement.c`](examples/entanglement.c)):

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

Running `build/cket examples/entanglement.c --emit-qasm` exports the exact reversible circuit
including condition allocation and automatic ancilla uncomputation, while `--resources` reports
qubit/gate usage:

```text
OPENQASM 3.0;
include "stdgates.inc";
qubit[3] q;
bit[2] results;
h q[0];
ctrl(1) @ x q[0], q[2];
ctrl(1) @ x q[2], q[1];
ctrl(1) @ x q[0], q[2];
results[0] = measure q[0];
results[1] = measure q[1];
```

Coherent phase kickback with temporary predicates also uncomputes intermediates cleanly
([`examples/coherent.c`](examples/coherent.c)):

```c
#include <quantum.h>
#include <stdio.h>

int main(void) {
    quant uint8_t x = superposition(0, 15);
    quant bool flag = (x * x == 9);
    if (flag) {
        phase_flip();
    }
    x += 5;
    printf("%u\n", (unsigned)measure(x));
    return 0;
}
```

### 3. Grover Search: Known vs. Unknown Solution Counts

`find(x, predicate)` applies amplitude amplification assuming **exactly one** solution
(`find_known(x, predicate, M)` accepts known count `1 <= M <= N`), consuming `x` and returning the
amplified register ([`examples/find.c`](examples/find.c)):

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

When the solution count is **unknown (including 0)**, use `search(x, predicate, budget)`
([`examples/search.c`](examples/search.c)), which samples, checks the predicate classically, and
retries up to a constant budget (`1..4096`), failing with probability bounded by `(7/8)^attempts` if
solutions exist:

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
build/cket examples/find.c --run --seed 7 -o build/find
# Output: 3
build/cket examples/search_empty.c --run --seed 7 -o build/search-empty
# Output: Search budget exhausted
```

### 4. IEEE-754 `float`/`double` & Reversible `<math.h>`

Wave 1B + Wave 2 + Wave 3 add full binary32 (`float`) and binary64 (`double`) registers,
int-to-float and float-to-int conversions, power-of-two scaling, float `++`/`--`, unordered
predicates, and reversible `<math.h>` primitives (see
[`examples/float_double.c`](examples/float_double.c) and
[`examples/everyday_math_geometry.c`](examples/everyday_math_geometry.c)):

```c
#include <math.h>
#include <quantum.h>
#include <stdio.h>

int main(void) {
    quant bool branch = superposition(0, 1);
    quant float fx = branch ? 3.0f : -1.5f;
    quant float fy = branch ? 4.0f : 2.0f;
    quant float norm = sqrtf(fx * fx + fy * fy);

    quant bool kick = superposition(0, 1);
    if (isless(kick ? 1.5f : 4.5f, 3.0f))
        phase(3.14159265358979323846);
    fourier(kick);

    printf("branch=%u norm=%.2f interference=%u\n",
           (unsigned)measure(branch),
           (double)measure(norm),
           (unsigned)measure(kick));
    reset(fx);
    reset(fy);
    return 0;
}
```

### 5. Functions, Structs, Hybrid Loops & Dynamic Heaps

Functions compose across read-only `const` borrows and mutable `quant` parameters
([`examples/functions.c`](examples/functions.c)):

```c
quant bool square_is_nine(quant const uint8_t value) {
    return value * value == 9;
}

void increase(quant uint8_t value, unsigned amount) {
    value += amount;
}
```

The compiler also supports hybrid CPU loops over stable quantum owners (`examples/hybrid_loop.c`),
structs and arrays (`examples/products.c`, `examples/vector_function.c`,
`examples/everyday_matrix_struct.c`), runtime CPU array indexing (`examples/dynamic_vector.c`), and
dynamically allocated quantum heaps via `malloc`/`calloc`/`realloc`/`free`
(`scripts/run-block.sh 05`):

```bash
build/cket examples/hybrid_loop.c --run -o build/hybrid_loop
build/cket examples/products.c --run -o build/products
CC=gcc-11 bash scripts/run-block.sh 05
```

### 6. Phases, QFT & Composable QIR Circuit Modules

`phase(theta)` applies a finite classical angle (in radians) under the active controls,
`phase_flip()` applies `pi`, and `fourier(x)` / `inverse_fourier(x)` apply the positive/inverse
quantum DFT across little-endian registers. Independent unitary `cket.qir` modules resolve relative
to the C source via `circuit("module.qir.json", reg)` and `inverse_circuit("module.qir.json", reg)`
([`examples/circuit_module.c`](examples/circuit_module.c),
[`docs/circuit-abi.md`](docs/circuit-abi.md)):

```bash
build/cket examples/fourier.c --run -o build/fourier
build/cket examples/phase_estimation.c --run --seed 7 -o build/phase_estimation
build/cket examples/circuit_module.c --run -o build/circuit_module
```

### 7. Selected Examples Catalog (`76` Examples Total)

The full suite spans `76` examples across top-level programs, blocks `02`–`06`, and integrated
everyday-profile examples (`3667` checks across `-O0`, `-O1`, `-O2`, `-O3`, and `-Os`). See
[`examples/README.md`](examples/README.md) for inputs and full listings:

| Example                                                                      | Concept Demonstrated                                           | Static QASM |
| ---------------------------------------------------------------------------- | -------------------------------------------------------------- | ----------- |
| [`examples/hello.c`](examples/hello.c)                                       | Basis initialization, reversible `+= 5`, measurement           | Yes         |
| [`examples/entanglement.c`](examples/entanglement.c)                         | Hadamard + coherent `if` Bell pair entanglement                | Yes         |
| [`examples/coherent.c`](examples/coherent.c)                                 | Coherent square predicate, `phase_flip()`, auto uncomputation  | Yes         |
| [`examples/find.c`](examples/find.c)                                         | Single-solution Grover search (`find`)                         | Yes         |
| [`examples/find_multiple.c`](examples/find_multiple.c)                       | Known multi-solution Grover (`find_known`)                     | Yes         |
| [`examples/search.c`](examples/search.c)                                     | Unknown-count verified search (`search` with retry budget)     | No          |
| [`examples/float_double.c`](examples/float_double.c)                         | IEEE-754 `float`/`double` arithmetic & phase interference      | Yes         |
| [`examples/everyday_math_geometry.c`](examples/everyday_math_geometry.c)     | `sqrtf`, `fdim`/`fmax`/`fmin`, rounding, unordered preds, `++` | Yes         |
| [`examples/functions.c`](examples/functions.c)                               | `const` borrow predicates and mutable procedure updates        | Yes         |
| [`examples/products.c`](examples/products.c)                                 | Arrays, structures, and classical pointer borrows              | Yes         |
| [`examples/phase_estimation.c`](examples/phase_estimation.c)                 | Controlled phase rotation + inverse QFT (`3/256` eigenphase)   | Yes         |
| [`examples/circuit_module.c`](examples/circuit_module.c)                     | Controlled unitary `increment.qir.json` module & its inverse   | Yes         |
| [`examples/hybrid_loop.c`](examples/hybrid_loop.c)                           | Dynamic CPU loop accumulating quantum operations               | No          |
| [`examples/everyday_dynamic_vector.c`](examples/everyday_dynamic_vector.c)   | Quantum heaps with iteration history in everyday C             | No          |
| [`examples/everyday_buffers_strings.c`](examples/everyday_buffers_strings.c) | Integrated string/memory adapters over fixed & dynamic buffers | No          |
| [`examples/everyday_matrix_struct.c`](examples/everyday_matrix_struct.c)     | Multi-axis indexing, structs, unions, bitfields & bounded VLAs | Yes         |
| [`examples/blocks/02/wide_integers.c`](examples/blocks/02/wide_integers.c)   | `int32`/`size_t64` division via 1024-wire sparse mode          | Yes         |
| [`examples/types.c`](examples/types.c)                                       | 32-bit `quant int` export example (intentionally > 24q dense)  | Yes         |

______________________________________________________________________

## 📊 Capabilities, Contracts & Known Limits

### C11 & Everyday Profile (`A1`–`A12`) Support Matrix

The repository inventory ([`scripts/inventory_c11.py`](scripts/inventory_c11.py),
[`docs/c11-library.json`](docs/c11-library.json)) tracks **1,224 C11 interfaces**: **94 partial**
adapters, **762 pending**, and **368 metadata** facilities.

| Language / Library Area       | Supported Today                                                                                                                                                                                                                                                                                                     | Current Boundaries & Honest Caveats                                                                                                                                                                                                                                                                                                                                                                                                        |
| ----------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Integer types & arithmetic    | Signed/unsigned up to 64-bit, `bool`, `+ - * / %`, bitwise, shifts (barrel shifter), promotions, in-place affine/permutation ops, `div`/`ldiv`/`lldiv`/`imaxdiv`, 128-bit divider                                                                                                                                   | Nonzero divisor must be provable; wide compositions remain bounded by synthesis budgets; integer widths > 24 qubits require sparse mode or static export                                                                                                                                                                                                                                                                                   |
| Floating-point & `<math.h>`   | End-to-end IEEE-754 `float` & `double`, int-float IR bridge, pow2 scaling opt, float `++`/`--`, `fabs`, `copysign`, `signbit`, `fmin`/`fmax`/`fdim`, `trunc`/`floor`/`ceil`/`round`, `sqrt`, unordered predicates                                                                                                   | Transcendental `<math.h>` (`sin`, `cos`, `exp`, `log`, etc.), `long double`, and `_Complex` are out of scope                                                                                                                                                                                                                                                                                                                               |
| Control flow                  | Coherent `if`/`else` & `switch` (ranges, `default`, `break`, `continue`, `return`), CPU `for`/`while`/`do`, bounded `while` + forward `goto`, structured early returns in `void`                                                                                                                                    | Unbounded dynamic quantum loops, arbitrary backward `goto`/unstructured CFG, and general early returns of owned values remain outside scope                                                                                                                                                                                                                                                                                                |
| Functions, pointers & linking | Intra-TU & multi-TU linking (`.cket_tu` unity-build sidecars), `const`/`static` owners, `const` borrows, indirect `quant` fn pointers, bounded recursion & variadics                                                                                                                                                | Unbounded recursion, foreign/dynamic-ABI callbacks reaching empty quantum stubs, and arbitrary pointer aliasing outside checked views are rejected                                                                                                                                                                                                                                                                                         |
| Structs, arrays & heaps       | Multi-axis `quant` indexing (`m[i][j]`), padded structs/unions/bitfields/pointer fields, bounded static VLAs, `malloc`/`calloc`/`aligned_alloc`/`realloc`/`free` (`<= 1024` elts)                                                                                                                                   | Unbounded VLAs, general heap address-aliasing escapes, and static QASM export of dynamic heap loops are out of scope                                                                                                                                                                                                                                                                                                                       |
| Memory & string adapters      | Checked `memcpy`, `memmove`, `memset`, `memcmp`, `strlen`, `strcmp`, `strncmp`, `strcpy`, `strncpy`, `strcat`, `strncat`, `memchr`, `strchr`, `strrchr`, `strpbrk`, `strstr`, `strspn`, `strcspn`, `atoi`/`atol`/`atoll`, `strtol`/`strtoll`/`strtoimax`, `strtoul`/`strtoull`/`strtoumax`, 14 `ctype.h` interfaces | Wave 2 adapters carry explicit partial contracts: `snprintf`/`sscanf` cover bounded single-digit format/parse, `bsearch` checks bounded sorted views with restricted `NULL`/comparator handling, `qsort` uses ascending/descending heuristics rather than general arbitrary comparators and requires non-constant array backing cleanly, and `strtok` validates compile-time delimiter sets rather than full superposed delimiter mutation |

### Resource, Synthesis & Simulator Limits

| Resource / Parameter      | Active Contract                                                                           | Notes & Reference                                                                                                                |
| ------------------------- | ----------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------- |
| Quantum integer width     | Up to `64` bits (`quant float`/`double` also binary32/binary64)                           | [`docs/language.md`](docs/language.md)                                                                                           |
| Gate storage capacity     | Up to `1024` physical wires (`QC_MAX_PHYSICAL`)                                           | Expanded from `128` in `5c5a965` via multi-word `qc_mask` ([`src/engine/internal.h`](src/engine/internal.h))                     |
| Dense simulator limit     | Up to `24` physical qubits (default runtime mode)                                         | Statevector alone uses `256 MiB` at `24` qubits; vector dimension never shrinks during execution                                 |
| Sparse simulator limit    | Up to `1024` physical wires, `65536` occupied states default                              | Opt-in via `CKET_SIMULATOR=sparse` and `CKET_SPARSE_STATES` ([`docs/simulacion-dispersa.es.md`](docs/simulacion-dispersa.es.md)) |
| Finite synthesis fallback | At most `12` relevant input bits                                                          | Disabled cleanly with backend `--no-tables`                                                                                      |
| Symbolic synthesis budget | `256` terms per bit, `2,000,000` work steps                                               | Prevents exponential blowup during expression synthesis                                                                          |
| IR & loop bounds          | `100,000` tokens/expr, `128` nesting levels, `64 MiB` input, `4096` fixed loop iterations | Enforced in [`src/engine/verify.c`](src/engine/verify.c) and frontend loop unroller                                              |
| Divider & heap bounds     | `128`-bit integer divider scratch; `1024`-element quantum heaps (`QC_HEAP_MAX_ELEMENTS`)  | Expanded in Wave 1A/1B ([`src/engine/arithmetic.inc`](src/engine/arithmetic.inc), [`src/engine/heap.inc`](src/engine/heap.inc))  |

### Verification, Sanitizer & Independent QuEST Evidence

Checks combine native C tests, optimizer differential campaigns across
`-O0`/`-O1`/`-O2`/`-O3`/`-Os`, AddressSanitizer/UndefinedBehaviorSanitizer builds, reduced density /
full statevector checks, and independent OpenQASM execution on **QuEST 3.7.0** (serial C backend
pinned to commit `d4f75f724993b4af8e43a796e3c09ce24ae11670`, SHA-256
`1a5939798a70b749dfa57cfaa116ece03a2d36aa59c1b712cb3dc2103ab1cded`, comparing amplitudes within
`1e-10` up to global phase; QuEST is downloaded into `build/validation-deps/` only for verification
and never linked into CKet binaries).

| Verification Suite / Tool                                           | Verified Scope                                                                                                                                                                                                                                                                                                                                                                                                | Pipeline Availability                                                 |
| ------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------- |
| `scripts/test.sh` (`cket_tests`, examples)                          | `408` native checks + `3667` checks across `76` examples (`-O0/-O1/-O2/-O3/-Os` + CLI/seed contracts)                                                                                                                                                                                                                                                                                                         | Verified locally on Wave 3 + CI                                       |
| `scripts/sanitize.sh` / `sanitize-all.sh`                           | ASan + UBSan execution across native (`408`), example, and validation harnesses                                                                                                                                                                                                                                                                                                                               | Verified locally on Wave 3 + CI                                       |
| `validate_float`                                                    | `68,469` IEEE-754 `float`/`double`, `<math.h>`, pow2 optimizer, int-float bridge & `++`/`--` checks                                                                                                                                                                                                                                                                                                           | Verified locally on Wave 3 + `scripts/verify.sh`                      |
| `validate-everyday-profile.py`                                      | Everyday profile `A1`–`A12` (`O0`/`O2` normal & `O1` ASan/UBSan): `36` traces, `36` inverses, `272` prog, `6` multi-TU, `56` integrated, `34` negative                                                                                                                                                                                                                                                        | Verified locally on Wave 3 + `scripts/verify.sh`                      |
| `validate_sparse`                                                   | `89` checks exercising `1024`-wire sparse execution and support diagnostics                                                                                                                                                                                                                                                                                                                                   | Verified locally on Wave 3 + `scripts/verify.sh`                      |
| `validate_expressions` / `validate_division` / `inplace` / `shifts` | `3509` pure expr + `1018` division (`64`-bit & `div`) + `271` in-place + `203` barrel shift checks                                                                                                                                                                                                                                                                                                            | Verified locally on Wave 3 + `scripts/verify.sh`                      |
| `validate_readonly_borrows` / `function_entries` / `harness`        | `305` readonly borrow + `90` checked ABI entry + `18` fault-injection harness checks                                                                                                                                                                                                                                                                                                                          | Verified locally on Wave 3 + `scripts/verify.sh`                      |
| `test-c11-classical.sh` & `validate_library`                        | `5/5` finite C11 programs (29 header families vs GCC) + `2100` libc boundary checks (210 APIs)                                                                                                                                                                                                                                                                                                                | Verified locally / `scripts/verify.sh`                                |
| Full `verify.sh` & archived `5c5a965` validators                    | `validate_patches` (`55,155`), `swap_rebind` (`35,871`), `phase` (`3380`), `history` (`2332`), `controlled_operands` (`1270`), `memset` (`940`), `byte_snapshots` (`650`), `bindings` (`519`), `strings` (`460` + `640`), `memcopy` (`414`), `memory_sequences` (`360`), `memcmp` (`302`), `strncmp` (`246`), `profiles` (`96`), `numeric_locale` (`70`), plus `70` QuEST 3.7.0 QASM state/measurement checks | Full CI archived at `5c5a965`; invoked locally by `scripts/verify.sh` |

______________________________________________________________________

## ⚙️ CLI Reference & Environment Variables

### `cket` Driver (`build/cket` or `bash cket`)

| Option / Argument                 | Description                                                                                       |
| --------------------------------- | ------------------------------------------------------------------------------------------------- |
| `<program.c> ...`                 | One or more C11 source files or `.o` objects (paired with `.cket_tu` sidecars when `-c` was used) |
| `-o <path>` / `--output <path>`   | Output executable, object, or IR/QASM/AST file path                                               |
| `-c`                              | Compile only; writes `.o` plus a `.cket_tu` translation-unit sidecar for later linking            |
| `--run`                           | Compile, link, and immediately execute the generated binary                                       |
| `--seed N`                        | Set nonnegative 64-bit simulation seed when running with `--run`                                  |
| `--emit-ast`                      | Dump the GCC GIMPLE tree consumed by the plugin frontend                                          |
| `--emit-ir` / `--emit-quantum-ir` | Emit validated `cket.qir` version 1 JSON                                                          |
| `--emit-qasm`                     | Export static quantum operations to OpenQASM 3                                                    |
| `--resources`                     | Print logical/physical qubit counts, ancillas, gate breakdown, and depth                          |
| `--ctype-locale <name>`           | Pin an explicit static character profile locale (required for static export of `ctype.h`)         |
| `--gcc <compiler>`                | Select the GCC executable matching the compiled plugin (`gcc-11`, etc.)                           |
| `[GCC options]`                   | Single-token pass-through GCC flags (`-O2`, `-I/path/to/headers`, `-DNAME=1`)                     |
| `--version` / `--help`            | Print compiler version or usage summary                                                           |

### Standalone Backend Tool (`build/cket_backend`)

| Option                         | Description                                                                                   |
| ------------------------------ | --------------------------------------------------------------------------------------------- |
| `--validate <ir.json>`         | Validate a `cket.qir` document structurally and semantically                                  |
| `--emit-qasm <ir.json>`        | Lower a validated `cket.qir` document to OpenQASM 3                                           |
| `--resources <ir.json>`        | Compute resource estimates without running simulation                                         |
| `--state <ir.json>`            | Dump non-zero complex amplitudes in JSON for debugging (before measurements)                  |
| `--specialize-ctype <ir.json>` | Specialize an IR document for a concrete `ctype` locale                                       |
| `--no-tables`                  | Reject finite truth-table synthesis fallback; require symbolic/register synthesis             |
| `--bindings <values.json>`     | Supply concrete classical binding values (required for `--state` or static QASM on templates) |
| `--ctype-locale <name>`        | Specify the locale name for character classification rules                                    |

```bash
# Inspect generated GCC AST, typed IR, OpenQASM 3, and resources without tables:
build/cket examples/find.c --emit-ast -o build/find.ast
build/cket examples/find.c --emit-ir -o build/find.qir.json
build/cket examples/find.c --emit-qasm -o build/find.qasm
build/cket examples/find.c --resources -o build/find.resources.txt
build/cket_backend --validate build/find.qir.json
build/cket_backend --resources build/find.qir.json --no-tables
```

### Environment Variables

| Variable             | Purpose                                                                                                   |
| -------------------- | --------------------------------------------------------------------------------------------------------- |
| `CC` / `CXX`         | Host C/C++ compilers used to build and run CKet (e.g. `gcc-11` / `g++-11`)                                |
| `GCC_PLUGIN_INCLUDE` | Custom path to GCC plugin headers (`gcc-plugin.h`); also auto-discovers headers under `.deps/gcc/`        |
| `CKET_SEED`          | Deterministic PRNG seed read by generated executables at runtime                                          |
| `CKET_SIMULATOR`     | Set to `sparse` to opt into the 1024-wire sparse statevector engine (`dense` is default)                  |
| `CKET_SPARSE_STATES` | Maximum occupied computational basis labels allowed in `sparse` mode (default `65536`)                    |
| `CKET_FUZZ_CASES`    | Override number of generated frontend differential test cases in [`scripts/verify.sh`](scripts/verify.sh) |
| `CKET_FUZZ_SEED`     | Override PRNG seed for [`fuzz_frontend`](tests/fuzz_frontend.c)                                           |

______________________________________________________________________

## 🚀 Installation, Build & Verification

### Requirements

- **OS:** Linux or WSL (reference environment: Ubuntu 22.04)
- **Toolchain:** `gcc-11`, `g++-11`, `gcc-11-plugin-dev`, `binutils` (the driver checks that the
  active GCC version matches the GCC headers used to build `cket_plugin.so`)
- **Optional (full CI verification & formatting):** `python3`, `curl`, `ca-certificates`, `locales`

### Clone & Build

```bash
git clone https://github.com/igna-s/cket.git
cd cket
sudo apt-get update
sudo apt-get install -y gcc-11 g++-11 gcc-11-plugin-dev binutils
export CC=gcc-11
export CXX=g++-11
bash scripts/build.sh
build/cket --version
```

### Run Tests & Full Audits

```bash
# Fast core build + native unit suite (408 checks) + all 76 examples (-O0..-Os):
bash scripts/test.sh

# Run examples standalone, or run specific feature blocks (02=sparse, 05=heaps, 06=strings):
bash scripts/run-examples.sh --all
bash scripts/run-block.sh 02
bash scripts/run-block.sh 05
bash scripts/run-block.sh 06

# Everyday profile matrix + classical C11 comparison + ASan/UBSan:
python3 scripts/validate-everyday-profile.py --levels O0 O2
bash scripts/test-c11-classical.sh
bash scripts/sanitize.sh

# Full verification suite (rebuilds, runs all validate_* tools, fetches & builds QuEST 3.7.0,
# verifies OpenQASM 3 statevectors & measurements, and re-runs under ASan/UBSan):
sudo apt-get install -y curl ca-certificates locales
bash scripts/verify.sh

# Check machine-readable C11 interface inventory:
python3 scripts/inventory_c11.py
```

### Troubleshooting Quick Reference

- **GCC version or header mismatch:** install `gcc-11-plugin-dev` matching your compiler, set
  `CC`/`CXX` (or `GCC_PLUGIN_INCLUDE`), and rebuild via `scripts/build.sh`.
- **Quantum value used as classical data:** call `measure(x)` explicitly; keep I/O, CPU mutations,
  and classical jumps outside quantum conditions.
- **Consumed owner / live dependency diagnostic:** end derived borrows before overwriting inputs,
  and do not reuse a variable after an ownership move or `find(...)`.
- **Dense 24-qubit limit exceeded:** shrink register widths, export statically with `--emit-qasm`,
  or run with `CKET_SIMULATOR=sparse` for low-entanglement/low-support circuits up to `1024` wires.
- **Dynamic construct rejected during `--emit-qasm`:** dynamic CPU loops, runtime inputs,
  `search(...)`, and runtime angles cannot flatten into static OpenQASM 3 and must execute natively.

______________________________________________________________________

## 📂 Repository Structure

```text
cket/
├── cket                            # Convenience shell launcher forwarding to build/cket
├── include/
│   └── quantum.h                   # Public C11 header (quant qualifier, math/search/phase/circuit macros)
├── src/
│   ├── cket.c                      # Native compiler driver (GCC + plugin + backend + linker)
│   ├── cket_plugin.cpp             # GCC 11 GIMPLE plugin entry & C++17 AST/IR bridge
│   ├── quantum_frontend.inc        # Function, struct/array, loop & multi-TU lowering
│   ├── quantum_semantics.inc       # Types, ownership, history, coherent control & borrows
│   ├── quantum_*.inc               # Memory, heap, phase, search, decimal & string C11 adapters
│   ├── cket_runtime.c              # Native C runtime linked into compiled executables
│   ├── heap_runtime.inc            # Runtime-sized quantum heap metadata & execution support
│   ├── cket_backend.c              # Standalone IR validator, QASM exporter, state & resource CLI
│   └── engine/
│       ├── quantum.c / quantum.h   # Typed IR engine, uncomputation & OpenQASM 3 exporter
│       ├── verify.c                # Structural & semantic IR validator
│       ├── simulator.c             # Default 24-qubit dense statevector simulator
│       ├── sparse_simulator.inc    # Opt-in 1024-wire sparse statevector simulator
│       ├── arithmetic.inc          # Integer arithmetic, comparisons & 128-bit divider scratch
│       ├── float_arithmetic.inc    # IEEE-754 binary32/binary64 & reversible <math.h> circuits
│       └── *.inc                   # Bindings, in-place, phase, heap, ctype & swap_rebind synthesis
├── examples/
│   ├── *.c                         # 47 top-level & everyday C programs + increment.qir.json
│   └── blocks/02..06/              # 29 focused feature examples (sparse, buffers, heaps, strings)
├── tests/                          # Unit tests, validator binaries, fuzzers & QuEST QASM reference
├── scripts/                        # Build, test, sanitizer, everyday-profile & formatting scripts
├── docs/
│   ├── paper/                      # Full English & Spanish papers + technical audit notes
│   ├── audit-evidence/             # Reproducible CI & verification evidence capsules
│   ├── releases/                   # Release notes (v1.0.0-rc.3, v0.4.0)
│   └── *.md / c11-*.md             # Language specs, C11 coverage matrix & subsystem contracts
├── output/
│   ├── pdf/                        # Compiled PDFs (c_ket_paper_en.pdf, c_ket_paper.pdf, 2p note)
│   ├── demo/                       # Standalone demo instructions
│   └── sdk/                        # Historical Windows/WSL SDK snapshots
└── .github/workflows/verify.yml    # GitHub Actions formatting, inventory, QuEST & ASan/UBSan CI
```

______________________________________________________________________

## 📚 Documentation, Papers & Audit Evidence

| Category                  | Document                                                                                                                            | Topic                                                                   |
| ------------------------- | ----------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------- |
| Papers & Technical Notes  | [`docs/paper/c_ket_paper_en.md`](docs/paper/c_ket_paper_en.md) ([PDF](output/pdf/c_ket_paper_en.pdf))                               | Full English manuscript on CKet design, semantics, and verification     |
| Papers & Technical Notes  | [`docs/paper/c_ket_paper.md`](docs/paper/c_ket_paper.md) ([PDF](output/pdf/c_ket_paper.pdf))                                        | Full Spanish manuscript                                                 |
| Papers & Technical Notes  | [`output/pdf/cket_memoria_runtime_nota_tecnica.pdf`](output/pdf/cket_memoria_runtime_nota_tecnica.pdf)                              | Two-page technical note on runtime quantum memory                       |
| Papers & Technical Notes  | [`docs/paper/current-evidence-map-2026-10-26.es.md`](docs/paper/current-evidence-map-2026-10-26.es.md)                              | Mapping between historical manuscript claims and post-RC3 updates       |
| Language & Architecture   | [`docs/language.md`](docs/language.md)                                                                                              | Core language syntax, types, promotions, and semantic boundaries        |
| Language & Architecture   | [`docs/ir-verification.md`](docs/ir-verification.md)                                                                                | `cket.qir` format rules and verification checks                         |
| Language & Architecture   | [`docs/circuit-abi.md`](docs/circuit-abi.md)                                                                                        | Modular `circuit()` / `inverse_circuit()` QIR ABI contract              |
| Language & Architecture   | [`docs/perfil-c-cotidiano.es.md`](docs/perfil-c-cotidiano.es.md)                                                                    | Everyday C profile (`A1`–`A12`) requirements and status (Spanish)       |
| Language & Architecture   | [`docs/estado-general-del-proyecto.es.md`](docs/estado-general-del-proyecto.es.md)                                                  | Comprehensive subsystem status summary (Spanish)                        |
| Language & Architecture   | [`docs/simulacion-dispersa.es.md`](docs/simulacion-dispersa.es.md)                                                                  | 1024-wire sparse simulation engine guide (Spanish)                      |
| Language & Architecture   | [`docs/no-clonacion.es.md`](docs/no-clonacion.es.md)                                                                                | Ownership, move/borrow rules, and retained-history overwrites (Spanish) |
| Roadmap & Coverage        | [`docs/c-coverage.md`](docs/c-coverage.md) · [`docs/c11-roadmap.md`](docs/c11-roadmap.md)                                           | C11 library coverage matrix and implementation roadmap                  |
| Roadmap & Coverage        | [`docs/v1-plan.md`](docs/v1-plan.md) · [`docs/c11-library.json`](docs/c11-library.json)                                             | Version 1.0 milestone plan and 1,224-entry C11 interface inventory      |
| Audit Evidence & Releases | [`docs/audit-evidence/README.md`](docs/audit-evidence/README.md)                                                                    | Guide to reproducible audit capsules and evidence sources               |
| Audit Evidence & Releases | [`docs/audit-evidence/full-ci-everyday-profile-v1/`](docs/audit-evidence/full-ci-everyday-profile-v1/README.es.md)                  | Archived GitHub Actions run `42507019584` (`5c5a965`) evidence bundle   |
| Audit Evidence & Releases | [`docs/repository-audit.md`](docs/repository-audit.md) · [`docs/audit-validation.md`](docs/audit-validation.md)                     | Reproduced findings, fixes, and validation harness guarantees           |
| Audit Evidence & Releases | [`docs/releases/v1.0.0-rc.3.es.md`](docs/releases/v1.0.0-rc.3.es.md) · [`v0.4.0`](docs/releases/v0.4.0.md)                          | Experimental release candidate and historical notes                     |
| Demos & Community         | [`output/demo/README.es.md`](output/demo/README.es.md) · [`output/sdk/loop-jumps/README.es.md`](output/sdk/loop-jumps/README.es.md) | Executable demo & historical Windows/WSL SDK snapshots                  |
| Demos & Community         | [`CONTRIBUTING.md`](CONTRIBUTING.md) · [`SECURITY.md`](SECURITY.md)                                                                 | Contribution workflow, CI commands, and vulnerability reporting         |

______________________________________________________________________

## 📄 License

This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details
(`Copyright (c) 2026 Ignacio Schwindt and contributors`).
