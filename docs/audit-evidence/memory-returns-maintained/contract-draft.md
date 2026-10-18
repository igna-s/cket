# Checked destination returns from memory calls

This contract belongs to an isolated compiler extension. It is not yet applied to the main tree. It
covers local checked address borrows returned by `memset`, `memcpy` and `memmove`; general quantum
pointer and separately compiled call ABIs remain incomplete. Cybersecurity stays classical.

## Address and value semantics

These standard memory functions return their destination. The extension emits the supported quantum
memory operation once and keeps that destination as compiler address metadata. Address comparisons
do not observe the quantum data stored there. Updating the destination's quantum version does not
make the borrow refer to a stale value.

The primary C reference is [N1570](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf),
sections 7.24.2.1, 7.24.2.2 and 7.24.6.1. Section 7.24.1 paragraph 2 permits zero length while
retaining the valid-pointer requirement; zero count does not authorize arbitrary pointer values.

Local declarations may infer a checked quantum borrow from a supported memory return or an existing
borrow. Compatible typed pointers and `void *` are supported. Character pointer views can carry
checked byte ranges into subsequent supported memory calls; this does not establish every scalar
character dereference or reinterpretation. Fixed offsets and the memory adapter's storage/count
constraints still apply. Pointee `const` is retained across aliases and casts cannot authorize a
write through a checked read-only borrow.

Equality, inequality, non-null tests and supported ordering within a known array are evaluated from
the checked address metadata. Cross-object ordering and ambiguous one-past adjacency require more
layout information and remain pending. The pointer representation is not exposed as an ordinary CPU
address or integer. CPU escapes, implicit reads of quantum values, incompatible pointee types and
general pointer rebinding keep their diagnostics.

Static expansion, direct quantum helpers and coherent scopes retain local alias state. Supported
quantum conditions control the memory writes and phases, without hidden measurement or reset. Zero
count still evaluates the value argument, preserves its effects and returns the checked one-past
destination in the supported cases.

## Example

```c
#include <quantum.h>
#include <string.h>

int main(void) {
    quant uint8_t value = 5;
    uint8_t *pointer = memset(&value, 17, 1);
    unsigned observed = measure(*pointer);
    return observed != 17 || pointer != &value;
}
```

The declaration obtains a checked borrow. The explicit `measure` observes the destination through
that borrow. A classical initialization such as `unsigned observed = *pointer` requires an explicit
observation boundary and is rejected. Ordinary CPU memory calls and pointers retain libc behavior.

## Verification scope

The maintained [C validator](../tests/validate_memory_returns.c) is connected to the normal and
ASan/UBSan verification scripts. Its 136-input scoped campaign passed 290 checks in each normal and
instrumented configuration. That run reuses source-identical compiler/core tools from the preceding
isolated experiment and builds the new validator separately. It is not a fresh complete compiler
regression or a main integration result.

The matrix contains 130 native executions and 60 specific rejections across five optimization levels
and both builtin modes. Twenty-six ordinary C reference executions and six ordinary syntax checks
are tracked separately. Expected rejections do not implement the rejected contexts.

The 100 [complete labeled-action checks](../tests/memory_return_state.inc) cover five source
profiles, both synthesis policies, all five optimization levels and both builtin modes. They verify
known values, zero-count value effects, coherent pointer control, relative phases, saved-definition
inverses, retained history and zero private wires. The word profile enumerates every one of the
65,536 input labels after verifying its sixteen-H preparation; the Boolean profiles check both
labels after a verified one-H preparation. The largest circuit uses 49 physical wires. This is a
complete check of those prepared input subspaces, not a dense simulation over 49 qubits.

The preceding archived 134-input experiment retains ten independent sparse prefix/inverse exports at
O2 with builtins and no tables. Two 10-wire control exports also match pinned QuEST 3.7.0 over all
complex amplitudes. Those older scoped proofs retain their own sources and are not relabeled as a
fresh broad regression for this maintained suite.

## Remaining work

The full broad regression and main integration remain pending. General pointer assignment and
escape, dynamic addresses/counts/allocation, indirect ABI, byte reinterpretation, aggregates and
layouts, and broader control/observation ordering require further implementation. Existing copy/fill
history and resource conditions still apply. No arbitrary-state cloning, physical QPU execution,
quantum speedup or complete C11 compatibility is established by these finite checks.
