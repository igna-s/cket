# Integer conditional expressions

Supported pure integer arms of `condition ? yes : no` compile to a coherent selection. The typed IR
retains the result type and explicit conversions. A quantum condition does not implicitly become a
CPU value or a measurement. Ordinary classical C continues through GCC. General side effects,
pointers, floating results and unsupported consuming contexts retain their own pending contracts.

The arithmetic plan builds internal expressions per result bit. Identical arm expressions use the
same view; distinct constants use the condition or its complement. Other columns calculate one
private mux bit per distinct pair of arm expressions. Repeated sign-extension columns can therefore
use one private bit instead of a full-width temporary.

Each public destination bit still has its own wire. Controlled X gates copy computational-basis
values into those wires and can entangle them with their inputs; they do not clone arbitrary quantum
states. The recorded plan then runs backward to clear private work, preserving source data and
relative phases without implicit measurement or reset.

The old full-width temporary made a selection between `INT64_MIN` and `INT64_MAX` exceed the
128-qubit representation. The [current evidence](audit-evidence/selection-views-v5/README.es.md)
checks that direct selection, its remainder results, mutable CPU-divisor snapshots and actual
compiler scope inverses. It also checks full-byte coherent inputs with complete 64-bit results,
shared/complement columns, nested conditions and arithmetic arms: 768 complete complex states across
normal and ASan/UBSan, with ordinary C references and zero private wires. The maximum observed gate
width is 102 qubits. The proof snapshot is production-identical to the final revision; only the
nested-test budget differs, and both identities are archived.

The maintained matrix adds 288 finite domains over every byte and both control labels/polarities,
with signed/unsigned 8/32/64-bit results. Its gate-action walks are finite basis checks, not dense
wide-state simulations. Its `result width + 40` resource check applies only to those profiles.
Normal/sanitizer targeted runs pass 1195 expression, 55 shift, 990 division, 393 native and 271
in-place checks each. The changed validator is rebuilt, using byte-verified cores from prior fresh
builds with identical production sources. The separate broad regression rebuilds both
configurations.

Distinct columns can still need a full word of private bits, plus both arm calculations, live inputs
and the public result. The representation remains limited to 128 qubits and native dense simulation
to 24. This does not complete general C11 integer contexts or the quantum ABI.
