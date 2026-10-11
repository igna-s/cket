# Types, ownership, and effects in `|c>`

## Real C and quantum qualification

GCC processes C11, including the preprocessor, headers, typedefs, and arithmetic promotions. The plugin registers `cket_quant` and the intrinsics, then analyzes trees at `PLUGIN_PRE_GENERICIZE`. `quant`, `cuant`, and `Cuant` are macros in `quantum.h` for that recognized attribute. They are not reserved words built into GCC's grammar yet.

```text
SemanticType = { GCC_C_Type, quantum, integer_width, signedness }
```

`int` and `quant int` retain the same C width and encoding but have different semantics. Qualification lives in the plugin's analysis and the Quantum IR type table. The attribute is not a native GCC type qualifier. A future frontend change must propagate qualification through compound types, arguments, results, typedefs, and the ABI.

C integers and Booleans up to 64 bits, fixed multidimensional arrays, and integer structures are supported. Functions compose over GCC's AST: scalar, pointer, and array parameters borrow registers; results may be pure conditional expressions or owned locals with a terminal return; procedures may create owned temporaries. Definitions must precede calls in the same translation unit. Owners belong to `main`, including variables from expanded procedures. Global storage, VLAs, unions, bitfields, quantum floating point, and a general quantum ABI remain pending. An internal `volatile` marker preserves quantum accesses through simplifications such as `x*0`.

`quant uint8_t *p = &x` is a reference with a classical address to the existing register. Dereferences and constant offsets resolve within checked bounds. Local aliases are supported, but escape into CPU memory and dynamic reassignment are not. Mutable parameters must not overlap; a read-only borrow must not be forwarded to a mutator. Borrows cannot be consumed through moves or `find`, including through aliases. Procedures support a final `return;`; early returns need further analysis.

Fixed scalar arrays and their borrows support `a[i]` with a CPU index for unsigned addition/subtraction and XOR with CPU operands, increments, `measure`, `reset`, and Fourier/inverse Fourier. The index is evaluated once and checked against bounds; selection does not measure the register. These operations may run in CPU loops without allocating new registers. Measuring or resetting an unknown element does not prove that every owner was consumed or cleaned. Coherent access with a quantum index and general quantum expressions over selected elements remain pending.

CPU initialization, such as `quant uint8_t x = read_value();`, evaluates the value once, preserves its C conversion, and prepares a new basis state. It does not read another register without explicit measurement. Products may also have CPU initializers. Dynamic preparation requires native execution and cannot be flattened into static QASM.

## New values and assignment

```c
quant uint8_t x = superposition(0, 15);
quant uint8_t y = x % 2;
```

This allocates zeroed storage and produces a reversible operation:

```text
|x>|0> -> |x>|x % 2>
```

It preserves information and may entangle registers; it does not clone an arbitrary state. Operators serialize into an IR expression with their GCC types. For example, `x*x` with `uint8_t` x preserves C's promotion to `int` rather than silently comparing an 8-bit truncated product. Only conversion to a `uint8_t` result truncates to that width.

Direct initialization `quant uint8_t b = a` with a compatible type is an **ownership transfer**: the names do not simultaneously access two states. `a` is consumed and later uses produce an error. Moves and `find` results require the same canonical C type: compatible typedefs are accepted, but equal width and signedness do not suffice for distinct C types. A width or signedness change computes a value conversion and preserves the input.

Unsigned updates `x += k` and `x -= k` use reversible modular addition. Addition/subtraction between distinct unsigned registers preserves the source and uses direct synthesis without tables or ancillas. `x ^= y` and XOR with supported independent operands preserve information; CPU operands are evaluated once. Other updates are synthesized as permutations only when injective over the entire bit space. `x = x % 2`, destructive shifts, and overwrites of an unknown register are rejected. Updating a derived result requires another new result.

An effectful CPU operand, such as `x += measure(y)` or `x += ++n`, is evaluated once before updating x. The plugin preserves GCC's shared `SAVE_EXPR` and emits explicit measurement before addition in Quantum IR. These CPU effects are rejected under coherent quantum control.

## Control flow and effects

A quantum `if/else` condition is computed in an ancilla, coherently controls both branches, and is uncomputed. Measurement does not select a branch. Branches accept supported unitary operations and `phase_flip()` and reject I/O, classical calls, CPU writes, reset, measurement, and classical jumps. Nested conditions accumulate controls.

Changing a condition input may make its inverse impossible. The compiler rejects this case unless it has a proven reversible transformation. An independent flag may control an update to another register.

In particular, `if (x == 0) x += 1;` maps both zero and one to one. It is non-injective over x, beyond a conservative analysis restriction. Retaining history distinguishes the inputs but does not ensure that history can be cleaned while preserving the intended result.

Classical `for/while/do` loops retain GCC's actual CPU control. Unsigned updates may receive CPU operands each iteration. Supported indexed operations may also measure/reset existing storage while preserving lifetime obligations for all owners. Quantum declarations within dynamic CPU flow still need further analysis. Scalar measurements/resets and coherent `if` with stable owners are supported; joins retain only facts true on all paths, including a loop's zero-iteration path. Loops with quantum conditions need a finite bound and a reversible history representation; general loops without that proof are not supported.

A `for` with a local counter, constant bounds, and `++`/`--` may expand up to 4096 iterations to support products, temporaries, or coherent control. Bounds and overflow are checked; jumps and counter changes in the body are rejected. Other CPU loops retain their execution, including nontermination when a counter wraps to zero. Quantum IR marks dynamic CPU control and rejects flattening it into QASM.

## Phases and the Fourier transform

`phase(theta)` applies a finite classical angle, constant or CPU-computed, to active controls. Outside control it is a global phase. Effectful CPU operands are rejected under coherent control. A dynamic angle requires native execution.

`fourier(x)` implements `|x> -> sum(k, exp(2*pi*i*x*k/2^w)|k>)/sqrt(2^w)` over the entire width-w register in little-endian order. `inverse_fourier(x)` reverses the exponent's sign. Both support coherent control, preserve correlations, and use quadratic synthesis without tables. A basis transformation invalidates value and uniformity proofs and is rejected if it prevents a pending inverse.

Fourier and phase estimation examples compare amplitudes against an independent mathematical DFT. Tests use double precision and numerical tolerance `1e-10`; no general error budget for every algorithm is claimed yet.

## Uncomputation and measurement

Each computed value records its creating expression, normalized external controls, and input/control versions. After its last use, the compiler applies the inverse and releases the register only when proven zero. Cleanup follows reverse dependency order. The backend requires the same expression, control mask/pattern, and available dependencies with matching versions; it rejects modifying, measuring, resetting, or releasing dependencies needed for pending cleanup.

A diagonal phase can preserve this proof because it does not change the basis values used by the computation. Its relative phases survive the inverse. Uniformity for `find` is invalidated where appropriate. The current certificate compares the expression's JSON text and may reject equivalent serializations.

```c
quant bool flag = (x + 5 == 42);
if (flag) phase_flip();
// flag and the control ancilla automatically return to zero.
```

Current finite synthesis directly composes the reversible Boolean oracle from the IR expression; it need not store each arithmetic node separately. Existing temporary registers are explicitly cleaned in IR with `quncompute` and `qrelease`.

`measure(x)` is the only conversion to a CPU value. It explicitly collapses the register and preserves the resulting state for later operations/measurements. `reset(x)` permits explicit destruction of information and leaves zeroed storage. No measurements or resets are inserted to facilitate coherent control or cleanup. Constants, preparation, and unitary gates never overwrite an unknown state through nonunitary initialization.

Before exit, every register must be explicitly measured/reset, transferred to another owner, or uncomputed and released. Returns with quantum lifetimes that cannot be proven safe are rejected. The C runtime immediately executes each operation, including cleanup, in its linked engine; it frees simulator memory on exit.

## Native C engine

The statevector contains `double complex` amplitudes in little-endian order. H maps a pair `(a,b)` to `((a+b)/sqrt(2),(a-b)/sqrt(2))`; X and its controls permute amplitudes; a phase multiplies selected amplitudes by `cos(theta)+I*sin(theta)`. Unitary operations preserve superposition without sampling.

Only `measure` samples according to Born's rule, removes incompatible amplitudes, and normalizes the remaining state. Partial measurement preserves correlations with other registers. Explicit `reset` applies the reset channel through a collapse-and-correction trajectory, as the program requested information destruction. Releasing workspace also verifies that its probability outside zero is below the numerical tolerance.

The driver `src/cket.c`, runtime, Quantum IR parser, synthesis, simulator, and test programs are C; the AST plugin is C++ because of GCC's API. No Python or quantum SDK is required by the compiler or runtime. Optional independent validation builds QuEST's C backend. `backend/cket_backend.c` provides validation, QASM, resources, and `--state` amplitude inspection before document measurements, solely for simulator debugging. Inspection does not change compiled program execution.

## Synthesis and experimental find

The backend attempts symbolic synthesis for bit operators, conversions, constant shifts, and supported predicates against constants. It does not enumerate the domain and limits each bit to 256 terms within a global budget of two million steps. Larger arithmetic uses register circuits; the optional finite path uses up to 12 relevant bits through algebraic normal form or minterms. `--no-tables` disables it. General permutations retain their 12-bit limit. `known` and `active` metadata require an independent engine-maintained proof. Enumeration constructs a circuit rather than replacing interference with precomputed answers.

The expression must be defined over the synthesized domain: division by zero, signed overflow, and invalid shifts detected in typed IR are rejected. Absence of all undefined behavior previously simplified by GCC or in arbitrary C is not proven. Pure `&&`, `||`, and `?:` preserve conditional evaluation when synthesizing their function.

`find(x, predicate)` requires an intact uniform x and promises one solution. `find_known(x,p,M)` receives a known count as a caller promise without checking it through enumeration. Both transfer the amplified register and consume the original name.

`search(x,p,attempts)` supports an unknown count, including zero. It is explicitly nonunitary: it measures candidates, verifies p, and repeats preparation/Grover within a constant budget from 1 to 4096. It returns a CPU bool and retains x with the last measured candidate. True implies a valid candidate; false means budget exhaustion rather than proving nonexistence. Under independent uniform randomness, if a solution exists, failure is at most `(7/8)^attempts`. A reproducible PRNG avoids modulo bias; this is not a guarantee for each seed. CPU feedback prevents flat QASM export.

The oracle ancilla ends in zero. Find results are probabilistic even without noise. Resource reports include logical qubits, ancillas, physical qubits, gates, and depth before device decomposition. One- and two-input controls count as CNOT and Toffoli; larger controls count as MCX without inventing a decomposition. Open controls are single IR operations; their QASM expansion adds opening/closing X gates and may increase exported counts and depth.

## Verification and development toward 1.0

The verifier checks schema, types, arities, controls, conversions, constants, and finite angles before synthesizing each operation. It structurally checks every expression branch while preserving short-circuit evaluation of arithmetic. CPU markers must balance but do not yet form a complete control-flow graph.

The [IR contract](ir-verification.md) describes guarantees and limits. Functions, fixed products, borrows, phases, QFT, and a phase estimation example already exist. The [C11 coverage matrix](c-coverage.md) lists remaining work toward extending C. The [1.0 plan](v1-plan.md) retains further ABI, synthesis, flow, and precision criteria.

## References

- [GCC: plugin API and PLUGIN_PRE_GENERICIZE](https://gcc.gnu.org/onlinedocs/gccint/Plugin-API.html).
- [OpenQASM 3: controls and global phase](https://openqasm.com/versions/3.0/language/gates.html).

## Arithmetic, functions, and execution updates

Register circuits cover multiplication, division/remainder with a proven nonzero divisor, and general comparisons, using compute/XOR-copy/inverse to clean workspace. Negative operands preserve truncation toward zero and the remainder's sign; absence of overflow is proven and MIN/-1 is rejected. Ranges compress sign extensions without changing the value. A 64-bit divider needs workspace not yet supported. Small table paths remain available; `cket_backend ... --no-tables` disables them. Language width (64 bits), gate storage (128 physical qubits), and simulation (24) are distinct limits.

A function with a terminal owned scalar return may transfer its local into a new `quant` initializer. Pure return branches become a conditional expression; early returns with pending lifetimes and returned products remain unsupported. Independent unitary QIR modules compose through `circuit` and `inverse_circuit`; [circuit-abi.md](circuit-abi.md) defines their contract.

The runtime retains parsed documents and plans for expressions, sums, and modules. Availability and versions are checked each call, and only free private workspace is reused. The first call may still synthesize; general ahead-of-time compilation is not claimed. The [1.0 contract](v1-plan.md) retains the open requirements.
