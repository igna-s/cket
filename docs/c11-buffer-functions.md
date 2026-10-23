# Borrowed buffers with quantum indices

The frontend now extends the [coherent index implementation](c11-coherent-index.md) to checked
integer/Boolean pointer borrows. A buffer can pass through direct functions in the same translation
unit, including read-only parameters and constant-offset local aliases. Reads produce correlated
quantum expression values. Writes preserve the previous storage versions.

```c
quant uint8_t selected_byte(quant const uint8_t *values, quant const bool index) {
    return values[index] ^ 3;
}

void replace_byte(quant uint8_t *values, quant const bool index) {
    values[index] = 11;
}
```

The borrow supplies a concrete checked owner range and an element type. The implementation uses the
suffix starting at its constant offset, containing 1–64 scalar elements. Its quantum index must be a
pure expression of at most 64 bits. This path currently supports nonnegative indices within that
suffix; negative indexing back before an offset borrow remains pending. A read-only borrow cannot be
written or passed to a mutable parameter.

GCC represents pointer indexing with a pointer-sized byte offset. Dividing an already scaled offset
would lose the original index when multiplication wraps. The new path instead recognizes the
unscaled operand of the canonical unsigned pointer-width multiplication, or an equivalent constant
left shift. Byte elements need no scale. Conversions on the index remain part of its C value; only
bit-preserving unsigned conversions around the scale may be removed for recognition.

The circuit checks the recovered index against the borrow's remaining element count before narrowing
the private selector. Thus it checks the index before pointer-byte wrapping could hide a large
value. Unrecognized scaling and compound address expressions retain a diagnostic. The finite
register multiplexer reuses existing typed synthesis; it performs no implicit measurement, reset,
host quantum-value load or physical quantum-RAM operation.

Discarded indexed increments now accept GCC's surrounding integer conversions: the statement path
lowers their effects and releases the unused result. Actual value consumers continue to serialize
the conversion through the expression path. Floating conversions are not added by this correction.

The maintained sources are [`buffer_functions.c`](../examples/blocks/03/buffer_functions.c), with
chained functions, a constant-offset alias and retained byte updates, and
[`buffer_words.c`](../examples/blocks/03/buffer_words.c), with a multi-byte element type.
`bash scripts/run-block.sh 03` supplies their native/export runner.

**Qualification is compilation-only.** No final-source example or suite has been executed for this
delivery. Joint states, phases, inverses, resource exhaustion, sanitizers, compiler configurations
and the complete runner still require validation. Their expected results do not establish that they
run within the simulator budget. The failed first compilation and corrected source snapshot have
separate evidence.

General CPU-indexed quantum reads, dynamic extents/allocation, pointer rebinding/escape, arbitrary
layouts, general function ABI, loop lifetimes and coherent external I/O remain incomplete. This
advance does not close the [everyday C profile](perfil-c-cotidiano.es.md).
