# C11 coverage and `quant` data

The final goal of `|c>` is explicit quantum compatibility for all C11 constructions and
standard-library interfaces. The implementation does not meet that complete goal yet.
[Version 1.0](v1-plan.md) is an intermediate milestone. The [complete inventory](c11-inventory.md)
and [roadmap](c11-roadmap.md) classify missing implementations, resource bounds and incompatible
contexts. GCC parses C; the following table describes the current intermediate subset. Recognizing a
construct does not imply implementing its quantum semantics.

Information-preserving operations may be synthesized coherently. Reading into CPU data requires
explicit measurement. A write that loses information must preserve it with defined ownership and
lifetime, or use an explicit program reset. Rejecting unimplemented cases protects these contracts
but does not count as functional support.

| C11 construct                                         | Status for quantum data                                        | Contract or remaining work                                                                                                                                                   |
| ----------------------------------------------------- | -------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Preprocessor, includes, macros                        | Real GCC                                                       | `quant` is a registered attribute, not yet a native GCC qualifier.                                                                                                           |
| Integers and `_Bool`                                  | Implemented up to 64 bits                                      | Width, signedness, promotions, and conversions are preserved in IR.                                                                                                          |
| `float`, `double`, complex types                      | Pending                                                        | Encoding, rounding, exceptions, and reversible circuits. CPU angles do use `double`.                                                                                         |
| `typedef`                                             | Compatible scalar aliases                                      | Native propagation of `quant` as part of the type remains pending.                                                                                                           |
| `const`                                               | Read-only borrowed parameters                                  | Constant owners and general borrowing through the full ABI remain pending.                                                                                                   |
| `volatile`, `_Atomic`, threads                        | Classical C; general quantum integration pending               | Internal `volatile` prevents early simplifications. The quantum runtime is global.                                                                                           |
| Automatic locals                                      | Implemented                                                    | Owners in `main` and expanded procedures.                                                                                                                                    |
| Globals, `static`, `extern`, TLS storage              | Pending for quantum owners                                     | Initialization, synchronization, ownership, and cleanup across calls.                                                                                                        |
| Constant initialization                               | Implemented                                                    | Prepares a basis state. A variable without an initializer starts in zero.                                                                                                    |
| CPU initialization                                    | Implemented                                                    | Evaluates once and prepares new storage; a dynamic value requires native execution.                                                                                          |
| Quantum expression initialization                     | Implemented for supported expressions                          | Preserves inputs, records an inverse, and permits entanglement.                                                                                                              |
| Value-producing assignments, increments and comma     | Partial scalar implementation                                  | Old/new quantum results with retained history; lazy expression effects and computed measurement operands remain pending. [Contract](c11-effect-values.md).                   |
| Fixed and multidimensional arrays                     | Implemented for integers/Booleans                              | Ownership and cleanup per element; defensively bounded expansion.                                                                                                            |
| Structures and nested structures                      | Implemented for integer products                               | No internal pointers, bitfields, unions, or anonymous quantum fields.                                                                                                        |
| Designated initializers                               | Implemented for fixed products                                 | Omitted elements start in zero.                                                                                                                                              |
| Whole array/structure copies                          | Pending                                                        | Cannot inherit an independent copy of an unknown state.                                                                                                                      |
| VLAs and dynamic arrays                               | Pending                                                        | Runtime lifetimes, bounds, and register allocation.                                                                                                                          |
| Constant indices                                      | Implemented                                                    | Storage bounds are checked.                                                                                                                                                  |
| Static loop indices                                   | Implemented                                                    | Loops expand when their bound and counter are checked.                                                                                                                       |
| Dynamic CPU indices                                   | Indexed operations on scalar arrays and borrows                | Unsigned addition/subtraction/XOR and increments, measurement/reset, Fourier/inverse; bounds checked. General quantum reads and nested products remain pending.              |
| Quantum indices                                       | Pending                                                        | Coherent read/write through reversible selection; no implicit CPU read.                                                                                                      |
| `&`, `*`, pointer access                              | Checked borrows                                                | `quant T *p` refers to existing registers with a classical address. Reinterpreting element types requires a byte-access contract and is rejected.                            |
| Pointer arithmetic                                    | Constant offsets and CPU-indexed selection                     | Moving back inside an array and representing a one-past-end pointer are supported; indexed accesses check bounds at runtime. General reference reassignment remains pending. |
| Reference aliases                                     | Implemented in static scope                                    | Aliases refer to the same register; use after move is rejected.                                                                                                              |
| Pointer reassignment/escape                           | Pending                                                        | Current references cannot escape into CPU data or be dynamically reassigned.                                                                                                 |
| `malloc`, `free`, `realloc`, dynamic quantum memory   | Pending                                                        | Storage manager, address-based ownership, and cleanup before release.                                                                                                        |
| Direct functions and procedures                       | AST composition                                                | Definition available before use in the same unit; fresh locals per call.                                                                                                     |
| Scalar, pointer, and array parameters                 | Borrows implemented                                            | Same canonical C type; borrows cannot be consumed and mutable parameters cannot overlap.                                                                                     |
| Quantum return                                        | Pure conditional expressions and terminal owned scalar results | A local may transfer to a new initializer; directly returning a borrowed parameter is rejected.                                                                              |
| General and multiple returns                          | Partial                                                        | Terminal owned returns and pure branches; returned products and early exits with pending lifetimes need further analysis.                                                    |
| Recursion, indirect calls, quantum variadic functions | Pending                                                        | ABI, lifetime/effect proofs, and bounds.                                                                                                                                     |
| Multiple translation units and quantum libraries      | Independent QIR circuit modules                                | `circuit` and its inverse embed typed unitary modules; linking C `quant` functions across units remains pending.                                                             |
| `+ - * / %` in a new result                           | Register circuits and small finite path                        | Positive/negative operands with proven ranges; zero divisors, signed overflow, and MIN/-1 rejected. General composition and 64-bit division remain pending.                  |
| In-place modular addition/subtraction                 | Direct synthesis                                               | CPU operands or distinct unsigned registers; preserves the other register.                                                                                                   |
| `& \| ^ ~` and constant shifts                        | Symbolic synthesis and finite path                             | Term budgets; checked shifts and conversions.                                                                                                                                |
| `^=`                                                  | Reversible accumulation                                        | Operand independent of destination; CPU argument evaluated once. Restricted Boolean conversions.                                                                             |
| `*= /= %= &= \|= <<= >>=`                             | Injective cases in finite path                                 | General destructive transformations require preserving history.                                                                                                              |
| `++ --`                                               | Supported update contexts                                      | Values of effectful quantum expressions in arbitrary contexts are not implemented yet.                                                                                       |
| Comparisons against constants                         | Structured or finite path                                      | 16-bit predicates tested exhaustively; some 32-bit cases exportable.                                                                                                         |
| Register comparisons                                  | Structured circuits                                            | Lexicographic comparison with reversible difference and sign bias; workspace limited.                                                                                        |
| Pure `! && \|\| ?:`                                   | Implemented                                                    | Preserves short-circuit arithmetic evaluation; CPU effects do not become coherent effects.                                                                                   |
| Direct scalar assignment to a new owner               | Move                                                           | Transfers ownership, consumes the previous name, and does not allocate another copy.                                                                                         |
| Assignment to occupied storage                        | Reversible cases; general case pending                         | Explicit history preservation and destination rules.                                                                                                                         |
| Quantum `if/else`                                     | Coherent control                                               | Computed predicate, control of both branches, and inverse; inputs needed for cleanup must not change.                                                                        |
| CPU `if/else`                                         | Scalar versions and ownership transport                        | Nested joins preserve selected wires and saved inverse dependencies. Controlled declarations/products and general CFG remain pending.                                        |
| Static `for`                                          | Bounded expansion                                              | Up to 4096 iterations, without jumps or counter changes in the body.                                                                                                         |
| Dynamic CPU `for/while/do`                            | Updates, coherent `if`, and measurements on stable owners      | Not flattened into QASM. Measuring/resetting a selected element does not prove the loop consumed every owner; dynamic allocation remains pending.                            |
| Quantum-condition loops                               | Pending                                                        | Bounds, reversible progress, and history.                                                                                                                                    |
| `switch`, `break`, `continue`, `goto`, labels         | Classical C; general quantum composition pending               | Control-flow graph, lifetime, and effect analysis required.                                                                                                                  |
| `sizeof`, `_Alignof`, classical C layout              | Real GCC                                                       | Do not read amplitudes. C layout is not a CPU representation of quantum state.                                                                                               |
| `read`, `scanf`, files, sockets                       | Classical CPU                                                  | Results may prepare new registers; quantum state must be measured before passing to CPU.                                                                                     |
| `write`, `printf`, files, sockets                     | CPU after explicit measurement                                 | Passing unmeasured registers or performing I/O under coherent control is rejected.                                                                                           |
| `memcpy`, `memmove`, `memset` on registers            | Pending                                                        | Byte access, ownership, and reversibility; not simulated as CPU copies of markers.                                                                                           |
| C undefined behavior                                  | Diagnostics over the synthesized domain                        | No complete absence-of-UB proof for arbitrary C or all earlier GCC transformations.                                                                                          |

## Additional quantum transformations

`phase(theta)` accepts finite classical angles, constant or runtime-computed. `fourier` and
`inverse_fourier` transform an entire register, with or without coherent controls, using quadratic
synthesis. Neither enumerates the domain.

The examples `functions.c`, `vector_function.c`, `dynamic_vector.c`, `fourier.c`, and
`phase_estimation.c` exercise composition, borrows, CPU-indexed access, and interference algorithms.
`find_known` receives a count as a promise; `search` verifies candidates within bounded attempts,
including when no solutions exist. `find` promises one solution. None counts the domain beforehand;
predicates remain subject to synthesis limits.

## Evidence and progress criteria

The suite checks complete C programs, complex amplitudes, correlations, dependency versions,
cleanup, and invalid IR. Exhaustive tests enumerate specific domains: 256 inputs for eight-bit
arithmetic, 3720 adder pairs, 65,536 predicate inputs, and QFT bases from one to six bits. It also
includes 1024 seeded typed expressions compared with C over every input and inverted, plus 4096 IR
mutations exercising parsing and validation. This does not cover all of C11 or constitute a complete
fuzzing campaign.

`scripts/sanitize.sh` instruments tests and the engine. `scripts/sanitize-all.sh` also builds an
instrumented plugin, driver, backend, runtime, and generated executables in `build/sanitized`. Host
GCC is not instrumented; leak detection is disabled in its processes to avoid attributing
GCC-retained allocations to the plugin. ASan and UBSan remain active in the plugin; scalar checks
inserted by GCC before the quantum pass are replaced with quantum domain validation where
appropriate. CPU code checks remain.

Completing the language requires implementations and contract tests for the pending rows, alongside
broader synthesis. This table records that work without claiming complete C coverage.

Additional evidence includes amplitudes and inverses for eleven operators without tables, negative
operands and C rounding, 24 growth circuits up to 32 relevant bits, generated frontend tests at five
optimization levels, and eleven exported QASM circuits checked against an independent simulator.
Current validation tools are C and use QuEST; three cases compare full intermediate
measurement/reset transcripts. The README describes scripts and evidence. See
[circuit-abi.md](circuit-abi.md) for the circuit ABI.
