# `|c>`: C with high-level quantum data

**A new quantum variant of C for GCC-based compilers.**

[![Verification](https://github.com/igna-s/cket/actions/workflows/verify.yml/badge.svg)](https://github.com/igna-s/cket/actions/workflows/verify.yml)
[![Version](https://img.shields.io/badge/version-0.4.0--experimental-orange)](docs/releases/v0.4.0.md)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

`|c>` is an experimental extension of C11 with the `quant` qualifier. Its `cket` driver uses GCC's real C frontend, generates typed Quantum IR, and runs circuits through a native C statevector engine. Programs use high-level expressions instead of writing gates or managing ancillas.

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

The compiler builds the oracle, amplitude amplification, and workspace cleanup. `find` promises exactly one solution; use `find_known` for another known count or `search` when the count is unknown.

The current driver is **0.4.0**, tested with **GCC 11.4.0**. It remains a prototype: version 1.0 has [seven explicit acceptance requirements](docs/v1-plan.md). CPU simulation, OpenQASM 3 export, and supported quantum operations work; a QPU backend and full quantum coverage of C11 remain pending.

## Quick start

Use Linux or WSL. The reference environment is Ubuntu 22.04 with GCC 11 and matching plugin development headers:

```bash
sudo apt-get update
sudo apt-get install -y gcc-11 g++-11 gcc-11-plugin-dev binutils
export CC=gcc-11
export CXX=g++-11
bash scripts/build.sh
bash scripts/test.sh
build/cket examples/find.c --run --seed 7 -o build/demo
```

The demo prints `3` with this seed. A generated executable can run directly:

```bash
CKET_SEED=7 ./build/demo
```

**Python is not required for building, running, or testing the project.** The driver, backend, runtime, engine, and test programs are C. The GCC plugin is **C++17** because it uses GCC's internal API; shell scripts coordinate builds and commands. The project therefore does not claim to be entirely C.

For another GCC version, install matching plugin headers. `scripts/build.sh` also accepts `GCC_PLUGIN_INCLUDE` and headers extracted into `.deps/`. The driver checks GCC's version against the one used to build the plugin. Requirements include binutils and libc/libm.

## Repository structure and why it is split this way

```text
.
├── src/          Compiler driver and GCC frontend integration
├── include/      Public language header used by source programs
├── native/       IR verification, reversible synthesis, and simulation
├── runtime/      Bridge called by generated CPU executables
├── backend/      Standalone IR validation/export/inspection tool
├── tests/        C tests, generators, and independent QASM validation
├── examples/     Small C programs and a source QIR circuit module
├── scripts/      Shell build and verification commands
├── docs/         Language contracts, coverage, ABI, roadmap, release notes
├── .github/      CI workflow and contributor-facing templates
├── build/        Generated binaries, logs, circuits, validation dependencies
├── .deps/        Optional locally extracted GCC development headers
├── cket          Optional shell launcher for build/cket
└── LICENSE       MIT license
```

The split follows compiler responsibilities. `src/` understands C and GCC trees; `native/` works with verified quantum operations and does not parse C. `runtime/` connects generated CPU code to that engine. `backend/` exposes the same engine for export and debugging without compiling a C program. Keeping the public `include/quantum.h` separate from internal `native/quantum.h` separates source-language syntax from the engine API.

Tests and examples have different purposes: tests establish expected behavior and rejection cases; examples show how to use the language. Scripts coordinate native programs rather than implementing tests in another language. `docs/` contains detailed contracts so this README can introduce the project without duplicating every specification.

`build/` and `.deps/` are generated/local directories excluded from Git. They are not source modules. Downloaded `.deb` files can be removed after extraction; keep `.deps/` while a build still uses its headers. Build artifacts can be regenerated.

The `.inc` files are source fragments included into a larger C or C++ translation unit. They split large implementations while sharing internal helpers; they are not separately compiled libraries. This is a practical organization for the current prototype, rather than a finalized module system.

| Component | Responsibility |
|---|---|
| [src/cket.c](src/cket.c) | Native driver: invokes GCC, validates IR, exports artifacts, links executables. |
| [src/cket_plugin.cpp](src/cket_plugin.cpp) | GCC plugin registration, attributes, declarations, semantic types. |
| [src/quantum_semantics.inc](src/quantum_semantics.inc) | Expressions, ownership, effects, coherent control, cleanup. |
| [src/quantum_frontend.inc](src/quantum_frontend.inc) | Function composition, fixed products, pointer borrows, bounded loop expansion. |
| [include/quantum.h](include/quantum.h) | Public `quant` syntax and high-level intrinsics. |
| [native/quantum.c](native/quantum.c) | Registers, reversible operations, ancillas, Grover, export, resources. |
| [native/structured.inc](native/structured.inc), [native/arithmetic.inc](native/arithmetic.inc) | Symbolic and register arithmetic synthesis with complexity budgets. |
| [native/circuit.inc](native/circuit.inc) | Independent unitary modules, borrow remapping, control, inversion. |
| [native/verify.c](native/verify.c) | IR schema/type checks, ranges, dependency and uncomputation certificates. |
| [native/simulator.c](native/simulator.c) | Statevector evolution, measurement, cleanup checks. |
| [native/json.c](native/json.c) | Serialized IR parser; GCC remains the sole C parser. |
| [runtime/cket_runtime.c](runtime/cket_runtime.c) | Linked bridge between CPU code and quantum engine. |
| [backend/cket_backend.c](backend/cket_backend.c) | Standalone validation, QASM, resource estimates, amplitude inspection. |

## How compilation and execution work

The plugin runs at `PLUGIN_PRE_GENERICIZE`, before GCC lowers the C AST to its generic representation. GCC handles includes, macros, declarations, expressions, promotions, and classical C compilation.

```text
C source + quantum.h
        |
        v
GCC AST + quantum semantic analysis
        |
        +-- CPU code with runtime calls --> native object/executable
        |
        +-- typed Quantum IR --> verification --> reversible gates
                                                    |
                                                    +-- OpenQASM 3
                                                    +-- resource estimates
```

At runtime, generated CPU code calls the linked C engine. Quantum gates evolve amplitudes; explicit `measure()` returns a classical result and CPU execution continues. No Python process or external quantum SDK executes the program.

The runtime carries serialized operations but caches parsed documents and reusable gate plans. Each call still checks types, availability, lifetimes, and dependency versions. Cache keys include expression, proven support, wires, and controls. Private workspace is reusable only when its wires are free. Limits are 16 MiB of parsed documents, 64 MiB of plans, and 64 MiB of modules.

CPU values parameterize additions, phases, and indexed selection. Independent unitary modules compile once on first use. Eight repeated computations and their inverses check five parses, one synthesis, and fifteen plan hits. General ahead-of-time gate linking remains pending: the first call may still synthesize.

## Language behavior

`quant` currently expands to a registered GCC declaration attribute:

```c
#define quant __attribute__((cket_quant))
#define cuant quant
#define Cuant quant
```

It is not yet a native keyword or type qualifier inside GCC. The plugin and IR distinguish classical and quantum data while preserving C width and signedness:

```text
SemanticType = { GCC_C_Type, quantum, integer_width, signedness }
```

Supported types include integers/Booleans up to 64 bits, fixed multidimensional arrays, and integer structures. Global quantum owners, VLAs, unions, bitfields, quantum floating point, and arbitrary cross-unit C quantum function linking are unsupported. Owners are automatic locals in `main`, including locals of expanded functions.

The internal `volatile` marker preserves accesses through some early GCC simplifications. It is a provisional frontend measure, rather than proof of every optimization's quantum correctness.

### Preparation and measurement

```c
quant uint8_t basis = 7;
quant uint8_t x = superposition(0, 255);
uint8_t result = measure(x);
printf("%u\n", (unsigned)result);
reset(basis);
```

Uniform preparation supports constant intervals `[0, 2^k-1]`. Arbitrary uniform intervals and dynamic interval bounds are rejected. Basis initialization may use a CPU value evaluated once, such as `quant uint8_t x = read_value();`; dynamic preparation requires native execution.

An uninitialized `quant uint8_t x;` starts in `|0>` in this language. This differs from an uninitialized classical C automatic variable.

`measure(x)` explicitly collapses the register, preserves correlations, and returns its C-typed result. It does not automatically make storage clean: a measured bit may be one. `reset(x)` explicitly destroys information and leaves zeroed storage. Cleanup never inserts hidden measurement or reset.

### Expressions and ownership

Pure expressions support arithmetic `+ - * / %`, comparisons, bit operators, constant shifts, Boolean operators, and conditional expressions. Typed IR retains promotions and conversions. For example, `uint8_t x * x` is promoted to `int`; a square is not silently truncated to eight bits before comparison. Division truncates toward zero and remainder follows C's sign rules. Pure `&&`, `||`, and `?:` preserve conditional arithmetic evaluation.

New results preserve inputs:

```c
quant uint8_t y = x % 2;  // |x>|0> -> |x>|x % 2>
```

This may entangle registers; it does not create an independent clone. The general computation uses XOR accumulation, `|x>|y> -> |x>|y XOR f(x)>`.

Direct same-type initialization transfers ownership:

```c
quant uint8_t b = a;     // b owns the existing register; a is consumed.
```

Compatible typedefs are accepted; equal width/signedness alone do not permit moves between distinct canonical C types. Width/signedness changes compute a value conversion and preserve the input.

Unsigned `+=`, `-=`, and supported XOR updates are reversible. Register addition/subtraction preserves a distinct source and uses direct synthesis without tables or ancillas. CPU operands are evaluated once. General in-place transformations must be injective over the entire bit space; `x = x % 2` and unknown-state overwrites are rejected. General assignment to occupied storage and automatic history preservation remain pending.

### Control flow, cleanup, and functions

Quantum `if/else` computes its condition in an ancilla, controls both branches coherently, then uncomputes and releases the ancilla. Nested conditions accumulate controls. I/O, CPU writes, classical calls, measurement/reset, inline assembly, and classical jumps are rejected under coherent control. Accepted quantum procedures can compose under those controls.

Changing inputs needed to invert a condition is rejected. For example, `if (x == 0) x += 1;` maps both 0 and 1 to 1 and is non-injective over x. An independent flag can control another register's update.

CPU `for/while/do` loops retain actual CPU execution. They support unsigned updates, indexed operations, coherent `if` with stable owners, and explicit measurements feeding later CPU decisions. Joins retain facts true on every path, including zero loop iterations. General dynamic owners, early returns with pending lifetimes, and complete CFG analysis remain pending. General coherent loops are unsupported.

Static `for` loops with checked constant bounds and local counters may expand up to 4096 iterations. Jumps, body changes to the counter, and bounds requiring overflow are rejected. Dynamic CPU control cannot be flattened into static QASM.

Computed results retain their creating expression, normalized controls, and input/control versions. Cleanup reverses dependency order and requires the same certificate with available dependencies. Pending inputs cannot be changed, measured, reset, or released. Diagonal phases may preserve the inverse while invalidating uniformity for Grover. JSON-text certificate comparison may conservatively reject equivalent serializations. Simulation also checks probability outside zero below `1e-10` before reuse.

Functions expand over GCC's AST. Definitions must precede calls in the same translation unit. Scalar/pointer/array parameters borrow registers; mutable borrows must not overlap or be consumed. Pure conditional returns and terminal owned scalar returns are supported. General product returns, recursion, indirect quantum calls, and arbitrary C quantum ABI linking remain pending.

Independent unitary QIR files compose through `circuit("module.qir.json", x)` and `inverse_circuit`. Their files are embedded during compilation; runtime plans are cached. Control preserves global phase as relative phase. See the [circuit ABI](docs/circuit-abi.md).

### Arrays, pointers, phases, and Fourier

Pointers to quantum data have classical addresses and borrow existing registers; they do not create addresses in superposition. Constant offsets and bounds are checked. Pointer reinterpretation without a byte-access contract, dynamic reassignment, escape into CPU memory, and dynamic quantum allocation are rejected.

Fixed scalar arrays support CPU indices for unsigned updates, measurement/reset, and Fourier/inverse. The index is evaluated once without measuring the selected register. Reads into CPU still require `measure(values[index])`. Resetting one selected element does not prove every array owner clean; [dynamic_vector.c](examples/dynamic_vector.c) explicitly resets both owners.

`phase(theta)` applies a finite classical angle to active controls; without controls it is global. Runtime-computed angles require native execution. `phase_flip()` applies pi.

`fourier(x)` implements the positive quantum DFT over the entire little-endian register; `inverse_fourier(x)` reverses it. Both accept coherent controls, preserve correlations, and use quadratic logical synthesis without tables. Tests compare amplitudes against an independent DFT to `1e-10`. A general algorithmic error budget remains pending.

## Arithmetic synthesis and complexity

The backend first attempts structured synthesis. Supported bit operators, casts, constant shifts, and predicates become symbolic Boolean polynomials or directly emitted controls. This avoids domain enumeration and supports some predicates over 32-bit registers.

Symbolic limits are **256 terms per bit** and **two million work steps**, including term comparisons. IR expression limits are **100,000 tokens** and **128 levels**. Exhaustion produces a diagnostic; avoiding tables alone does not prove scalable composition.

Register circuits cover addition/subtraction, multiplication, division, remainder, and register comparisons beyond 12 relevant bits. Negative operands require proven ranges and absence of signed overflow. Division truncates toward zero; remainder retains the dividend's sign. A divisor must be proven nonzero by range or a fixed-one bit; `MIN/-1` is rejected. Private workspace is computed, copied by XOR, and uncomputed.

The optional finite path evaluates at most **12 relevant input bits** (4096 combinations), then emits a real reversible circuit using minterms or algebraic normal form. General in-place permutations also retain a 12-bit limit. Engine-maintained proofs validate `known` and `active` metadata before reducing support.

Use backend `--no-tables` to require structured synthesis. The C growth benchmark covers six operators at widths 4, 8, 12, and 16, producing **24 circuits**. Two 16-bit inputs mean **32 relevant bits**. Modular 16-bit multiplication records **1680 logical gates and 16 ancillas**, before MCX decomposition.

Current controlled-increment constructions have up to cubic logical gate cost and linear workspace for a binary operator. Larger compositions, more precise ranges, and 64-bit division remain pending; the divider needs an extra-width work register. Resource bounds and simulator limits still apply.

## Search: known and unknown solution counts

Oracle construction does not count solutions. `find_known(x,p,M)` requires the caller's promise `1 <= M <= N`; `find(x,p)` is its **M=1** shorthand. Both require an intact uniform input and pure predicate, transfer the amplified register, and consume the original name.

For a known count, `theta = asin(sqrt(M/N))` and `P(r)=sin²((2r+1)*theta)`. Iteration selection compares integers near `pi/(4*theta)-1/2` without checking the promise by enumeration. With N=16 and M=1, three iterations give probability `0.9613189697265625`. An incorrect promise has no valid success guarantee.

```c
quant uint8_t x = superposition(0, 15);
quant uint8_t answer = find_known(x, x * x == 9, 1);
printf("%u\n", (unsigned)measure(answer));
```

For an unknown count, including zero:

```c
quant uint8_t x = superposition(0, 15);
bool found = search(x, x * x == 9, 80);
if (found) printf("%u\n", (unsigned)measure(x));
else reset(x);
```

`search` mixes direct sampling and random Grover iterations, measures and verifies each candidate, and retains the last measured candidate. It explicitly measures/resets and reprepares between attempts. `true` always identifies a valid candidate; `false` means **budget exhausted**, not proof that no solution exists.

Budgets range from 1 to 4096 attempts. Under independent uniform randomness, failure with at least one solution is at most `(7/8)^attempts`. Reproducible PRNG sampling avoids modulo bias; the bound is not a per-seed guarantee. `search` needs CPU feedback and cannot be exported as flat QASM. Predicate synthesis and simulator limits remain independent constraints.

## Quantum IR, simulator, and export

The custom format is `cket.qir`, version 1. **It is not LLVM QIR**, and `--emit-llvm` is not implemented.

Instructions cover allocation/preparation, compute/uncompute/release, reversible updates, phases/Fourier, indexed CPU selection, measurement/reset, ownership moves, known/unknown-count search, borrowed module parameters, module application, and CPU control markers. Typed expressions preserve C widths and signedness.

The verifier rejects unknown/duplicate fields, invalid types/arities/conversions, false range metadata, unavailable registers, invalid controls, incompatible dependencies, and nonfinite angles. It structurally checks every branch while preserving short-circuit arithmetic evaluation. CPU markers balance but do not yet represent a complete CFG. See [IR verification](docs/ir-verification.md).

The native simulator stores `double complex` amplitudes in little-endian order. H combines amplitude pairs; X and controlled X permute them; phase multiplies selected amplitudes by `cos(theta)+I*sin(theta)`. Unitary operations preserve superposition and correlation without sampling. Measurement follows Born's rule, collapses and normalizes state. Reset applies an explicit collapse-and-correction trajectory. Sampling uses SplitMix64 with `--seed` or `CKET_SEED`.

There is no noise model, density-matrix engine, GPU backend, QPU connection, or concurrent quantum runtime. The runtime currently has a global context. Backend `--state` inspects simulator amplitudes while omitting document measurements; it is a debugging view, not a language operation for reading amplitudes without measurement.

OpenQASM 3 export includes registers, controlled gates, phases, measurements, and resets. Generated artifacts stay under ignored `build/`:

```bash
build/cket examples/find.c --emit-ast -o build/demo.ast
build/cket examples/find.c --emit-ir -o build/demo.qir.json
build/cket examples/find.c --emit-qasm -o build/demo.qasm
build/cket examples/find.c --resources -o build/demo.resources.txt
```

Resource estimates report logical/peak qubits, ancillas, gate classes, and dependency-based depth. They do not include device decomposition, connectivity, routing, or timing. Open controls count as logical operations; QASM adds X gates around positive controls. The demo reports 8 logical qubits, 1 ancilla, 9 peak qubits, and depth 26; its 24 logical X gates expand to 48 exported X gates.

Dynamic CPU control and missing runtime parameters are rejected for static QASM/resource export, even when native hybrid execution is supported.

## Current limits

| Aspect | Limit or restriction |
|---|---|
| Quantum integer width | 1–64 bits. |
| Gate representation | 128 total physical qubits. |
| Local simulation | 24 total physical qubits. |
| Finite expression synthesis | 12 relevant input bits; structured paths have separate limits. |
| General in-place permutations | 12-bit width. |
| Uniform preparation | Constant `[0, 2^k-1]` intervals. |
| Quantum owners | Automatic locals in `main`, including expanded function locals. |
| Functions | Borrows, owned scalar results, independent unitary QIR modules; general C ABI/results pending. |
| Arrays/structures | Fixed integer products; CPU-indexed scalar arrays and constant nested-product access. |
| Pointers | Checked classical-address borrows; dynamic quantum memory/addresses pending. |
| General coherent loops | Rejected. |
| General occupied-destination assignment | Pending. |
| General error budget, QPU, LLVM QIR | Pending. |

Accepting a type does not mean the simulator can execute it. A 32-bit `quant int` can export but already exceeds the 24-qubit simulation limit.

Statevector memory is `2^n * sizeof(double complex)`: at 16 bytes per amplitude, 24 qubits require **256 MiB for the vector alone**. Released zeroed qubits are reused; releasing a register does not shrink vector dimension.

Additional defensive bounds include 64 MiB IR files, 128 expression sources, and 2,097,152 export gate entries. The compiler does not prove all C undefined behavior absent, particularly after earlier GCC transformations, or claim arbitrary C programs gain quantum speedups. The [C11 coverage matrix](docs/c-coverage.md) records supported and pending constructs.

## Verification entirely in C

The native suite is C. Additional validation programs in `tests/` replace the previous Python tools:

```bash
sudo apt-get install -y curl ca-certificates
bash scripts/build-validation.sh
build/fuzz_frontend
build/benchmark_synthesis
build/validate_qasm
```

`build-validation.sh` downloads [QuEST 3.7.0](https://github.com/QuEST-Kit/QuEST/releases/tag/v3.7.0) at commit `d4f75f724993b4af8e43a796e3c09ce24ae11670`, checks SHA-256 `1a5939798a70b749dfa57cfaa116ece03a2d36aa59c1b712cb3dc2103ab1cded`, and builds its serial C backend. Downloaded sources and their upstream license stay under `build/validation-deps/`. QuEST is linked only into the independent validation program, not the compiler/runtime. No Python, pip, Qiskit, or Aer is required.

| Check | Scope |
|---|---|
| Native suite | Complete C programs, amplitudes, correlations, ownership, cleanup, IR validation, and rejection cases. |
| C frontend generator | 29 programs, seven input values each, five optimization levels: 145 compilations; default seed 20261011. |
| C synthesis growth | 24 circuits, widths 4/8/12/16, tables disabled, gate/ancilla/depth CSV. |
| Independent C QASM validation | 8 complete state/final-probability comparisons and 3 measurement/reset transcript comparisons with QuEST. |
| ASan/UBSan | Core tests or the full project compiler chain, including generated executables. |

The current native suite passes **354 tests with zero failures**, including the GCC logical XOR phase/cleanup regression added during the C validation migration.

The C frontend generator uses SplitMix64, so its generated corpus differs from the previous Python campaign even with the same numeric seed. It compares full expression values with independently evaluated GCC CPU references and preserves short circuits, explicit narrowing, signed conversions, and effects evaluated once. It also found a GCC-generated logical XOR node; the frontend now normalizes truth values before comparing them, with a phase/cleanup regression.

The independent QASM reader processes the exported artifact unchanged and uses QuEST for gate execution. It is a reader for this exporter's static subset, not a general OpenQASM parser. Eight malformed artifacts are rejected. State comparisons allow one common global phase and require maximum amplitude error `1e-10`; final measurement probabilities also check classical-bit mappings independently against QIR register order.

Three channel cases use **8192 transcripts each**, including entangled intermediate measurement, reset of entangled input, and measurement followed by reuse. A two-sample Hoeffding/union bound over all outcomes gives family-wise false rejection at most `1e-6` under independent sampling. This does not certify particular PRNG seeds. Dynamic exported CPU feedback, noise, and every reset channel are not covered.

The native suite includes 21 expressions over all 256 eight-bit inputs; addition/subtraction and inverses over 3720 basis pairs; eight predicates over 65,536 inputs; QFT/inverse against an independent DFT over 126 bases; 1024 seeded typed IR expressions over all their inputs and inverses; and 4096 IR mutations. These finite checks provide evidence rather than a formal proof for every accepted program.

```bash
bash scripts/test.sh
bash scripts/sanitize.sh
bash scripts/sanitize-all.sh
```

The additional C validation tools, native reference library, and QuEST backend can also be instrumented:

```bash
VALIDATION_SANITIZERS=1 bash scripts/build-validation.sh
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 build/fuzz_frontend-sanitized
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 build/benchmark_synthesis-sanitized
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 build/validate_qasm-sanitized
```

`sanitize.sh` instruments core tests and the engine. `sanitize-all.sh` also instruments the plugin, driver, backend, runtime, and generated executables under `build/sanitized/`. Host GCC itself is not instrumented; leak detection is disabled in its processes because GCC retains allocations until exit. Plugin memory/UB checks remain active. GCC scalar sanitizer checks on quantum data are replaced with domain validation where necessary; CPU checks remain.

The [first remote CI run](https://github.com/igna-s/cket/actions/runs/38150646854) passed 353 native tests, 353 full-chain sanitizer tests, 145 frontend compilations, 24 growth circuits, and 11 QASM comparisons using the former Qiskit/Aer tools. [Release notes](docs/releases/v0.4.0.md) and attached logs preserve that historical evidence. The [current workflow](.github/workflows/verify.yml) builds and runs the C replacements and archives logs, growth CSV, and failure reproductions.

## Commands and examples

| Driver option | Result |
|---|---|
| No export action | Compile and link a hybrid executable. |
| `--run` | Compile and execute the generated binary. |
| `--seed N` | Set the seed for execution through `--run`. |
| `--emit-ast` | Export the real GCC AST. |
| `--emit-ir` / `--emit-quantum-ir` | Export custom Quantum IR. |
| `--emit-qasm` | Export supported static programs as OpenQASM 3. |
| `--resources` | Report static logical circuit resources. |
| `--gcc compiler` | Select GCC. |
| `-o output` | Select output file/executable name. |
| `--version` / `--help` | Version/help. |

Additional GCC options are individual tokens, such as `-I/path` or `-DNAME=1`.

```bash
bash scripts/demo.sh
build/cket examples/functions.c --run --seed 7 -o build/function-demo
build/cket examples/vector_function.c --run -o build/vector-demo
build/cket examples/fourier.c --run -o build/fourier-demo
build/cket examples/phase_estimation.c --run -o build/phase-demo
build/cket examples/circuit_module.c --run -o build/circuit-module-demo
build/cket examples/dynamic_vector.c -o build/dynamic-vector-demo
printf '1\n' | build/dynamic-vector-demo
```

The dynamic vector example prints `9` for index 1 and rejects out-of-range input before allocating the vector. Other examples cover preparation, types, products, coherent branches, and hybrid loops.

```bash
build/cket_backend --validate build/demo.qir.json
build/cket_backend --emit-qasm build/demo.qir.json
build/cket_backend --resources build/demo.qir.json --no-tables
build/cket_backend --state build/demo.qir.json
```

`--no-tables` rejects predicates requiring finite synthesis rather than changing the computation. `--state` is the simulator debugging view described above.

## Version 1.0 work remaining

The [closed 1.0 specification](docs/v1-plan.md) requires scalable arithmetic and bounded symbolic complexity; known/unknown-count search contracts; full accepted-path ownership analysis; useful function composition/ABI; stronger GCC differential evidence; broader independent validation; and static/parameterized plans avoiding first-call synthesis.

Structured arithmetic, census-free search, conservative CPU joins, owned scalar returns, circuit modules, caches, and reproducible CI are implemented advances. Larger arithmetic composition, 64-bit division, complete CFG analysis, a general C quantum ABI, wider validation campaigns, and general ahead-of-time compilation remain pending. QPU support, dynamic memory, all of C11, and general coherent loops are outside the first release's requirements.

## Documentation and license

- [Language types, ownership, and effects](docs/language.md).
- [C11 coverage](docs/c-coverage.md).
- [IR verification](docs/ir-verification.md).
- [Circuit ABI](docs/circuit-abi.md).
- [Version 1.0 acceptance](docs/v1-plan.md).
- [Contributing](CONTRIBUTING.md) and [security reports](SECURITY.md).

Project code is licensed under [MIT](LICENSE). External dependencies retain their own licenses. Repository access remains private; adding a license does not change visibility.

Technical references: [GCC plugin API](https://gcc.gnu.org/onlinedocs/gccint/Plugin-API.html), [OpenQASM 3 controls and global phase](https://openqasm.com/versions/3.0/language/gates.html), and [QuEST's pinned C implementation](https://github.com/QuEST-Kit/QuEST/tree/d4f75f724993b4af8e43a796e3c09ce24ae11670).
