# Fixed arrays and borrowed buffers with quantum indices

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
path. `measure(values[index])` with a quantum index can explicitly observe the computed selection;
it does not measure or consume every candidate owner. Reset/Fourier of that computed selection
remain unsupported storage operations. [Checked pointer borrows](c11-buffer-functions.md) now
recover the element index before canonical pointer-byte scaling; their nonnegative index is bounded
by the remaining owner range. Subsequent integration at `5c5a965`
([`full-ci-everyday-profile-v1`](audit-evidence/full-ci-everyday-profile-v1/README.es.md))
additionally supports multi-axis indexing (`m[q_i][q_j]`), struct-array members (`t[q_i].arr[q_j]`)
and checked search-pointer offsets (`p[q_i]`, `*(p + q_i)`). General pointer arithmetic, negative
indices into an offset borrow, general nested pointer layouts, dynamic allocation/counts, address
escape, general mutable pointer ABI, effectful indices and further loop/lifetime combinations also
remain pending. Pure private value programs can compose as indices or assignment values; CPU side
effects require sequencing.

[`quantum_index.c`](../examples/blocks/04/quantum_index.c) provides a source example of reads,
assignment values and postfix updates. `bash scripts/run-block.sh 04` supplies its native/export
command. The [ownership campaign](audit-evidence/ownership-contracts-v1/README.es.md) now executes
that example and checks finite coherent reads, writes, phases and inverses across compiler levels.
It does not exhaust index widths, all update consumers, resource limits, measurement channels or
broad integration, and does not complete block 04.
