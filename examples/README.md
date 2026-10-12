# C Ket examples

The collection contains 36 C programs. The runner executes the 35 programs that fit the native
statevector simulator; [types.c](types.c) intentionally demonstrates an exportable 32-bit register
that exceeds its 24-qubit limit.

Run these commands from the repository root in Linux or WSL. See the
[installation instructions](../README.md) for GCC and plugin dependencies.

```bash
bash scripts/build.sh
bash scripts/run-examples.sh --all
```

List the runnable programs, select a few, or change the measurement seed:

```bash
bash scripts/run-examples.sh --list
bash scripts/run-examples.sh for_sum while_sum fibonacci maximum_scan
CKET_SEED=12 bash scripts/run-examples.sh measurement_histogram find_multiple
```

Executables are written to `build/examples/`. The runner uses seed 7 by default, supplies the sample
inputs below, and stops if compilation or execution fails. Run the compiled executable directly to
provide your own input.

## Loops and arithmetic

| Program                                        | What it teaches                                                    | Expected stdout                   | Static QASM |
| ---------------------------------------------- | ------------------------------------------------------------------ | --------------------------------- | ----------- |
| [for_sum.c](for_sum.c)                         | CPU `for`, quantum accumulation of 1 through 10.                   | `sum(1..10)=55`                   | No          |
| [while_sum.c](while_sum.c)                     | CPU `while`, counter and final measurement.                        | `while sum=55 iterations=10`      | No          |
| [do_while_countdown.c](do_while_countdown.c)   | Five subtraction steps with `do/while`.                            | `remaining=5 iterations=5`        | No          |
| [nested_for_sum.c](nested_for_sum.c)           | Nested row/column loops sum a 3-by-4 matrix.                       | `matrix sum=78`                   | No          |
| [register_accumulate.c](register_accumulate.c) | Repeated register addition and its inverse, preserving the source. | `sum=15 restored=0 term=3`        | No          |
| [fibonacci.c](fibonacci.c)                     | Addition and a reversible XOR swap in two registers.               | `F(8)=21 F(9)=34`                 | No          |
| [modular_arithmetic.c](modular_arithmetic.c)   | Eight-bit unsigned addition, subtraction and XOR.                  | `add=14 subtract=240 xor=255`     | Yes         |
| [dot_product.c](dot_product.c)                 | CPU vector products accumulated in a quantum register.             | `dot product=70`                  | No          |
| [input_sum.c](input_sum.c)                     | Input validation and a loop with a runtime bound.                  | For input `10`: `count=10 sum=55` | No          |

The loop conditions above are classical C. No implicit measurement decides when to stop. Quantum
storage is declared before dynamic loops and is explicitly measured or reset afterwards, including
when a loop executes zero times.

Unsigned `uint8_t` updates operate modulo 256. The Fibonacci example stops at F(9)=34, and
`input_sum.c` accepts only counts 0 through 16 so its largest sum is 136. Larger counts require
deliberate handling of width and wraparound. In matrix sums and dot products, full CPU intermediate
values are computed before updating the narrow quantum accumulator.

## Minima and maxima

| Program                          | What it teaches                                                                                       | Expected stdout                                           | Static QASM |
| -------------------------------- | ----------------------------------------------------------------------------------------------------- | --------------------------------------------------------- | ----------- |
| [pair_minimum.c](pair_minimum.c) | Compute a reversible minimum into a third register while preserving both inputs.                      | `min(19,7)=7`                                             | Yes         |
| [pair_maximum.c](pair_maximum.c) | Compute a reversible maximum into a third register.                                                   | `max(19,7)=19`                                            | Yes         |
| [minimum_scan.c](minimum_scan.c) | Evaluate `f(x)=min(x,15-x)` for all 16 candidates and keep the smallest measured score.               | `minimum f(x)=min(x,15-x): value=0 x=0`                   | No          |
| [maximum_scan.c](maximum_scan.c) | The corresponding exhaustive maximum, including a tie.                                                | `maximum f(x)=min(x,15-x): value=7 x=7`                   | No          |
| [minmax_array.c](minmax_array.c) | Load, measure and reuse one register for eight dataset values; record both extrema and their indices. | `min=0 index=3 max=255 index=5`                           | No          |
| [input_minmax.c](input_minmax.c) | The same reduction with validated user input.                                                         | For the runner's dataset: `min=0 index=3 max=255 index=5` | No          |

The pair examples perform the comparison and selection on quantum registers. The exhaustive examples
evaluate the score in quantum storage, then measure it and perform the minimum/maximum reduction on
the CPU. Their fixed `for` loops expand so each temporary has a checked lifetime; explicit resets
clean storage for reuse. The dataset examples prepare basis values and measure them; their
comparison and index tracking are classical. These examples demonstrate hybrid programming and do
not claim a quantum speedup or implement quantum minimum finding.

All scans use zero-based indices and keep the first occurrence of an extremum. For the score
function, the minimum occurs at x=0 and x=15, and the maximum at x=7 and x=8. Strict comparison
retains x=0 and x=7 respectively.

## Measurement, feedback and search

| Program                                            | What it teaches                                                                      | Output contract                                                   | Static QASM |
| -------------------------------------------------- | ------------------------------------------------------------------------------------ | ----------------------------------------------------------------- | ----------- |
| [measurement_histogram.c](measurement_histogram.c) | Repeated preparation, Fourier transform and measurement of one bit inside a `while`. | `zeros=Z ones=O trials=32`, where Z+O=32.                         | No          |
| [repeat_until_success.c](repeat_until_success.c)   | Measurement feeds a CPU retry condition, with at most 16 attempts.                   | `found=F attempts=A`; F is 0 or 1, 1\<=A\<=16; F=0 requires A=16. | No          |
| [find_multiple.c](find_multiple.c)                 | Grover amplification with the known count of four solutions.                         | `candidate=V`, V in {1,5,9,13}.                                   | Yes         |
| [search_constraints.c](search_constraints.c)       | Verified unknown-count search with interval and remainder constraints.               | A candidate 10 or 13, or `Search budget exhausted`.               | No          |
| [coherent_accumulate.c](coherent_accumulate.c)     | A quantum `if/else` inside a fixed loop, without measuring the controlling flag.     | `flag=0 total=30` or `flag=1 total=15`.                           | Yes         |

Do not expect the histogram to split evenly on every run. Retry exhaustion is a valid outcome.
`find_known` takes its solution count as a caller promise; this example has exactly four matches in
0 through 15. `search` verifies a candidate before returning true, while false reports exhaustion of
its 80-attempt budget and does not prove that the domain has no solutions.

The coherent example measures both registers after the loop to demonstrate their correlation.
Classical I/O, CPU mutation and measurement stay outside quantum branches.

## Interactive inputs

`input_sum` reads one count in 0 through 16. Zero is accepted:

```bash
build/cket examples/input_sum.c -o build/input_sum
printf '4\n' | build/input_sum
# count=4 sum=10
printf '0\n' | build/input_sum
# count=0 sum=0
```

`input_minmax` reads a count in 1 through 8, followed by that many integers in 0 through 255. Values
may repeat:

```bash
build/cket examples/input_minmax.c -o build/input_minmax
printf '4 9 2 9 7\n' | build/input_minmax
# min=2 index=1 max=9 index=0
printf '1 255\n' | build/input_minmax
# min=255 index=0 max=255 index=0
```

Both programs report invalid counts, malformed values, missing input or values outside their range
on stderr and exit with status 1. They validate input before allocating quantum storage.
[read_input.h](read_input.h) reads decimal digits, accepts leading zeros and rejects signs, trailing
nondecimal characters and arbitrarily large numbers without integer wraparound. The runner supplies
`10` to `input_sum` and `8 42 17 42 0 128 255 9 255` to `input_minmax`. It supplies index `0` to the
existing [dynamic_vector.c](dynamic_vector.c).

## Existing language demonstrations

Start with [hello.c](hello.c), then explore [hybrid_loop.c](hybrid_loop.c),
[functions.c](functions.c), [products.c](products.c), [vector_function.c](vector_function.c), and
[dynamic_vector.c](dynamic_vector.c) for composition, products and indexing.

For quantum behavior, see [entanglement.c](entanglement.c), [prepare_measure.c](prepare_measure.c),
[coherent.c](coherent.c), [fourier.c](fourier.c), [phase_estimation.c](phase_estimation.c), and
[circuit_module.c](circuit_module.c). [find.c](find.c), [search.c](search.c), and
[search_empty.c](search_empty.c) cover single-solution search and budget exhaustion.

The export-only type demonstration can be inspected without executing it:

```bash
build/cket examples/types.c --emit-qasm -o build/types.qasm
build/cket examples/types.c --resources -o build/types.resources.txt
```

## Validation

```bash
bash scripts/test.sh
```

The example suite compiles every C program at `-O0`, `-O1`, `-O2`, `-O3` and `-Os`, checks outputs
and exit status, validates emitted IR, and checks static export support or rejection. Interactive
boundary cases run at all five levels: zero/maximal counts, singleton datasets, repeated extrema,
ordered values, out-of-range and oversized values, malformed input and EOF. The stochastic additions
run over 16 seeds with semantic checks; the coherent example must demonstrate both correlated
branches in that corpus.

See the [language contract](../docs/language.md) for ownership, cleanup, arithmetic and control-flow
limits.
