# String spans and search predicates

This delivery adds `strspn` and `strcspn` prefix-length expressions and direct null comparisons of
`memchr`, `strchr`, `strrchr`, `strpbrk` and `strstr`. It is partial implementation. Full coherent
state, inverse, sanitizer and compiler-context qualification is deferred until the implementation
block closes. Earlier audit campaigns do not validate these new sources.

The C reference is
[WG14 N1570, §7.24.5](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf). `strspn` counts
an initial segment in a character set; `strcspn` counts one outside the set. The search predicates
answer whether a character, set member or substring occurs. A null comparison of `strrchr` has the
same existence result as `strchr`; selecting the last address remains pending.

```c
quant unsigned char text[2] = {superposition(0, 255), 0};
quant uint8_t prefix = strspn((const char *)text, "0123456789");
quant bool found = strstr((const char *)text, "a") != NULL;
if (found)
    phase(0.17);
```

Inputs remain quantum. The compiler constructs typed integer/Boolean expressions and uses the
existing reversible synthesis paths; the adapters introduce no measurement or reset. Prefix lengths
retain `size_t` semantics before an explicit C conversion to a narrower result. The source and
checked CPU captures remain available to inverse synthesis.

Each remaining string view must contain a NUL within 1–64 bytes. Views use the existing checked,
padding-free integer storage model; fixed arrays, constant offsets and overlapping read-only views
are allowed. This is a frontend synthesis bound, separate from gate and simulation budgets. A
checked object longer than 64 bytes is rejected even when its first byte happens to be NUL. General
layouts, dynamic addresses and indirect quantum calls remain pending.

`memchr` instead takes a constant count from 0 to 64, contained in its checked source view. It can
search binary data, including zero. Its character is converted to `unsigned char`; `strchr` and
`strrchr` use target `char` conversion and include the terminating NUL as a searchable character.
Character expressions must be pure; sequence CPU increments, calls or volatile reads separately.
`strpbrk` ignores the terminating NUL and bytes following it. An empty `strstr` needle succeeds,
including in an empty haystack; a match cannot continue through the haystack terminator.

Only a direct `== NULL` or `!= NULL` consumes a quantum search result in this block, with either
operand order. A Boolean conversion or condition is supported when GCC represents it in that form.
Saving the pointer, subtracting it, comparing it to an object address or dereferencing it is
pending. Such calls are not allowed to inspect a host placeholder as though it were quantum data.

Known CPU objects supply once-only byte captures. String loads stop at their own CPU terminator;
saved inverses retain the captured values after subsequent CPU mutation. Volatile and atomic objects
are rejected. Both string arguments must satisfy the checked termination contract, even where a
particular successful search would need fewer bytes in libc. Missing terminators currently use the
existing internal arithmetic-domain diagnostic; a dedicated string diagnostic is pending.

Ordinary CPU calls and standard function addresses keep libc ABI. Function-like header aliases
preserve calls for the quantum frontend before GCC folds them into host byte operations; private
user functions with matching names are not recognized as library adapters.

The examples are [`string_spans.c`](../examples/blocks/06/string_spans.c) and
[`string_search.c`](../examples/blocks/06/string_search.c). Build with GCC 11, then run:

```bash
CC=gcc-11 CXX=g++-11 bash scripts/build.sh
CC=gcc-11 bash scripts/run-block.sh 06
```

The runner selects the bounded sparse simulator and produces native executables, OpenQASM and
resource reports. This is an advance within block 06; it does not close the block or certify C11.
Physical QPU execution and a speedup over classical search are not claimed.
