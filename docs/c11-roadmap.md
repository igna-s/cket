# Roadmap to complete C11 quantum compatibility

The final objective is a quantum semantics for **every C11 construction and standard-library
interface**. The current integer subset is an intermediate implementation. Passing its tests does
not complete that objective.

Implementation now ships as [usable incremental deliveries](entregas-incrementales.es.md), starting
with a small integer/Boolean package. Delivery counts do not measure C11 coverage; the feature
contracts and inventory below retain their independent status.

Coverage means an implemented transformation with tested preconditions, or a diagnostic for a
specifically justified incompatible context. A missing implementation cannot be closed as a physical
impossibility. A documented proposal cannot be counted as an implementation.

The working language reference is the public
[WG14 N1570 committee draft](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf), dated
April 12, 2011. It is a draft reference, not a claim of certification against the published ISO
text. The target profile must identify GCC, ABI, integer representation, floating formats, libc and
optional C11 facilities.

## Implementation order

The user requested this order on October 19, 2026: complete the commonly used types and operations
before less frequently used facilities. This is a practical work order, not a measured usage ranking
or a claim that the first groups are already complete.

1. Close the everyday integer foundation: `_Bool`/`bool`, `char`, signed/unsigned `char`, `short`,
   `int`, `long`, `long long`, their ordinary aliases, `size_t` and `ptrdiff_t`. Check
   initialization, promotions, mixed signedness, casts, comparisons, arithmetic, shifts, assignment
   results and cleanup. The declared target's widths and plain-`char` signedness must remain
   explicit. The
   [small-constant remainder implementation](c11-division.md#remainder-by-a-small-constant) now
   avoids the previous `int % 3` workspace failure and has finite boundary/control/inverse evidence.
   This closes that arithmetic case, not the whole integer foundation. The
   [selection-view implementation](c11-selections.md) also removes the full-word temporary that
   blocked direct selection between signed 64-bit limits. The
   [sparse runtime](simulacion-dispersa.es.md) now permits actual native execution of wide registers
   with bounded occupied support. General division, general wide expressions and the remaining
   consuming contexts still require work. [Variable shifts](c11-shifts.md) now avoid full stage
   registers, allowing full 32-bit circuits and selected wide native executions within the existing
   physical representation.
1. Close the common consuming contexts: conditions, loops, direct functions, arrays, basic structs,
   checked pointers, and frequent memory/string operations. The `memset` temporary-lifetime
   correction now has a
   [146-input targeted integration](audit-evidence/known-fill-cleanup-v2/README.es.md); its full
   regression remains pending. Keep ordinary C behavior intact.
1. Implement the next common capabilities, including quantum `float`/`double`, their conversions and
   ordinary arithmetic, dynamic storage and general pointer/function use. Publish precise contracts
   for classical I/O boundaries. Existing CPU floating angles for `phase` do not implement quantum
   floating registers.
1. Address less common facilities afterward: `long double`, complex arithmetic, general bitfields
   and unions, wide-character/locale interfaces, advanced floating environments, atomics and
   threading. These remain in the complete objective; their lower priority does not close them.

Cybersecurity and security-specific interfaces remain classical under the user's standing scope.
Each implementation must have evidence for its types, effects and consuming contexts; adding a
declaration or passing a small fixture cannot mark a whole type or header complete.

## Status and restriction classes

Every inventory entry needs a classical status, a quantum status, evidence, remaining work and a
restriction class. Multiple classes may apply to different contexts of the same construct:

| Class | Meaning                                         | What closes it                                                                 |
| ----- | ----------------------------------------------- | ------------------------------------------------------------------------------ |
| P     | Pending implementation or incomplete analysis   | Implementation, integration tests and independent evidence                     |
| R     | Resource requirement or explicit bound          | A declared bound, resource accounting and both acceptance and exhaustion tests |
| S     | Semantic incompatibility in a specified context | A precise argument, diagnostic and an explicit alternative where one exists    |

Examples: quantum floating point, unions, bitfields and dynamic allocation are P, with resource
considerations R. Their absence is not a no-cloning result. Erasing an arbitrary state while
preserving coherence and leaving no history is S. Coherent indexing is P/R; using a CPU pointer as
though its address were in superposition is S for that particular pointer representation. Recursion
is P/R when bounded. Unbounded computation cannot be promised as a finite static circuit, but that
does not rule out classical control or bounded recursion.

“Classical: GCC/libc” means delegated to the selected implementation, not that this project has
exhaustively validated the entire C standard or every libc interface. Quantum operations must not
corrupt ordinary C behavior.

## Required semantic model

The following contracts are design requirements until their implementation and tests are linked from
the inventory. Existing supported integer operations retain their present documented contracts.

### Values, conversions and definedness

Quantum data encode C values in computational-basis bit patterns. The complex amplitudes of that
superposition are distinct from a C `_Complex` value. Conversions, promotions and sequencing occur
before reversible synthesis. Arithmetic must be defined on the reachable support of every evaluated
branch. Undefined C behavior has no invented quantum result; implementation-defined choices must be
recorded in the target profile. Logical short circuiting controls effects and arithmetic on the
appropriate branch, without a hidden measurement. `sizeof`, `_Alignof`, `_Generic` and
`_Static_assert` inspect types or classical metadata, rather than amplitudes; VLA extent evaluation
keeps its ordinary C side effects and timing.

Every assignment operator, prefix/postfix increment and conversion needs a value result as well as
its storage effect. The old value produced by postfix increment is a retained quantum value, not a
CPU integer. A compiler must not replace an expression-valued effect with a statement or remove a
classical cast.

### Destructive assignment and history

An injective update may act in place. A noninjective update must preserve enough information to make
the complete transformation reversible. The partial implementation uses versioned ownership:
evaluate the right-hand side once, retain the old storage as history, produce the new value in fresh
storage, and update the owner and checked aliases consistently. Ancillas used to compute the new
value are uncomputed with their still-available inputs.

The old value itself cannot generally be erased by that cleanup. The lifetime and qubit cost of
retained history must be visible in diagnostics and resource reports. A proposed history capability
transfers or releases it only after a proved inverse, known basis restoration, or an explicitly
requested destructive channel. Syntax for this capability is not implemented yet. Scope exit and
`free` must not silently measure, reset or trace out dirty history. The
[implemented unconditional scalar path](c11-history.md) retains unknown root history as part of the
joint output until context end, with current/peak qubits in resource reports. Derived versions and
known-basis roots have checked cleanup. Structured coherent joins now have the
[selected-value implementation](c11-control.md). Source-level history capabilities, general CFG
joins, runtime iterations and general memory updates remain P/R. Structured CPU scalar joins now
have a [gate-free ownership transport contract](c11-cpu-control.md). Measure/reset channels remain
explicit source operations.

### Floating and complex data

The target must select representations for `float`, `double`, `long double` and their complex
counterparts. A first reference profile can use binary32, binary64 and the declared host long-double
format; C11 does not require every implementation to use those formats. Padding, trap
representations and implementation-defined behavior require separate layout rules.

Reversible arithmetic preserves input encodings and computes the specified rounded output into a
destination. Rounding loses information in the output function, not in the complete input-preserving
circuit. Private work is then inverted. The contract must cover each rounding mode, subnormals,
signed zero, infinities, quiet/signaling NaNs, payload policy, overflow, underflow, inexact and
invalid results. Float-to-integer conversion requires its representable domain; integer-to-float
rounding is retained explicitly. Complex values encode two real components and follow C's
conversions and branch cuts; C does not provide ordered comparisons for complex numbers.

The proposed first quantum numeric profile is **P, not implemented**:

| C type                              | Proposed basis-value encoding                                          | Layout obligation                                                        |
| ----------------------------------- | ---------------------------------------------------------------------- | ------------------------------------------------------------------------ |
| `float`                             | Binary32: sign 1, exponent 8, fraction 23; bias 127                    | 32 meaningful bits; target byte order and character views                |
| `double`                            | Binary64: sign 1, exponent 11, fraction 52; bias 1023                  | 64 meaningful bits; target byte order and character views                |
| `long double` on this x86-64 target | x87 extended: sign 1, exponent 15, explicit significand 64; bias 16383 | 80 meaningful bits inside a 16-byte C object; padding tracked separately |
| Each `_Complex` counterpart         | Ordered pair of the corresponding real encodings                       | Both components and their padding/alias views; a compound ABI            |

The long-double layout follows the observed GCC 11.4.0 target macros (`__SIZEOF_LONG_DOUBLE__=16`,
`__LDBL_MANT_DIG__=64`, `__LDBL_MAX_EXP__=16384`) and
[GCC's x86 ABI documentation](https://gcc.gnu.org/onlinedocs/gcc-11.4.0/gcc/x86-Options.html). Other
targets and flags require another declared profile. The current scalar IR width limit of 64 bits
does not already implement the 80-bit format.

For binary32/64, normal values use the implicit leading one, exponent zero encodes signed
zero/subnormals, and the maximal exponent encodes infinity or NaN. x87's explicit integer bit and
noncanonical encodings require their own validation; they must not be treated as binary64. Object
padding is not extra numeric precision and cannot be silently erased when observable byte/history
storage is live.

The proposed arithmetic rounds the exact basic-operation result in one of four declared modes:
nearest with ties to even, toward zero, toward positive infinity, or toward negative infinity. It
preserves signed zeros and gradual underflow. Each operation produces a separate reversible result
and five status bits (invalid, division by zero, overflow, underflow, inexact); sticky updates
retain the old flags. Underflow detection is specified after rounding for this profile. Signaling
NaNs set invalid before being quieted. A proposed canonical quiet-NaN output policy is an explicit
extension where C leaves payload/sign choices open; original encodings remain preserved in the
inputs or history.

Ordered real comparisons with NaNs are false, inequality with a NaN is true, and Boolean conversion
tests against zero; signaling behavior is reported in the status contract. Complex equality compares
both components, and its branch cuts/special cases follow the separately selected Annex G profile.
Math-library functions must publish accuracy and flag contracts per interface; the presence of a
format does not imply correctly rounded transcendental functions. M6 must contrast bits and flags
against independent rounded references before crediting these rules as implemented.

Floating exception flags may depend on a quantum input. They must be quantum result/status registers
or explicitly measured output, not an implicit write to CPU `fenv` or `errno`. NaN canonicalization
and destructive flag updates need retained history. Whether the optional IEC 60559 annexes are
supported must be specified separately. A floating encoding or an exported circuit alone does not
establish rounded arithmetic correctness.

### Control and functions

Classical control executes on the CPU with an explicit control-flow graph (CFG), ownership joins,
loop invariants and exit cleanup. Coherent control uses quantum predicates, controlled effects and
recorded branch/iteration history when needed. `switch`, fall-through, `break`, `continue`, `goto`
and early returns require these analyses; they are not intrinsically impossible. Coherent external
I/O without an explicit measurement/interface is incompatible with ordinary CPU I/O. Differing
coherent path lengths require a bound and defined inactive operations, not sampling a path.

A typed ABI must describe borrowed/consumed parameters, alias permissions, effects, result ownership
and cleanup. Scalar and product results, separate translation units, indirect calls and bounded
recursion require that ABI. A coherent indirect call needs controlled dispatch among compatible
unitary targets; a classical function pointer remains a CPU address. Variadic quantum arguments
require typed metadata and promotion rules. Tail calls and nonlocal jumps must preserve or
explicitly transfer cleanup obligations.

### Memory and storage

A classical pointer to quantum storage identifies an owner or checked view; it does not encode a
coherent address. Its provenance, bounds, alignment, aliasing and lifetime must survive dynamic
selection, assignments and calls. A coherent address uses a quantum index/register and reversible
multiplexing or a declared memory backend. No constant-cost QRAM assumption may be hidden in
resource counts.

Arrays, VLAs, flexible members, structures, unions and bitfields require a layout contract. Union
members are views of shared storage, not independent registers. Bitfields require offset/width
metadata, C promotions and preserved history for destructive partial writes. Effective type,
padding, trap values, common initial sequences and character access must follow the selected C
profile. Pointer-to-integer conversions cannot expose amplitudes as CPU bits.

`malloc`, `calloc`, `realloc` and `aligned_alloc` with classical sizes manage quantum ownership
through a quantum allocation interface. Their C-compatible surface and failure behavior must be
specified and implemented; applying ordinary libc allocation to placeholder bytes does not allocate
qubits. `calloc` initializes basis zero. Growth preserves existing state; shrink/free requires clean
released regions or an explicit destruction channel. A coherent size reserves a bounded maximum and
retains its allocation/length metadata; it cannot change the Hilbert-space dimension invisibly
between branches.

Globals, static storage, TLS and `extern` owners need initialization, synchronization,
interprocedural lifetime tracking and explicit shutdown. `volatile`, `_Atomic` and `restrict` need
quantum contracts. Current global runtime state is not a thread-safe quantum execution context.

### Standard-library interaction

Classical arguments continue to use ordinary libc. Pure functions on quantum values need
input-preserving circuits, conversions, domain checks and error results. Impure functions need an
explicit CPU/channel boundary.

`memcpy`/`memmove` cannot clone an arbitrary quantum state. The partial
[implemented extension](c11-memcopy.md) copies computational-basis values with correlations for
superposed inputs. Known-basis destinations use controlled translation and copy gates. Unknown
overwrites now retain history; a checked byte route supports partial integer words, different
integer sizes and known CPU sources. Read-before-write ordering preserves supported overlap. General
layouts, dynamic extents, cyclic aliases and unrestricted returned-pointer consumers remain pending.
The new extension has finite delivery evidence; full qualification is deferred.
[Local checked destination returns and same-object distances](c11-memory-returns.md) have partial
adapters. The partial [memset adapter](c11-memset.md) now preserves unknown overwritten state as
history and applies fixed byte ranges with once-only value evaluation.
[Discarded memory-call sequences](c11-memory-sequences.md) compose checked calls in comma and lazy
expressions. General pointer results, counts and layouts remain pending. The partial
[memcmp adapter](c11-memcmp.md) now compares checked integer object bytes without modifying its
inputs. [CPU snapshots](c11-byte-snapshots.md) now capture selected current bytes of known classical
storage once and retain them for inverse cleanup. General addresses, search/sort and comparator
effects still need implementations, rather than CPU reads of markers. The partial
[strncmp adapter](c11-strncmp.md) adds NUL-stopping bounded character comparison on the same checked
integer byte views. Its full signed result is computed coherently; CPU suffix loads stop at that
operand's own NUL. General strings, volatile/atomic captures and dynamic ranges remain open.
Cybersecurity remains classical and outside the quantum adaptation scope.

Formatted I/O, strings sent to external systems, signals, clocks, locale, environment calls and
pseudorandom CPU state are classical effects. Quantum values reach them through explicit
measurement. Parsing classical input into a new basis state is different from coherent parsing of
quantum character data. Math/complex functions need specified accuracy, rounding and special values.
Atomics and concurrency require linear ownership or checked sharing plus a defined ordering model;
quantum computation must not race on the global engine.

## Stages and evidence gates

No stage closes merely because existing examples remain green. Each gate needs new integration
programs, negative contracts, resource cases and authoritative evidence tied to the exact tested
source state.

| Stage | Deliverable and dependencies                                                       | Required gate                                                                                                                                                                                      |
| ----- | ---------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| M0    | Full C11 inventory and audited baseline; preserve historical paper evidence        | Every language/library category has a current status, P/R/S reason and next stage; confirmed bugs have reproductions and regressions                                                               |
| M1    | Target/type/effect model and GCC qualification, before wider transforms            | Promotions, all conversions, const/alias rules, definedness and evaluation-once at five optimization levels; ordinary C differentials                                                              |
| M2    | Complete integer expressions, all assignments and versioned history; depends on M1 | Signed/unsigned corners, variable shifts, prefix/postfix values, noninjective writes, clean inverse, retained-history lifetime/cost; no-table circuits beyond 12 input bits                        |
| M3    | CFG and classical/coherent branches, loops and jumps; depends on M1/M2             | Zero iterations, fall-through, all exit edges, early returns, nested coherent/classical control, bounded coherent iterations and resource exhaustion                                               |
| M4    | Layout, dynamic memory and storage; depends on M1/M2/M3                            | Arrays/VLA/products/unions/bitfields, aliases/provenance, allocation failure, resize/free, globals/TLS, byte access and dirty-release rejection                                                    |
| M5    | Typed ABI and full function composition; depends on M3/M4                          | Separate translation units, product returns, compatible indirect calls, variadics, recursive bounds, alias rejection and inverse/control across calls                                              |
| M6    | Floating/complex values and mathematical circuits; depends on M1/M2                | Independent rounded reference, specials and exception results, all formats/modes, complex branch cuts, combined computations and inverses; resource-only exports labeled separately                |
| M7    | Every C11 library family, atomics and concurrency; depends on M4/M5/M6             | Per-interface semantic contract, positive implemented adapters, explicit effect boundaries, concurrent ownership/order tests and combinations of math/memory/control/I/O                           |
| M8    | Coherent addresses and scalable integrated synthesis; depends on M3/M4/M5          | Multiplexed read/write, address/value history, no implicit QRAM cost, global gate/workspace/time budgets, multi-feature programs without truth tables                                              |
| M9    | Coverage sign-off for the declared C11 target; depends on all previous stages      | No P entry hidden as S, no untested implementation counted complete, every justified S context diagnosed, reproducible external state/channel checks, GCC-version differentials and audited papers |

M6 and parts of M4/M5 may proceed in parallel once their dependencies exist. This table specifies
dependencies, not an invented delivery estimate.

M2 now includes [preserved-input variable shifts](c11-shifts.md) with a table-free barrel network,
clean private inverse, interval-definedness checks, small complete-state comparisons, wide exports
and GCC composition tests. This advances one gate; remaining value/effect contexts, general write
history, all assignments and the rest of M2 remain open.

M2 also includes [pure integer expression synthesis](c11-expressions.md): negation, complement,
logical short circuiting and conditional selection with branch-local definedness and clean private
inversion. Its original 97 checks distinguish complete small-state comparisons, C integrations and
wide circuit generation. Three additional QuEST states exercise guarded division and conditional
unary operations. These finite gates do not close sequenced effects, history, all types or general
control flow.

M2 now also includes [in-place bit/affine certificates](c11-inplace.md), with zero private qubits,
proper Boolean truth conversion and an enforced table policy. Its 271 finite checks cover complete
small states, inverse, C composition and wide generation; three more QuEST states contrast the
emitted updates. General noninjective history, support extensions, remaining value/effect contexts,
all assignments and the rest of M2 remain open.

M2/M3/M4 now include [versioned assignments](c11-history.md). Its baseline checks distinguish
complete phased domains and derived inverses, lifetime/ownership guards, compiled C integrations and
specific pending-contract diagnostics. Three further table-free QuEST cases contrast derived
cleanup, unknown retained outputs and CPU snapshot inverses. Four further QuEST cases cover
[coherent joins](c11-control.md), self controls, nesting and CPU predicate snapshots. General CFG
joins, runtime iterations, general memory, history handles and remaining value/effect contexts
remain open. [Structured CPU scalar joins](c11-cpu-control.md) add gate-free owner transport and
deferred cleanup, with 46 engine contracts, 55 C programs, 120 native paths and 240 selected-trace
states. Three further QuEST cases contrast concrete CPU decisions; unbound CPU templates remain
distinct from verified circuits.

M2 also includes [ordered scalar value-producing updates](c11-effect-values.md): assignment values,
prefix/postfix new/old values and comma ordering, with protected reads and retained versions. Its
220 additional checks distinguish 120 native results, 60 complete phased states and 40 diagnostics
at five GCC levels. Three further QuEST fixtures contrast postfix, prefix and comma exports. Mixed
CPU expression effects, general memory/float observations, effectful function returns, full
source-level sequencing and the rest of C11 remain P/R; this advances M2 without closing it.

M2 now also includes [explicit integer expression observation](c11-observations.md): typed old/new
results, predicate channels that preserve coherence within fibers, a retained compute inverse and
clean temporary release. The 730 additional checks distinguish native C, conditional full states,
lifetime guards and pending contracts. Three further QuEST channel fixtures compare interference and
postfix ownership. Full source-level GCC value/effect preservation, general memory/float
observation, mixed CPU effects and the rest of C11 remain open.

M2/M3 also include [selected scalar effects in lazy expressions](c11-lazy-values.md). Saved
decisions control conditional/logical effects and their values, with intermediate history cleaned by
inverses. The 573 additional checks distinguish ordinary C references, combined programs, complete
phased states, direct fixed-predicate contracts and specific diagnostics. Three further QuEST states
contrast these exports without tables. CPU effects, general aggregate/ABI lifetimes, runtime loop
history and full source-level GCC sequencing remain P/R; this does not complete either milestone.

## Search, external validation and GCC guarantees

M8 now includes the [failure-classification audit](audit-validation.md): 18 subprocess tests, fatal
infrastructure failures, bounded text without NUL truncation, and specific diagnostics for all 170
negative memory cases. Native negatives share the supervised runner, though some still require
narrower per-case messages. Diagnostic-format coverage and the rest of M8 remain open.

M4/M7 now include the partial memory-copy contracts above. The 414 finite checks distinguish
complete states/inverse and C execution from wide generation, and four further QuEST cases contrast
the emitted circuits. History, byte layouts, dynamic memory, pointer-result ABI and the remaining
library families stay open; two partial adapters do not complete M4, M7 or C11.

`find` promises one solution; `find_known` receives a correct count, with a pristine independent
uniform power-of-two input and a defined pure Boolean predicate. Neither counts the domain to check
the promise. Wrong counts need documented behavior and diagnostic limits, not a claimed success
guarantee.

For `search`, let `N >= 2`, `1 <= M <= N`, `theta = asin(sqrt(M/N))` and `K = ceil(sqrt(N))`. Half
the trials sample directly; the other half choose `j` uniformly in `0..K-1`. For `M >= N/2`, direct
sampling alone contributes at least `1/4`. Otherwise the average amplified success is
`1/2 - sin(4*K*theta)/(4*K*sin(2*theta))`, from
[Boyer et al., Lemma 2](https://arxiv.org/pdf/quant-ph/9605034). Here
`K*sin(2*theta) >= 2*sqrt(1-1/N) >= sqrt(2)`, so that average exceeds `1/4` and the mixture exceeds
`1/8`. Independent trials therefore give failure at most `(7/8)^attempts`. This derivation applies
to ideal independent uniform randomness and correct oracles, not every deterministic PRNG seed or
arbitrary floating-point accumulated error. `M=0` exhausts the budget and proves no nonexistence
claim. Successful candidates are checked classically.

Validation must separately label synthesis/export, native execution, exact small domain/reference
comparison, and external simulator comparison. The QuEST suite contains sixty-four state cases and
six sampled channels, not every generated circuit. Extend it to integrated library, memory, history
and control workloads; state checks alone cannot validate intermediate measurement/reset channels.

GCC transformations before `PLUGIN_PRE_GENERICIZE` are currently trusted. Optimization differentials
and sanitizers are finite evidence, not a compiler correctness proof. Qualification/effect
preservation must be checked against multiple declared GCC targets and against ordinary C references
for defined behavior. Resource reports and papers must identify their exact implementation, scripts,
seeds, transcript criteria and unsupported features. The historical 0.4.0 paper remains a study of
the intermediate subset.

M2/M4/M7 now include [integer-division products and projections](c11-division.md) for the four
standard signed division functions. Constant power-of-two division has table-free signed rounding
corrections. The 990 checks distinguish phased/inverse state validation, ordinary C references, wide
generation and diagnostic/resource contracts. Effectful quantum arguments, general quantum product
ABI and larger complete-product storage remain P/R; this does not close those milestones.

M2/M4/M7 also include [once-only CPU bindings and classical product preparation](c11-bindings.md).
Concrete circuit definitions retain parameter snapshots for their inverse. Structural template
validation does not certify numerical definedness or generated circuits. Nested classical
structure-return initializers now retain evaluations and values instead of silently zero-filling
their quantum fields. The 519 finite binding checks and two additional QuEST states provide evidence
for these contracts; general quantum product ABI, coherent CPU effects and full C11 coverage remain
open.

M2/M7 also include [immutable character adapters](c11-ctype.md) for all fourteen C11 interfaces. The
1815 finite checks cover three byte locales, masks, phased states, mapping bits, scalar effects,
functions, memory, loops, explicit observation, thread locales, saved inverses, unitary modules and
classical GCC differentials. Four QuEST fixtures contrast exported character circuits. Indirect
calls, automatic per-capture static locale traces, concurrency and source provenance before GCC
folding remain P/R; invalid arguments retain the C undefined-domain contract (S). This is partial
library coverage, not completion of M7.

## Completion rule

M4/M7 now also include bounded [`strlen` and `strcmp`](c11-strings.md), with 460 maintained checks
per configuration and 640 separate independent compiled-circuit states across normal and ASan/UBSan.
The historical 144-input integration adds supported deferred CPU phase arithmetic and guarded pure
CPU loads. Its larger local regression completed in two segments, with 80 commands and 665 fuzz
compilations per configuration, as preserved in the
[later completion archive](audit-evidence/common-remainder-v2/README.es.md). Its
[remote CI](audit-evidence/integrated144-remote-ci/README.es.md) passed all 28 normal/sanitizer
suites on the same 144 compiler inputs. Remote fuzzing uses 145 compilations per configuration; the
expanded local campaign uses 665. Their seeds differ and their evidence remains separate. These
additions do not close the string, memory, floating-point or general library milestones.

The historical 146-input integration adds cleanup for known `memset` temporaries, with six targeted
normal/sanitizer suites, 3528 independent compiled-scope states and 20 separate root contract
executions. Its full campaign and remote CI remain pending. A 64-bit partial-fill experiment failed
at the physical budget; its checked 8/16/32-bit prefix remains separate from that failure. This does
not close M2/M4 or the lifetime work. The later 148-input integration implements small-constant
remainder, and the historical 151-input integration adds selection views. Its 1195 expression checks
include the original 97, 810 remainder and 288 selection domains. The separate broad151 campaign
remains separate; no earlier broad result certifies those changed inputs.

The project goal remains open until the inventory has implemented, validated coverage or a reasoned
incompatible-context contract for every C11 item of the declared target. Resource limits must have
explicit tests and alternatives; they cannot serve as a blanket reason to omit operators or library
families. The present subset and its passing tests are evidence for M0, not completion of M9.
