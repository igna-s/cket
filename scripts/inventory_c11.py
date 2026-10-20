#!/usr/bin/env python3
"""Rebuild/check the C11 interface index. This does not certify implementation.

Default validation uses only Python's standard library. Rebuilding requires
PyMuPDF and a local copy of the public WG14 N1570 draft. Only interface names
are retained; declarations and explanatory text are not redistributed.
"""
from __future__ import annotations

import argparse
import collections
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TARGET = ROOT / "docs" / "c11-library.json"
HEADERS = "assert complex ctype errno fenv float inttypes iso646 limits locale math setjmp signal stdalign stdarg stdatomic stdbool stddef stdint stdio stdlib stdnoreturn string tgmath threads time uchar wchar wctype".split()
MACRO_FUNCTIONS = {
    "assert",
    "CMPLX",
    "CMPLXF",
    "CMPLXL",
    "fpclassify",
    "isfinite",
    "isinf",
    "isnan",
    "isnormal",
    "signbit",
    "isgreater",
    "isgreaterequal",
    "isless",
    "islessequal",
    "islessgreater",
    "isunordered",
    "setjmp",
    "va_arg",
    "va_copy",
    "va_end",
    "va_start",
}
INTEGER_TYPES = {
    "bool",
    "wchar_t",
    "wint_t",
    "char16_t",
    "char32_t",
    "size_t",
    "ptrdiff_t",
    "intmax_t",
    "uintmax_t",
    "intptr_t",
    "uintptr_t",
}
ABS = {"abs", "labs", "llabs", "imaxabs"}
STRING_SPANS = {"strspn", "strcspn"}
STRING_COPIES = {"strcpy", "strncpy"}
STRING_CONCATENATIONS = {"strcat", "strncat"}
STRING_SEARCHES = {"memchr", "strchr", "strrchr", "strpbrk", "strstr"}
MEMORY_COPY = {"memcpy", "memmove"}
DIVISION = {"div", "ldiv", "lldiv", "imaxdiv"}
DIVISION_PRODUCTS = {"div_t", "ldiv_t", "lldiv_t", "imaxdiv_t"}
FAMILY_CONTRACTS = {
    "assert.h": (
        ["P", "S"],
        "M1/M3/M7",
        "Classical assertions remain CPU checks. A coherent condition needs a reversible failure flag; classical termination on an unmeasured branch is incompatible with coherence.",
    ),
    "complex.h": (
        ["P", "R"],
        "M6/M7",
        "Represent paired quantum real/imaginary encodings, finite rounding modes, special values and branch cuts; preserve inputs and coherently retain exception/status outputs.",
    ),
    "ctype.h": (
        ["P"],
        "M7",
        "Capture an immutable declared locale; input is EOF or an unsigned-char value promoted to int. Produce the classification/mapped value reversibly without measuring the input.",
    ),
    "fenv.h": (
        ["P", "R"],
        "M6/M7",
        "Separate the CPU environment from coherent rounding/exception state. Retain previous flags/modes for reversible updates and distinguish explicit CPU effect boundaries.",
    ),
    "inttypes.h": (
        ["P", "R"],
        "M2/M4/M7",
        "Integer adapters need exact target widths, product-return ABI for division, bounded string domains and retained parsing state; formatting crosses an explicit measured/CPU boundary.",
    ),
    "locale.h": (
        ["P", "S"],
        "M7",
        "Locale tables may be snapshotted as classical circuit parameters. An unmeasured branch cannot silently modify the process's single classical locale; coherent locale state requires a separate model.",
    ),
    "math.h": (
        ["P", "R"],
        "M6/M7",
        "Use the declared finite quantum floating format, rounded reference and domain/special-value rules. Preserve inputs and carry pointer outputs/exception flags coherently; returning errno is an explicit boundary.",
    ),
    "setjmp.h": (
        ["P", "S"],
        "M3/M5/M7",
        "Classical transfers must respect quantum ownership and cleanup on all edges. A coherent jump cannot discard branch/history information or escape to a single CPU continuation without an explicit effect boundary.",
    ),
    "signal.h": (
        ["P"],
        "M3/M5/M7",
        "Classical signal effects and asynchronous handlers require checked quantum lifetimes, effect synchronization and reentrancy; do not invoke the current global runtime concurrently.",
    ),
    "stdarg.h": (
        ["P"],
        "M5/M7",
        "A quantum variadic ABI must retain typed ownership, widths and effects. A libc va_list currently describes CPU arguments, not quantum storage.",
    ),
    "stdatomic.h": (
        ["P", "R"],
        "M4/M7",
        "Specify quantum ownership and happens-before/order contracts. Exchanges/CAS need result/history registers; classical memory ordering alone supplies neither quantum cloning nor coherent erasure.",
    ),
    "stdio.h": (
        ["P", "S"],
        "M4/M5/M7",
        "Classical I/O remains available for CPU data. Obtaining a scalar/string to send to a classical stream requires explicit measurement; coherent output/environment effects need a separately declared model.",
    ),
    "stdlib.h": (
        ["P", "R"],
        "M2/M4/M5/M7",
        "Each pure value operation needs a reversible result adapter and bounded defined domain. Allocation, comparator callbacks, randomness, environment and termination require their own memory/ABI/effect contracts.",
    ),
    "string.h": (
        ["P", "R"],
        "M4/M7",
        "Use bounded quantum byte/string storage with checked aliases and explicit length/terminator domains. Copy into zero storage creates basis correlation, not a cloned unknown state; destructive/overlapping writes retain history.",
    ),
    "tgmath.h": (
        ["P", "R"],
        "M6/M7",
        "Dispatch by the actual qualified real/complex type to a verified quantum mathematical adapter, preserving rounding and exception results; recognizing the macro alone is insufficient.",
    ),
    "threads.h": (
        ["P", "R"],
        "M4/M5/M7",
        "Classical threads require exclusive/shared quantum ownership, a synchronized runtime and declared ordering. The current global runtime is not a verified concurrent quantum execution model.",
    ),
    "time.h": (
        ["P", "S"],
        "M4/M7",
        "Pure bounded date conversions can be circuits; clock/environment reads remain explicit classical inputs. A classical clock cannot silently return a coherently branch-specific CPU value.",
    ),
    "uchar.h": (
        ["P", "R"],
        "M4/M7",
        "Bound the encoding sequences, specify locale/invalid-input behavior and preserve multibyte conversion state with reversible status/count outputs.",
    ),
    "wchar.h": (
        ["P", "R", "S"],
        "M4/M5/M7",
        "Define target wide-character layout and bounded stateful conversions/copies. Wide I/O requires explicit measured/CPU data; copying quantum wide storage must preserve aliases/history rather than clone states.",
    ),
    "wctype.h": (
        ["P", "R"],
        "M5/M7",
        "Use a declared immutable locale, checked character domains and descriptor dispatch. Produce classifications/transformations reversibly without implicit measurement.",
    ),
}


def semantics(header, name, kind, optional):
    if optional:
        return (
            "pending",
            ["P"],
            "M7",
            [],
            "Optional Annex K target profile; no quantum adapter is implemented.",
        )
    if header == "ctype.h":
        return (
            "partial",
            ["P", "R", "S"],
            "M2/M7",
            ["tests/validate_ctype.c", "tests/validate_qasm.c"],
            "Input-preserving immutable byte-locale classification and case conversion on the declared "
            "GCC/glibc 8-bit-byte, 32-bit-int target. Raw classification masks, EOF and argument "
            "definedness are preserved. Direct interval-prefix synthesis has no input truth table; "
            "saved inverses retain the effective global/thread locale profile. Embedded profiles "
            "support unitary modules. Indirect calls, automatic per-capture static locale traces, "
            "concurrency and full source provenance before GCC folding remain pending. "
            "Native simulation and physical/gate budgets are resource bounds. Arguments outside "
            "EOF/unsigned-char have no defined C11 meaning, not an omitted adapter.",
        )
    if name in ABS:
        return (
            "partial",
            ["R"],
            "M2/M7",
            ["tests/test_library.inc"],
            (
                "Input-preserving signed integer absolute value; representable result required. "
                "Builtins and external system declarations are adapted. Width, gate and "
                "simulator budgets apply; current tests cover narrow promoted sources and "
                "a complete small signed state domain, not every 64-bit input."
            ),
        )
    if name in DIVISION or name in DIVISION_PRODUCTS:
        return (
            "partial",
            ["P", "R"],
            "M2/M4/M7",
            ["tests/validate_division.c", "tests/validate_bindings.c", "tests/validate_qasm.c"],
            "Input-preserving signed integer quotient/remainder products and scalar projections "
            "retain the standard parameter/field types and require a defined C domain. Pure quantum "
            "arguments and once-only CPU integer inputs are adapted; effectful quantum arguments "
            "need sequenced capture. Classical structure results can prepare fresh quantum fields. "
            "Unbound templates have structural validation only; numerical synthesis requires values. "
            "Constant power-of-two lowering is table-free. Full div_t "
            "products can be generated on the declared target; two 64-bit fields plus a live input "
            "exceed the current 128-qubit representation. Small complete phased states and inverse "
            "are validated separately from wide circuit generation. General product ABI, library "
            "composition and wider resource budgets remain open.",
        )
    if header == "string.h" and name in STRING_CONCATENATIONS:
        return (
            "partial",
            ["P", "R"],
            "M4/M7",
            [],
            "Concatenation snapshots the original destination length and source prefix length "
            "before typed quantum updates, retaining overwrite history and untouched bytes. "
            "The mutable checked padding-free integer destination has a 1..64-byte remaining "
            "view with an in-view NUL; the appended prefix plus its NUL must fit. strcat needs "
            "an in-view source NUL within 64 bytes. strncat uses a constant append limit at most "
            "64, requires no NUL when that many source bytes exist, and always terminates the "
            "result without padding its tail. Known nonvolatile CPU sources use guarded once-only "
            "captures. Potential overlap is rejected conservatively. Local destination-return "
            "borrows and discarded call sequences are supported. Dynamic pointers/counts, "
            "general layouts/ABI and further lifetime contexts remain pending; explicit gate, "
            "history and expression budgets apply. Full coherent/inverse, sanitizer and GCC "
            "qualification is deferred; delivery examples are finite evidence only.",
        )
    if header == "string.h" and name in STRING_COPIES:
        return (
            "partial",
            ["P", "R"],
            "M4/M7",
            [],
            "String copies lower to typed quantum updates on mutable checked padding-free "
            "integer object bytes, preserving untouched bytes and retaining overwrite history. "
            "strcpy copies through a NUL that must fit both checked source and destination; "
            "its full remaining source view is at most 64 bytes. strncpy uses a constant "
            "count at most 64, zero-pads after NUL, and permits a nonterminated source when "
            "n readable bytes exist. Known nonvolatile CPU sources use once-only guarded "
            "byte captures. Overlapping potential byte ranges are rejected conservatively. "
            "Local destination-return borrows and discarded-call sequences are supported. "
            "Quantum-to-CPU writes, dynamic addresses/counts, general layouts, indirect ABI "
            "and further return consumers remain pending. Gate/history budgets apply. Full "
            "state, inverse, sanitizer and compiler-context qualification is deferred; the "
            "delivery example is finite evidence, not complete C11 support.",
        )
    if header == "string.h" and name in STRING_SPANS:
        return (
            "partial",
            ["P", "R"],
            "M4/M7",
            [],
            "Input-preserving strspn/strcspn expression adapters return size_t prefix lengths "
            "on nonempty checked padding-free integer byte views, each at most 64 bytes. "
            "Both arguments require an in-view NUL; set bytes after NUL do not contribute. "
            "Known nonvolatile CPU objects use once-only byte/termination captures retained "
            "for inverses. Classical calls and function addresses keep libc ABI. "
            "Dynamic extents/addresses, general layouts and indirect quantum calls remain "
            "pending. Simulator/gate budgets apply. The delivery example is separate from "
            "the deferred full state, inverse, sanitizer and GCC context qualification; "
            "implementation status does not certify those pending tests.",
        )
    if header == "string.h" and name in STRING_SEARCHES:
        return (
            "partial",
            ["P", "R"],
            "M4/M7",
            [],
            "Direct null and same-object address comparisons, plus same-object pointer "
            "subtractions, lower to input-preserving Boolean/integer expressions without "
            "materializing or measuring a returned pointer. Checked "
            "padding-free integer object-byte views and known nonvolatile CPU snapshots "
            "are supported within the 64-byte synthesis bound. memchr requires a constant "
            "checked count and converts the pure character argument to unsigned char; "
            "strchr/strrchr convert to target char and include the terminating NUL. "
            "String views require an in-view NUL; strpbrk excludes it and an empty strstr "
            "needle succeeds. Relative byte positions distinguish first and last occurrence; "
            "NULL subtraction and ordering have no defined C meaning and use a domain guard. "
            "Local immutable character/void-pointer borrows save a once-only quantum selector "
            "and provenance; aliases share it. Character dereference and constant indexing "
            "read current source bytes while captured positions remain fixed. Pointer writes, "
            "rebinding, general pointer arithmetic/ABI, dynamic extents and further consumers "
            "remain pending. Native read validation and complete quantum qualification remain "
            "deferred where not covered by the delivery examples. "
            "Delivery examples do not certify the deferred coherent/inverse, sanitizer "
            "or complete GCC context qualification. Resource budgets apply.",
        )
    if header == "string.h" and name in {"strlen", "strcmp"}:
        return (
            "partial",
            ["P", "R"],
            "M4/M7",
            ["tests/validate_strings.c", "tests/validate_bindings.c", "tests/validate_library.c"],
            "Input-preserving strlen/strcmp on checked padding-free integer object-byte views. "
            "Each full remaining object view must be nonempty and at most 64 bytes. "
            "strlen returns size_t; strcmp returns the first differing unsigned-byte difference "
            "and direct comparisons with zero use Boolean predicates. Selected evaluated "
            "domains need a NUL inside checked storage. Identical strcmp views fold to zero "
            "on the defined string domain and make no invalid-input diagnostic promise. "
            "Mutable known CPU objects supply once-only byte and termination snapshots, "
            "retained by saved inverses. Classical calls and function addresses keep libc ABI; "
            "the header alias preserves ordinary calls before GCC builtin folding. Quantum "
            "indirect calls, dynamic extents/addresses, volatile/atomic CPU access, general "
            "quantum layouts and pre-GCC folding provenance remain pending. The generic "
            "missing-NUL diagnostic currently names the internal division-by-zero domain guard, "
            "not a division in the source program. Dedicated string diagnostics remain open. "
            "Expression, gate, 128-wire representation and 24-wire native simulation budgets "
            "are separate from the frontend byte bound. No implicit measurement/reset occurs.",
        )
    if header == "string.h" and name == "strncmp":
        return (
            "partial",
            ["P", "R"],
            "M4/M7",
            [
                "tests/validate_strncmp.c",
                "tests/validate_byte_snapshots.c",
                "tests/validate_qasm.c",
            ],
            "Input-preserving bounded unsigned-character comparison stops at a first difference "
            "or an equal NUL. Padding-free integer object storage, checked constant byte views, "
            "overlap and readonly quantum borrows are supported. Known fixed-size CPU objects "
            "supply immutable constants or once-only current byte snapshots; CPU storage can "
            "include integer, floating and aggregate object representations. CPU bytes after "
            "the first classical NUL are not loaded. Saved inverses retain captured values "
            "after CPU mutation. The result is the exact first differing unsigned-byte difference; "
            "direct relations against zero use Boolean predicates. All n bytes must fit both "
            "checked objects even if a particular input terminates earlier. The header alias "
            "keeps normal calls visible before GCC folding; classical calls retain libc behavior. "
            "Dynamic extents/addresses, volatile/atomic CPU access, general quantum layouts, indirect quantum "
            "ABI and complete pre-GCC source provenance remain pending. The 64-byte frontend "
            "bound is separate from expression, gate, physical-qubit and simulator budgets. "
            "No implicit measurement or reset is introduced.",
        )
    if header == "string.h" and name == "memcmp":
        return (
            "partial",
            ["P", "R"],
            "M4/M7",
            ["tests/validate_memcmp.c", "tests/validate_byte_snapshots.c", "tests/validate_qasm.c"],
            "Input-preserving comparison of padding-free integer object bytes in target byte order; "
            "constant checked byte views, partial elements and overlap are supported. Known "
            "fixed-size CPU objects supply immutable constants or once-only current byte snapshots, "
            "including floating and aggregate CPU representations. Saved inverses retain captured "
            "values after CPU mutation. This does not implement quantum floating or aggregate layouts. "
            "The result is the first differing unsigned-byte difference; "
            "direct comparisons against zero lower to Boolean lexical predicates. Pure classical "
            "calls retain libc behavior. The header alias preserves normal calls before GCC's "
            "one-byte builtin folding. Dynamic extents/addresses, volatile/atomic CPU access, general "
            "quantum layouts and indirect quantum calls remain pending. The frontend bound is 64 compared "
            "bytes, with separate expression, physical-qubit, simulator and gate resource bounds. "
            "No implicit measurement or reset is introduced.",
        )
    if header == "string.h" and name == "memset":
        return (
            "partial",
            ["P", "R"],
            "M4/M7",
            [
                "tests/validate_memset.c",
                "tests/memset_state.inc",
                "tests/memset_effects.inc",
                "tests/validate_memory_returns.c",
                "tests/memory_return_state.inc",
                "tests/validate_memory_sequences.c",
                "tests/memory_sequence_state.inc",
                "tests/validate_qasm.c",
            ],
            "Checked padding-free 8/16/32/64-bit integer storage accepts constant in-bounds byte "
            "views up to 64 bytes, including partial elements and fixed arrays. The value is "
            "converted to an unsigned byte and captured once before all destination writes. "
            "Supported sequenced quantum effects use the existing expression scheduler; "
            "supported classical arguments are captured outside coherent control. Unknown "
            "overwritten values retain history; independent known-basis destinations can reuse "
            "their wires. Coherent conditions preserve inactive values without an implicit "
            "measurement/reset. Zero length still evaluates the argument and checks reachable "
            "arithmetic definedness. Destination returns support local typed/void checked borrows, "
            "constant byte views and supported address relations without exposing CPU placeholders. "
            "General returned-pointer consumers/indirect ABI, runtime counts, dynamic "
            "memory, general layouts, volatile/atomic access and CPU-loop history remain pending. "
            "Dense state/inverse checks, complete labeled controlled paths, native integrations, "
            "diagnostics, independent exports and wide generation have separate proof scopes. "
            "Expression, gate, physical-wire and simulator budgets apply separately. "
            "Ordinary CPU calls retain classical libc behavior; security-specific interfaces "
            "remain classical and are not adapted by this rule.",
        )
    if header == "string.h" and name in MEMORY_COPY:
        return (
            "partial",
            ["P", "R", "S"],
            "M4/M7",
            [
                "tests/validate_memcopy.c",
                "tests/validate_memory_returns.c",
                "tests/memory_return_state.inc",
                "tests/validate_memory_sequences.c",
                "tests/validate_qasm.c",
            ],
            "Checked fixed integer storage supports whole-element basis copying into known-basis "
            "destinations, including coherent control and read-before-write memmove overlap; "
            "exact memmove aliases are identity. Source/destination entanglement is not state cloning. "
            "Overwritten history, byte views/padding, dynamic extents, mixed CPU boundaries and "
            "general returned-pointer consumers remain pending. Local typed/void destination-return "
            "borrows and supported address relations are implemented. Logical synthesis is table-free and linear. "
            "Exact universal tensor-product cloning of an arbitrary unknown state is the incompatible context, "
            "not copying basis values or the whole library interface.",
        )
    if kind == "profile-macro" or name in ("NDEBUG", "static_assert"):
        return (
            "metadata-only",
            [],
            "M1",
            [],
            "Classical feature/preprocessing or constant-expression facility; no quantum runtime operation or implementation credit.",
        )
    if kind in ("macro", "constant", "pragma", "object"):
        # Including/expanding a macro is never credited as a quantum adapter.
        if header in (
            "ctype.h",
            "tgmath.h",
            "fenv.h",
            "math.h",
            "complex.h",
            "stdatomic.h",
            "stdarg.h",
            "assert.h",
            "setjmp.h",
        ):
            classes, milestone, contract = FAMILY_CONTRACTS[header]
            return (
                "pending",
                classes,
                milestone,
                [],
                contract + " Macro availability is not quantum implementation.",
            )
        return (
            "metadata-only",
            [],
            "M1",
            [],
            "Classical preprocessing/metadata or CPU object; no quantum operation is implied. Runtime object access requires the family contract.",
        )
    if kind == "type" and (
        name in INTEGER_TYPES or re.fullmatch(r'u?int(?:_least|_fast)?\d+_t', name)
    ):
        return (
            "partial",
            ["P", "R"],
            "M1/M2",
            ["tests/test_native.c"],
            "GCC integer representation can qualify supported scalar storage; target precision, promotions, layouts and resource bounds still need per-type evidence.",
        )
    classes, milestone, contract = FAMILY_CONTRACTS.get(
        header,
        (
            ["P"],
            "M1/M4/M7",
            "The header's target type/layout/effect contract remains unimplemented for general quantum data.",
        ),
    )
    if name in ("malloc", "calloc", "aligned_alloc", "realloc", "free"):
        contract = (
            "Classical size/address metadata may own quantum registers after a checked allocation contract. "
            "Quantum sizes/addresses need bounded coherent allocation. Resizing preserves live/history data; "
            "free releases only proven zero storage or an explicitly requested destruction channel, never a silent reset."
        )
    elif name in ("memcpy", "memmove", "wmemcpy", "wmemmove"):
        contract = (
            "Checked classical pointers may select quantum storage. Zero-destination basis copy entangles "
            "source and destination; it is not a tensor-product clone. Overwrites/overlap need preserved "
            "history and a specified lifetime or an explicit destruction boundary."
        )
    elif header == "time.h" and name not in ("clock", "time", "timespec_get"):
        classes = ["P", "R"]
        contract = "Snapshot declared calendar/locale rules and bound the buffers; pure date conversions and formatting need reversible results, pointer outputs and retained overwritten state."
    elif header == "locale.h" and name == "localeconv":
        classes = ["P", "R"]
        contract = "Expose a declared immutable locale snapshot with checked pointer/layout metadata; this query does not itself change the classical locale."
    elif header == "stdio.h" and name in (
        "snprintf",
        "sprintf",
        "sscanf",
        "vsnprintf",
        "vsprintf",
        "vsscanf",
    ):
        classes = ["P", "R"]
        contract = "Bound memory buffers, format/parse domains and locale; use a typed variadic ABI, reversible byte outputs and retained overwritten state. Formatting in memory does not itself write to an external stream."
    elif header == "wchar.h" and not re.match(
        r"^(?:[fv]?wprintf|[fv]?wscanf|fgetw|fputw|fwide|getw|putw|ungetw|vfw|fw)", name
    ):
        classes = ["P", "R"]
        contract = "Define target wide-character layout, locale and bounded strings/conversion state. Memory-only operations need reversible outputs and checked aliases/history; they do not themselves perform stream I/O."
    return "pending", classes, milestone, [], "Not implemented: " + contract


def rebuild(reference):
    import pymupdf

    document = pymupdf.open(reference)
    # Locate the actual annex, rather than accepting its table-of-contents entry.
    start = next(
        i
        for i, p in enumerate(document)
        if "Annex B\n(informative)\nLibrary summary\nB.1 Diagnostics" in p.get_text()
    )
    end = next(
        i
        for i in range(start + 1, len(document))
        if "Annex C\n(informative)" in document[i].get_text()
    )
    raw = "\n".join(p.get_text() for p in document[start:end])
    lines = []
    furniture = re.compile(
        r"^(?:N1570|ISO/IEC 9899:201x|Committee Draft — April 12, 2011|Library summary|Annex B|\(informative\)|\d+|§B\.\d+)\s*$"
    )
    for line in raw.splitlines():
        line = line.strip()
        if (
            line
            and not furniture.fullmatch(line)
            and not re.search(r"Library summary\s+§B|§B\.\d+\s+Library summary", line)
        ):
            lines.append(line.replace("_ _", "__"))
    parts = re.split(r"(?m)^B\.(\d+) ([^\n]*<([^>]+)>[^\n]*)$", "\n".join(lines))
    interfaces = []
    for i in range(1, len(parts), 4):
        number, _, header, body = parts[i : i + 4]
        pending = False
        optional = False
        entries = []
        for line in body.splitlines():
            line = line.strip()
            if not line or line == "∗":
                continue
            if pending:
                pending = ";" not in line
                continue
            if "__STDC_WANT_LIB_EXT1__" in line:
                optional = True
            names = re.findall(r"\b([A-Za-z_]\w*)\s*\(", line)
            if names and not line.startswith("#") and " " in line.split("(")[0]:
                name = next((n for n in names if n not in ("void", "size_t")), names[0])
                entries.append((name, "macro" if name in MACRO_FUNCTIONS else "function", optional))
                pending = ";" not in line
            else:
                name = line.split("(")[0].replace("#define ", "")
                kind = "macro" if names or header == "tgmath.h" else "constant"
                if name.startswith("#pragma "):
                    name, kind = name.removeprefix("#pragma ").split(" on-off-switch")[0], "pragma"
                elif (
                    name.startswith("struct ")
                    or name.endswith("_t")
                    or name
                    in (
                        "FILE",
                        "memory_order",
                        "va_list",
                        "jmp_buf",
                        "once_flag",
                        "atomic_flag",
                        "complex",
                        "imaginary",
                    )
                    or name.startswith("atomic_")
                ):
                    kind = "type"
                elif name.startswith("__STDC_"):
                    kind = "profile-macro"
                elif name in ("stdin", "stdout", "stderr", "errno", "math_errhandling"):
                    kind = "object"
                entries.append((name, kind, optional and name != "__STDC_WANT_LIB_EXT1__"))
        # N1570 Annex B omits the alignof pair present in normative 7.15.
        if header == "stdalign.h":
            entries.extend([(n, "macro", False) for n in ("alignof", "__alignof_is_defined")])
        for name, kind, annex_k in entries:
            expanded = [name]
            if name in ("atomic_fetch_key", "atomic_fetch_key_explicit"):
                expanded = [name.replace("key", op) for op in ("add", "sub", "or", "xor", "and")]
            if header in ("stdint.h", "inttypes.h") and re.fullmatch(
                r"u?int(?:_least|_fast)?N_t|U?INT(?:_(?:LEAST|FAST))?N_(?:MIN|MAX|C)|(?:PRI|SCN)[diouxX](?:LEAST|FAST)?N",
                name,
            ):
                # The standard defines families, not literally intN_t/PRIdN.
                expanded = [
                    re.sub(r"N(?=_t|_(?:MIN|MAX|C)|$)", str(w), name, count=1)
                    for w in (8, 16, 32, 64)
                ]
            for symbol in expanded:
                quantum, classes, milestone, tests, contract = semantics(
                    header, symbol, kind, annex_k
                )
                interfaces.append(
                    {
                        "header": header,
                        "name": symbol,
                        "kind": kind,
                        "source_index": "B." + number,
                        "profile": "annex-k-optional" if annex_k else "core-or-target-conditional",
                        "classical": "GCC/libc target; not an exhaustive certification",
                        "quantum": quantum,
                        "restriction_classes": classes,
                        "milestone": milestone,
                        "tests": tests,
                        "contract": contract,
                    }
                )
    result = {
        "standard_reference": "https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf",
        "scope": "Named interfaces in N1570 Annex B plus normative stdalign supplement; integer families expanded for the current 8/16/32/64-bit target. Additional implementation-defined extended widths remain a target obligation.",
        "meaning": "This index is not a claim that the library is implemented or tested. Partial/type tests are finite evidence. Required per-interface positive, negative, coherent, inverse, GCC and integrated tests remain open where absent.",
        "interfaces": interfaces,
    }
    TARGET.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")


def audit(require_complete=False):
    data = json.loads(TARGET.read_text(encoding="utf-8"))
    rows = data["interfaces"]
    headers = {r["header"] for r in rows}
    assert headers == {h + ".h" for h in HEADERS}, "Missing or unexpected C11 headers"
    keys = [(r["header"], r["name"]) for r in rows]
    assert len(keys) == len(set(keys)), "Duplicate interface"
    for r in rows:
        assert r["quantum"] in {"pending", "partial", "implemented", "metadata-only"}
        assert set(r["restriction_classes"]) <= {"P", "R", "S"}
        assert r["contract"] and r["milestone"]
        assert all((ROOT / p).is_file() for p in r["tests"])
    assert ABS | MEMORY_COPY | STRING_SPANS | STRING_COPIES | STRING_SEARCHES | {
        "memset",
        "strlen",
        "strcmp",
    } <= {r["name"] for r in rows if r["quantum"] in {"partial", "implemented"}}
    for name in (
        "alignof",
        "__alignof_is_defined",
        "atomic_init",
        "memcpy",
        "memmove",
        "malloc",
        "free",
        "thrd_create",
        "c16rtomb",
        "va_copy",
        "SCNd8",
        "INTMAX_MIN",
        "INT8_MIN",
        "int_fast64_t",
    ):
        assert any(r["name"] == name for r in rows), name
    assert not any("fetch_key" in r["name"] or "intN_t" == r["name"] for r in rows)
    counts = collections.Counter(r["quantum"] for r in rows)
    print(
        json.dumps(
            {
                "index_status": "valid",
                "implementation_status": (
                    "incomplete" if counts["pending"] or counts["partial"] else "complete-in-index"
                ),
                "headers": len(headers),
                "interfaces": len(rows),
                "quantum_status": counts,
            },
            sort_keys=True,
        )
    )
    if require_complete and any(r["quantum"] in {"pending", "partial"} for r in rows):
        raise SystemExit(
            "Full quantum C11 library compatibility is not implemented: "
            f"{counts['pending']} pending and {counts['partial']} partial interfaces. "
            "Metadata availability and expected rejections do not implement these interfaces."
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--reference", type=Path, help="Regenerate from a local N1570 PDF; requires PyMuPDF"
    )
    parser.add_argument(
        "--require-complete",
        action="store_true",
        help="Fail unless the index records complete quantum library compatibility.",
    )
    args = parser.parse_args()
    if args.reference:
        rebuild(args.reference)
    audit(args.require_complete)
