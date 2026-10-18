# C11 character classification and case conversion

All fourteen `ctype.h` interfaces have partial quantum adapters: `isalnum`, `isalpha`, `isblank`,
`iscntrl`, `isdigit`, `isgraph`, `islower`, `isprint`, `ispunct`, `isspace`, `isupper`, `isxdigit`,
`tolower` and `toupper`. This advances M2/M7; full C11 and library coverage remain open.

## Representation and C semantics

The declared target is GCC 11 on Linux/WSL with glibc, eight-bit bytes, ASCII-compatible encoding
and 32-bit `int`. Native calls capture the effective `LC_CTYPE`, including POSIX thread locales. The
engine duplicates that locale, reads its byte rules through explicit-locale libc interfaces and
frees the duplicate. It changes neither the global nor thread locale. Successful capture preserves
`errno`. This follows the native
[duplocale contract](https://man7.org/linux/man-pages/man3/duplocale.3.html). Concurrent global
locale mutation and concurrent use of the shared quantum runtime remain P.

[N1570, section 7.4](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf) requires an
argument representable as `unsigned char`, or `EOF`: `-1` or `0..255` on this target. The backend
proves this domain on reachable support before emitting gates. Invalid reachable arguments are
diagnosed; unreachable lazy arms impose no argument-domain requirement. Explicit casts follow C
conversion rules. Invalid C arguments have undefined semantics (S); failure to prove valid support
is P/R.

Classification retains the native glibc `int` masks, rather than normalizing to one. Boolean
conversion applies zero/nonzero rules. Case mappings retain the input when no mapping exists. `EOF`
is preserved and remains distinct from byte 255, even when that byte is classified. No implicit
measurement or reset implements a call. Wide characters and multibyte decoding remain P.

## Immutable profiles and coherent synthesis

Profile capture queries every classical byte value for each of the fourteen libc functions, plus
EOF, and compresses the results into intervals. This is bounded classical metadata acquisition.
`--no-tables` constrains the subsequent quantum expression synthesis; it does not prohibit this
byte-profile acquisition. Logical gate counts describe the resulting circuit and exclude the CPU
cost of capturing the profile.

The CPU snapshot compresses byte rules into sorted disjoint intervals. Classification intervals
store raw masks; case intervals store the XOR difference between input and output. Typed IR owns
these data. Acquiring a finite libc byte profile is classical data acquisition; it does not
enumerate quantum input states or synthesize a gate for each basis state.

Synthesis uses disjoint binary-prefix covers and groups equal masks/XOR differences. A sign-bit
condition excludes EOF from byte intervals. All `ctype` computations use this route, including when
general truth-table fallback is enabled. Private computation is reversed after copying the result
and private wires return to zero. Calls preserve their source, may entangle result and argument, and
preserve once-only increments in scheduled consumers.

Profiles have at most 256 intervals. Capture keeps fourteen current strings, each smaller than 8
KiB; replacement can allocate fourteen additional temporary strings. CPU work is bounded and text
specialization has a separate 64 MiB cap. Circuit cost grows with interval covers, distinct masks
and output bits. Arbitrary byte profiles can require more workspace than ASCII. Native simulation
has a 24-qubit cap; physical, gate and proof-work budgets are R.

Saved definitions and circuit-plan keys include actual profile data. Cleanup uses them even after
another capture, a locale change or freeing the original POSIX locale object. Current strings are
replaced at capture; definition copies follow the owner/inverse lifecycle. Parsed and plan caches
retain their existing bounded lifetimes.

## Static export and modules

Unresolved profiles receive structural validation, which reports no synthesized resources. Static
synthesis requires an explicit locale:

```bash
build/cket program.c --emit-ir -o build/program.json
build/cket_backend --emit-qasm build/program.json --no-tables --ctype-locale C
build/cket program.c --resources --ctype-locale C
```

That choice is a precondition for every selected capture. It does not reconstruct arbitrary CPU
`setlocale` calls, branch decisions or thread histories. Programs changing locales require a
compatible fixed-profile trace or separately specialized segments; automatic per-capture locale
traces remain P. Omitted or unavailable locales are rejected.

To publish the actual byte rules in a standalone artifact:

```bash
build/cket_backend --specialize-ctype build/program.json --ctype-locale C >build/program.fixed.json
build/cket_backend --emit-qasm build/program.fixed.json --no-tables
```

Specialization embeds profiles and removes top-level capture markers. Other CPU bindings/control
markers retain their own requirements. Borrowed unitary modules accept embedded profiles and reject
unresolved captures. Execution and inversion then need no mutable locale object.

## Integration and remaining restrictions

Legacy IR without profile fields denotes the fixed C byte profile. Its legacy guards compare
effective thread rules with that profile rather than inspecting only the global locale name.

The verifier checks bounded, sorted, disjoint and representable intervals. A user-written profile
declares a byte function; structural validation does not certify that its data came from libc.
Native captures and the finite differential tests provide that separate evidence.

The compiler includes the native system header and routes macros through distinct attributed
declarations with libc assembler aliases. Classical calls retain their native effects. Function
forms surviving GCC parsing are recognized by system declarations; private same-name functions are
not adapters. Tests use macros with builtins enabled and parenthesized calls with builtins disabled.
Indirect calls, complete provenance before GCC folding and automatic capture of mixed CPU call
effects remain P. Wide-character descriptors and general concurrency remain P/R. Other byte/integer
representations require another declared target implementation (P).

## Finite validation

`tests/validate_ctype.c` defines 1815 checks:

- 1680 complete phased states: all 256 bytes in `C`, `C.UTF-8` and isolated Latin-1, both call
  forms, five GCC levels and both synthesis policies. Raw masks and all case-mapping bits are
  contrasted with native libc; private amplitudes are included.
- 30 complete coherent EOF/byte-255 states across those locales, levels and policies.
- 70 integrated programs: effects, functions, arrays, borrows, loops, lazy domains, explicit
  observation, global/thread locale changes and cleanup after freeing the original locale.
- 10 domain diagnostics and 10 ordinary-GCC differentials over every byte and EOF.
- 12 malformed-profile/static-specialization contracts and three module contracts: unresolved
  capture rejection, immutable phased states and complete inverses under another locale.

Four QuEST fixtures independently contrast exported character circuits, including Latin-1, nested
calls, phases, borrows and coherent EOF. Generated circuits and externally contrasted states remain
separate evidence categories.

Tests create Latin-1 with `localedef --no-archive -i en_US -f ISO-8859-1` in their own temporary
directory and remove only generated fixtures. They require glibc locale sources and do not change
system locales.

```bash
export CC=gcc-11 CXX=g++-11
bash scripts/build.sh
build/validate_ctype
bash scripts/build-validation.sh
build/validate_qasm
```

`scripts/verify.sh` runs normal and ASan/UBSan campaigns. Completed transcripts and exact source
hashes belong in [audit-evidence](audit-evidence/README.md), separately from the historical paper
study. This finite evidence does not certify all installed locales or complete C11 coverage.

The glibc 2.35 target has a native `newlocale`/`LOCPATH` allocation leak
([upstream 25770](https://sourceware.org/pipermail/glibc-bugs/2020-April/047605.html)).
`tests/repro_glibc_locale.c` reproduces it without C Ket. The character, numeric-locale and QuEST
locale-fixture validators use the scoped `tests/glibc-locale.lsan` exclusion for `argz_add_sep`;
LeakSanitizer remains active for project allocations and locale handles.
`scripts/test-locale-sanitizer.sh` verifies both real leak sentinels remain detectable. This is an
external host-library defect, with an unbounded repeated lookup cost (R); repairing glibc requires
an upstream/host fix. The quantum adapter's owned profile strings, locale duplicates and register
lifetimes retain their explicit cleanup contracts.
