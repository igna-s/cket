# Checked `strlen` and `strcmp`

This contract describes the partial adapters in the current 144-input integration. It does not claim
complete quantum C11 string compatibility or a completed broad campaign.

The adapters preserve the input registers and compute the result reversibly. `strlen` retains C's
`size_t` result type. `strcmp` interprets characters as unsigned bytes and returns the exact
difference of the first differing bytes, a conforming choice for C's required sign. A direct
comparison against zero can use a Boolean circuit. A result's saved inverse uses the same captured
CPU values after the CPU object changes.

The current implementation requires checked integer object-byte views without padding. Each
remaining view must contain between 1 and 64 bytes. It must have a terminating NUL on the evaluated
domain. These are implementation bounds; they do not characterize all valid C strings. Dynamic
sizes, general pointers, other quantum layouts and indirect quantum calls remain pending.

For mutable known CPU objects, byte reads stop after their own NUL and are captured once. String
literals supply immutable bytes. Pure classical calls and function addresses retain libc behavior.
The header aliases keep ordinary calls visible before GCC folds builtins. They do not solve every
provenance loss from source folding or every function-pointer quantum ABI.

An identical checked quantum view compares equal without reading its data. That identity is valid on
strcmp's defined string domain; no diagnostic is promised for an otherwise invalid call with
identical unterminated inputs. Other missing-NUL paths use an internal typed division-by-zero domain
guard. Its generic diagnostic currently mentions division/remainder; it refers to that internal
guard, not a division written in the source. A dedicated string diagnostic remains pending.

The 64-byte frontend bound is separate from expression, gate and physical resource budgets. Native
dense simulation supports at most 24 physical wires; the gate representation supports 128. Wider
exported states can be checked with an independent sparse simulator on finite domains. This is not
evidence of physical hardware execution or quantum speedup.

For example, the selected GCC/Linux target can run this complete program:

```c
#include <quantum.h>
#include <string.h>

int main(void) {
    quant char input[2] = {'a', 0};
    quant uint8_t length = strlen(input);
    int bad = measure(length) != 1;
    reset(input[0]);
    reset(input[1]);
    return bad;
}
```

An equality result can also remain quantum until explicitly observed:

```c
#include <quantum.h>
#include <string.h>

int main(void) {
    quant char input[2] = {'a', 0};
    quant bool equal = strcmp(input, "a") == 0;
    int bad = !measure(equal);
    reset(input[0]);
    reset(input[1]);
    return bad;
}
```

The maintained validator covers native programs, explicit rejected domains, complete phased states,
saved inverses and separate wide-generation cases. Independent compiled-circuit proofs additionally
check complete 64-bit `size_t` and 32-bit `int` results, two quantum inputs and mutable CPU
snapshots. The [integration evidence](audit-evidence/strings-integration-v6/README.es.md) records
the closed targeted normal/ASan/UBSan checks and 640 independent complex states. Its broader local
regression remains pending after a WSL interruption. The
[remote CI](audit-evidence/integrated144-remote-ci/README.es.md) passed all 28 normal/sanitizer
suites on the same 144 compiler inputs, including 460 string checks per configuration. The smaller
remote fuzz campaign remains distinct from the expanded local campaign. Cybersecurity remains
classical.
