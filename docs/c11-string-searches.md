# String spans and search predicates

This delivery adds `strspn` and `strcspn` prefix lengths and direct consumers of `memchr`, `strchr`,
`strrchr`, `strpbrk` and `strstr`: null/address comparisons and same-object pointer differences. It
is partial implementation. Full coherent state, inverse, sanitizer and compiler-context
qualification is deferred until the implementation block closes. Earlier audit campaigns do not
validate these new sources.

The C reference is
[WG14 N1570, §7.24.5](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf). `strspn` counts
an initial segment in a character set; `strcspn` counts one outside the set. The search predicates
answer whether a character, set member or substring occurs. A null comparison of `strrchr` has the
same existence result as `strchr`; position consumers distinguish the first and last occurrences.

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

A direct `== NULL` or `!= NULL` consumes an existence predicate, with either operand order. A
Boolean conversion or condition is supported when GCC represents it in that form. Direct comparisons
against checked addresses within the same source object use the selected position. Equality returns
false for a missing match; inequality returns true. Relational ordering requires a successful
search. Cross-object search-address comparisons remain pending.

Direct subtraction against a checked address in the same object produces its signed byte distance,
with either operand order; GCC retains the element-size division for typed pointer subtraction. For
example, `quant int8_t last = strrchr(text, 'a') - text;` computes the last occurrence's offset when
`text` contains an `a`. Subtracting or ordering a null pointer is undefined C; a domain guard keeps
a failed search from becoming a fabricated valid offset. The existing backend decides whether an
evaluated expression has a provably defined domain, so general guarded compositions still need
qualification. CPU object identity is checked independently of contents: two arrays containing the
same bytes do not become the same pointer provenance.

Local character/void pointers can now retain a search result. The compiler saves an 8-bit quantum
selector once, with a missing-match sentinel and checked object provenance. Copying the local
pointer creates an immutable alias of that selector. Null tests, same-object comparisons and
differences consume the saved result rather than rerunning the search. Two saved results compare
equal when both are null, or when both select the same address in the same checked object.

```c
const char *last = strrchr(text, 'a');
const char *alias = last;
quant int8_t offset = alias - text;
quant char selected = *alias;
quant char following = alias[1];
```

Character reads support direct dereference and constant indexing, including constant pointer-byte
offsets within a read expression. A read selects from the checked object bytes without measuring the
selector. The pointer must be nonnull and the selected address must be inside the object; one-past
addresses are not readable. The result has the pointed-to character's signedness. Dynamic indexes,
wider pointed-to layouts and volatile/atomic reads remain pending.

The saved position is fixed, while a later read uses current source contents. Mutable CPU sources
capture fresh candidate bytes, retaining initial string-reachability guards from the original
search. These accesses can load multiple checked candidate bytes; they do not provide quantum RAM.
Quantum source updates remain subject to the existing dependency and retained-history rules. CPU
mutation, guard-page access, quantum mutation and every mixed context still need the deferred
qualification; the delivery example alone does not establish those contracts exhaustively.

These local borrows cannot be rebound or used for writes; pointer/overwrite history remains pending.
General pointer arithmetic outside a read, passing/returning a pointer through a function, escaping
to CPU storage and a general quantum pointer ABI are also pending. Such uses cannot inspect a host
placeholder as quantum data.

The engine now retains conservative nonnegative support intervals inferred from computed
expressions. It can independently prove the bounds of a saved selector instead of treating it as
every possible byte. Basis-changing operations invalidate these certificates. Table synthesis
extends certified unreachable labels without evaluating their invalid C branches; interval analysis
uses the same certificates. Inference is optional and bounded: a failed proof preserves the ordinary
full-type domain rather than accepting an asserted frontend range. Complete qualification of this
shared-engine change remains part of the deferred closure.

Known CPU objects supply once-only byte captures. String loads stop at their own CPU terminator;
saved inverses retain the captured values after subsequent CPU mutation. Volatile and atomic objects
are rejected. Both string arguments must satisfy the checked termination contract, even where a
particular successful search would need fewer bytes in libc. Missing terminators currently use the
existing internal arithmetic-domain diagnostic; a dedicated string diagnostic is pending.

Ordinary CPU calls and standard function addresses keep libc ABI. Function-like header aliases
preserve calls for the quantum frontend before GCC folds them into host byte operations; private
user functions with matching names are not recognized as library adapters.

The examples are [`string_spans.c`](../examples/blocks/06/string_spans.c),
[`string_search.c`](../examples/blocks/06/string_search.c),
[`search_positions.c`](../examples/blocks/06/search_positions.c) and
[`saved_search.c`](../examples/blocks/06/saved_search.c). Build with GCC 11, then run:

```bash
CC=gcc-11 CXX=g++-11 bash scripts/build.sh
CC=gcc-11 bash scripts/run-block.sh 06
```

The runner selects the bounded sparse simulator and produces native executables, OpenQASM and
resource reports. This is an advance within block 06; it does not close the block or certify C11.
Physical QPU execution and a speedup over classical search are not claimed.
