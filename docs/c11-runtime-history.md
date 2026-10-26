# Native runtime iteration history

**Current finite follow-up:** the
[RC3 fixed-buffer correction](audit-evidence/fixed-patches-v1/README.es.md) extends masked writes to
fixed words and preserves their untouched bytes in the published counterexample. CPU-loop
regressions still compare complete correlated complex states and inverses; their observer records
mask/layout while the reference predicts masked old values independently. General lifetimes,
aliases/ABI, all string writers and large profiles remain open.

Structured CPU loops can now retain separate versions of scalar assignments, heap update results and
heap-to-fixed byte destinations. The C condition chooses iterations classically; the payload
continues to use quantum registers. This advances the everyday C profile and does not implement
general coherent loops, arbitrary CFG lifetimes or an unrestricted pointer ABI.

```c
quant uint8_t *values = calloc(1, sizeof *values);
quant uint8_t output = 7, sum = 0;
for (unsigned i = 0; i < rounds; ++i) {
    sum += values[0]++;
    memcpy((void *)&output, (const void *)values, 1);
}
```

The complete [example](../examples/blocks/05/heap_iterations.c) checks allocation, prepares a
coherent input, observes results explicitly and compares them with C formulas. `rounds` comes from
`argv`. Zero iterations preserve the initial destination and accumulator.

## Owners and inverses

`qassign` computes the typed expression into clean private storage before replacing its public
owner. The previous owner receives a private identity. Every saved inverse that referred to that
owner is relabeled consistently, including dependencies and their epochs. The public name then owns
the computed result. The computation uses real gates; identity transport adds no gates. Self reads
resolve before transport, so `x = f(x)` keeps the information needed for inversion.

`qslot` gives an internal expression capture clean storage on each executed iteration. An existing
capture is retired with its own identity first; borrowers keep its concrete saved recipe alive. This
supports a postfix result after the heap element has changed. GCC's shared `SAVE_EXPR` for a
compound assignment is captured once before its consumer. Restoring integer promotions preserves
that capture's identity. Independent unsigned accumulation can keep its existing reversible update
instead of creating an avoidable history chain.

Runtime loop exits dispose optional internal captures, including when the loop executes zero times.
Derived values are inverted only after their borrowers finish. Known roots may be restored with X
gates; unknown roots remain joint outputs. No measurement, reset or partial trace erases history. A
byte transfer of length zero preserves the destination's existing owner and wires.

CPU branch and back-edge joins keep the stable public owner and use concrete saved definitions for
later cleanup. Structural template validation checks shape and types; actual native paths enforce
inverse dependencies. An unexecuted template is not evidence for every runtime input.

For heap transfers, fixed buffer parameters retain the actual caller address for classical
destination returns. Compiler-only reference markers never reach GCC's native address lowering.
Modified CPU value parameters in expanded procedures receive local copies; decrementing a callee's
loop count does not change its caller. Read-only CPU parameters keep their existing once-only input
path. This corrects an internal GCC error in a procedure that copied a heap into its fixed-buffer
parameter inside `while(n--)`.

## Resources and pending contexts

Retained versions consume qubits. A new assignment may need another full result width and synthesis
workspace. The backend represents at most 128 physical wires; dense native simulation supports 24,
with a separate bounded sparse mode. Reaching a budget fails explicitly rather than dropping old
states. Repeated self-dependent versions can exhaust that budget even when the C loop is finite.

Fixed-to-fixed `memcpy`, `memmove` and `memset` now share this history path in structured CPU loops,
with checked addresses. Counts may also be [pure CPU runtime values](c11-runtime-memory.md). A
self-dependent fill byte is captured before any word changes. Their
[finite evidence](audit-evidence/fixed-memory-loops-v1/README.es.md) is separate from the earlier
heap-loop campaign.

Quantum declarations inside controlled branches or CPU loops, general quantum loop conditions, early
returns with live owners, arbitrary jumps, general indexed overwrites and heap strings retain
separate pending contracts. Runtime iteration history and heap access require native execution; flat
QASM export and the borrowed unitary-module ABI reject this profile. Cybersecurity, cryptography and
authentication remain classical.

```sh
CC=gcc-11 CXX=g++-11 bash scripts/build.sh
bash scripts/run-block.sh 05
CKET_SIMULATOR=sparse build/blocks/05/heap_iterations 3
build/validate_runtime_versions
python3 scripts/validate-runtime-versions.py --output build/runtime-history-proof --levels O0 O2
```

The directed validator separates complete complex joint-state formulas, independent gate evolution
and reversal of the complete gate stream. Iteration counts 0, 1 and 3 exercise retained scalar
values, CPU branches, heap postfix results, accumulation, partial runtime lengths, function buffer
parameters and saved inverses. The
[delivery evidence](audit-evidence/runtime-history-v1/README.es.md) records 300 complete state
checkpoints and 90 complete gate inverses across normal `O0`/`O2` and ASan/UBSan `O1`, with both
synthesis policies. The engine contracts separately cover malformed schemas before mutation,
transport of old inverse dependencies and explicit exhaustion of the 128-wire history budget. These
are finite checks with declared configurations; they do not certify all C11 or arbitrary quantum
input states.

## Conditional inverse transport in 1.0.0-rc.1

The [later scoped-borrow correction](audit-evidence/rebound-borrows-v1/README.es.md) supersedes the
earlier failing possible-active root-overwrite examples. Saved inverses refer to the conditional
location of each old input after a controlled swap. Reconstructible private decisions are expanded
from stable roots so their physical copies can clean while declared borrowers remain live. Epochs,
control masks, acyclicity and proof budgets are checked before the exchange. Complete complex states
are checked before/during/after cleanup, including negative signed values and phase kickback. This
finite correction does not qualify general repeated frontend writes, derived destinations,
correlated range proofs, 64-bit scope cleanup or nonidentity coherent heap copies. The latter still
has a reproduced inactive-sector coherence failure. See the
[candidate scope](releases/v1.0.0-rc.1.es.md).
