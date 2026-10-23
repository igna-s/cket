# Fixed arrays with quantum indices

The frontend now implements partial coherent reads and writes for fixed scalar arrays with an
unmeasured integer index. The source is checked quantum storage with 1–64 integer or Boolean
elements. The index is a pure integer expression of at most 64 bits. Signed negative and
out-of-range indices fail the circuit domain check; narrowing the private selector follows that
check.

```c
quant bool index = superposition(0, 1);
quant uint8_t values[2] = {3, 9};
quant uint8_t before = values[index];
quant uint8_t assigned = (values[index] = 7);
```

The bounds-checked selector is captured once in private zero storage. A finite multiplexer selects
the corresponding register value. Writes capture the assigned value before updating any candidate,
then use retained scalar histories. Simple and compound assignments, prefix/postfix increments and
decrements produce their C expression values. Existing promotions and signed-overflow definedness
apply. A snapshot preserves the original index if a store changes an input on which it depended.

The implementation uses the existing typed reversible synthesis paths. It performs no implicit
measurement, reset or CPU inspection of a quantum placeholder. Private allocations have
unconditional lifetimes; coherent controls select their computations and writes. This is a finite
circuit over the array registers, without a physical quantum-RAM or speedup claim. Element,
selector, history, gate and simulator budgets are separate costs.

CPU-indexed measurement/reset, Fourier and the existing restricted updates retain their previous
path. Quantum-index pointer-byte arithmetic still needs a proof before pointer-size wrapping and
remains pending. Nested products, dynamic allocation/counts, address escape, general mutable pointer
ABI, effectful indices and further loop/lifetime combinations also remain pending. Pure private
value programs can compose as indices or assignment values; CPU side effects require sequencing.

[`quantum_index.c`](../examples/blocks/04/quantum_index.c) provides a source example of reads,
assignment values and postfix updates. `bash scripts/run-block.sh 04` supplies its eventual native
and export command. **This version has compilation evidence only:** execution, full joint states,
inverses, GCC configurations, sanitizers and broad regression are deferred at the user's request.
The example does not yet qualify those behaviors or complete block 04/C11.
