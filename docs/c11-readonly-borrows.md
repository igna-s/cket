# Shared read-only quantum borrows

The frontend accepts repeated read-only arguments in direct same-unit quantum procedures. Scalar
parameters and checked fixed-buffer pointer views can refer to the same owner:

```c
static void mark(quant const bool a, quant const bool b) {
    if (a && b)
        phase(3.14159265358979323846);
}

/* The two parameters refer to the same register. */
mark(value, value);
```

The call borrows one existing register through two names. It does not create independent copies of
its state. Expressions may use temporary registers under the existing uncomputation contract.
Overlapping read-only pointer suffixes and scalar/pointer views follow the same rule.

If either overlapping parameter is mutable, the call remains rejected in either argument order. The
frontend tracks all borrowed storage separately from the subset that may be written, so sharing a
read-only view never permits an ownership move or `find` to consume its owner.

The read-only quantum contract also survives casts. Before resolving a callee pointer access to the
caller's underlying register, the frontend checks assignments and increments against the active
borrow scope. Taking the address of a scalar parameter does not remove that scope. Classical C
pointers retain GCC semantics; this is an explicit contract of quantum borrows.

Previously, the fixed-storage call checker rejected even two read-only arguments. Separately, a
single read-only pointer parameter could be cast and written because the read-only scope had ended
before statement lowering. The new check closes that demonstrated case while allowing read-only
sharing. Heap borrows already distinguish parameter mutability; their general alias and lifetime
combinations remain subject to their separate contract.

The maintained [example](../examples/readonly_borrows.c) applies a phase through scalar and buffer
aliases, then interferes the inputs to obtain `scalar=1 vector=0,1`. This deterministic output
checks a phase effect, rather than just unchanged computational-basis probabilities.

The focused validator is integrated into the normal build, sanitizer runner and full verification
script. Its 305 checks comprise 30 IR compilations, 30 native executions, 60 complete states against
C formulas, 60 independently evolved gate states, 60 independent complete gate inverses and 65
expected mutation/ownership diagnostics. It uses five GCC levels and two synthesis policies for the
state checks. Only the two explicit final observations are excluded from the unitary state/inverse
comparison; unexpected channels are rejected by the reference.

This finite correction does not establish a general function ABI, arbitrary layouts, dynamic
lifetimes or unrestricted overlapping mutable borrows. The full integration gate remains open.
