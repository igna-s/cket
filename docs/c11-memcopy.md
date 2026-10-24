# C11 memory copying: quantum contracts and evidence

`memcpy` and `memmove` have partial adapters for checked integer storage. They now support retained
overwrites, partial object bytes, different integer element sizes and known CPU sources. The new
extension has delivery examples. Structured CPU loops now retain each overwritten owner, including
whole-element copies and partial object-byte updates. The
[fixed-loop campaign](audit-evidence/fixed-memory-loops-v1/README.es.md) records finite complete
states, phases, inverses and sanitizer checks; general compiler-context qualification remains open.
Earlier campaigns certify their own frozen implementations.

The C reference is N1570 [7.24.2.1–2](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf).
`memcpy` requires nonoverlapping ranges. `memmove` reads original source bytes even when ranges
overlap. Both return their destination. The frontend supports discarded calls and checked local
destination-return borrows; the [return contract](c11-memory-returns.md) describes their consumers.
General escaping or indirect quantum pointer ABI remains pending.

## Checked whole-element path

The original path accepts equal-size padding-free integer elements, constant byte counts divisible
by their object size, and checked scalar/array pointers with constant element offsets. It expands at
most 4096 elements. A zero-length range at a valid one-past pointer is accepted. Signed and unsigned
object representations of the same size transfer bits without changing their sign bit numerically.

For an independent known-basis destination `k`, the adapter translates by `k` and applies controlled
XOR copy gates. Its reversible extension is `|x>|y> -> |x>|y XOR k XOR x>`; on the proved support
`y=k`, the result has the required copied value. This path remains table-free, with at most one copy
gate and one basis-translation gate per bit. Exact `memmove` aliases preserve arbitrary states.

Unknown, derived or inverse-dependent destinations now use the existing retained-history assignment
path. Their previous owners remain available for saved inverses. This adds storage and removes the
former known-basis restriction for those overwrites; it does not inherit the fast path's linear gate
or zero-private-workspace guarantee. Existing lifetime and expression budgets still apply.

## Object-byte path

Partial words, byte pointer offsets, different source/destination element sizes and CPU sources use
a second path with **0–64 copied bytes**. Both ranges must resolve to checked storage and the count
must fit both. Quantum objects use padding-free integer representations with 8-bit bytes and element
sizes up to 64 bits. The target byte order determines shifts and masks; bytes outside the
destination range are preserved. The adapter does not reinterpret a quantum host placeholder as CPU
memory.

```c
quant unsigned char input = superposition(0, 1);
quant uint16_t word = 0x5a7f;
memcpy((void *)&word, (const void *)&input, 1);
const unsigned char cpu_bytes[2] = {'Q', 0};
memcpy((unsigned char *)&word + 1, cpu_bytes, 1);
```

Known nonvolatile CPU objects supply captured raw bytes, including embedded zero bytes. These are
not string loads: a zero does not terminate the copy. Captures are shared within the call and saved
for inverse computation. Mutable CPU sources introduce parameter bindings; static exports need
explicit specialization, while native execution reads the actual object. CPU volatile/atomic reads
need a separate access contract. Quantum-to-CPU writes require explicit measurement and remain
outside ordinary copying.

Byte updates are combined per destination integer. A dependency graph schedules a read before any
other word overwrites that source. Self-dependent partial writes use retained history. This
preserves original bytes for supported contiguous `memmove` ranges in either direction; unsupported
cyclic word dependencies require a retained snapshot and are rejected. `memcpy` rejects overlapping
nonempty byte ranges. Exact same-range `memmove` is an identity even for a partial word.

Retained computation can keep a source needed by a later consumer. Finish or explicitly observe that
consumer before measuring/resetting its input. The example measures the moved destination bytes in
dependency order. Full mixed-history lifetime qualification remains pending.

## Control, ABI and remaining work

Coherent controls select writes without measurement. Inactive branches preserve destination byte
values according to the existing retained-history mapping. This is a reversible mapping of basis
values with retained information; it does not create independent copies of an arbitrary unknown
quantum state. No physical QPU execution or speedup over CPU is claimed.

Direct header aliases preserve `memcpy`/`memmove` before GCC can replace them with host byte
operations. Classical calls still link to libc, standard function addresses retain their ABI, and
private user definitions are not adapted merely because their names match. Fortified system
declarations retain the existing identity check.

Nonidentity copies in structured CPU loops use the
[per-iteration history contract](c11-runtime-history.md). They always compute a retained version,
even if the destination was initially known: a later iteration cannot reuse that initial basis
proof. Read-before-write ordering preserves original `memmove` bytes in either direction. Exact
same-range identity and zero-length copies keep their existing no-write path. Checked static
expansions remain available. Dynamic counts/addresses, padding and `_Bool` object padding, general
aggregate layouts, general pointer returns and arbitrary aliasing functions remain pending.
Simulator, physical representation, expression and history budgets apply separately from the
frontend byte bound. The sparse simulator permits bounded occupied support; it does not make
arbitrary 128-qubit states tractable.

## Evidence

[`byte_copy.c`](../examples/blocks/06/byte_copy.c) compares an overlapping move, retained overwrite,
partial integer writes and a CPU byte source with libc. Build with GCC 11 and run
`CC=gcc-11 bash scripts/run-block.sh 06`. The runner executes the examples and exports
QASM/resources. These finite observations do not replace complete coherent/inverse checks.

Historical campaigns ran 414 memory-copy checks on their recorded sources, including complete phased
states/inverses, native C contracts and wide generation. Those results do not qualify the new
extension. The maintained validator moves seven obsolete rejection cases into positive native
contracts and uses sparse simulation for those executions. The runtime loop case is now a positive
native contract with consumers observed before their sources. The 414 checks include 120 complete
phased states/inverses, four overlapping states, 190 native contracts, 70 rejections and 30 wide
exports. The fixed-loop delivery separately records new runtime states and the maintained suite on
its own sources. The normal native core regression and delivery results are recorded separately in
the current evidence capsule.

The [C11 inventory](c11-inventory.md) and [interface index](c11-library.json) keep these interfaces
partial. Broad integration, sanitizers, complete states/inverses and all GCC contexts remain open.
