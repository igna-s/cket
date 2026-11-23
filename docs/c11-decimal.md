# Checked decimal conversion

`atoi`, `atol`, `atoll`, `strtol`, `strtoll` and `strtoimax` now have partial quantum adapters.
Their reference is [N1570 §7.22.1](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf) and
§7.8.2.3. They skip leading whitespace, accept one optional sign, and consume the first digit run
(base 10 for `atoi`/`atol`/`atoll`; checked constant bases `2..36` with optional `0`/`0x` auto-base
0 rules for `strtol`/`strtoll`/`strtoimax`, plus optional `NULL` or local `&end` `endptr`). No
conversion produces zero. The result must fit the signed return type; an out-of-range result has
undefined C semantics (or overflow bounds) and is rejected through a circuit domain guard when
reachable.

The checked source contains a NUL within 1–64 bytes of padding-free integer storage. Constant
offsets and the target byte order follow the existing byte-view contract. Dynamic addresses,
non-local `endptr` lifetimes, dynamic bases, `errno`/`ERANGE` outputs, other encodings, general
layouts and general CPU-loop lifetimes remain pending.

The parser builds typed computations for its phase, sign and magnitude. Private zero registers are
prepared independently of coherent controls; selected computations and saved inverses use the
existing reversible engine. No measurement chooses a character or parsing branch. Intermediate
widths grow with the possible digit count, up to the target result width. Previous values remain
available for inverses. Source and private state add resource costs; a 64-byte frontend bound does
not promise that every such circuit fits the baseline 128-wire representation (or Wave 1A engine's
1024-wire expansion) or simulator budgets.

Whitespace classification uses the captured effective byte `LC_CTYPE` profile, including the
existing thread-locale contract. ASCII digits retain decimal meaning. Locale-specific extensions to
numeric syntax outside that grammar remain pending. Ordinary CPU calls and standard function
addresses keep libc ABI. The adapter adds no errno output. Its successful metadata capture preserves
errno under the existing ctype contract.

Static exports must supply a character profile explicitly, for example `--ctype-locale C`. The
delivery runner does so only for this example; native execution captures its actual effective
locale. Missing static profiles retain their existing rejection. Dynamic allocation, arbitrary
history/control combinations and indirect quantum calls remain pending.

[`decimal_conversion.c`](../examples/blocks/06/decimal_conversion.c) converts a superposed two-digit
string with all three functions and compares the observations with libc. `atoi` uses an actual
32-bit result; the `atol`/`atoll` results are explicitly narrowed to an 8-bit consumer. That finite
example does not prove all 64-bit boundaries. The
[ownership campaign](audit-evidence/ownership-contracts-v1/README.es.md) now checks finite complete
states, phases and cleanup for these consumers across five GCC levels and a separate instrumented
configuration. General locale combinations, overflow boundaries and larger strings remain pending.

This advances partial library compatibility; it does not complete C11 or claim physical QPU
execution, independent state cloning or acceleration over CPU.
