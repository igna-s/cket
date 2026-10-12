# Validation failures and semantic rejections

An expected negative case must establish its contract violation. An unsuccessful command is not
enough: a missing tool, timeout, crash, compiler internal error or sanitizer report can have the
same exit status. This closes part of the evidence requirements in [M8](c11-roadmap.md), while
complete C11/library implementation remains open.

## Reproduced false positives

The previous memory-copy validator accepted any nonzero result. A controlled reproduction returned
124 after a timeout, 1 after a synthetic ASan report, and 127 after an execution failure. All three
satisfied its expected-rejection predicate. The native validator required a `|c>` marker, but did
not require the ordinary rejection status or separately exclude internal failure reports.

The shared runner now aborts the suite on process signals, timeout, setup/execution failure, and
captured compiler/sanitizer fault reports. This check applies even when a tool exits zero, as a
recovering UBSan process can do. A failed invocation also cannot retain a previous invocation's
semantic diagnostic: its text log is truncated before launch.

The runner's diagnostic and artifact readers accept bounded text, at most 64 MiB, and reject
embedded NUL bytes. Binary output needs a separate reader. Reports outside the captured streams and
new diagnostic formats are not covered by this detector; the tested GCC/tool configuration remains
part of its scope.

## Rejection contracts

`validation_rejected` requires normal rejection status 1, a `|c>` diagnostic, no captured fault
report, and the requested semantic fragment on the same diagnostic line. A forwarded signal-style
status does not satisfy that contract. Numbered GCC source excerpts are excluded: quoting
`runtime error:` in a source string is not a sanitizer report, and quoting `|c>` is not a compiler
diagnostic.

The native compiler/runtime fixtures now use the shared bounded runner and rejection predicate.
Their general negatives require a proper C Ket diagnostic; some still need narrower per-case
messages. Memory-copy cases additionally declare a specific message for all 17 negative programs,
checked at five optimization levels with builtins enabled/disabled: 170 diagnostic checks. Their
positive state/value/generation evidence remains separate.

## Fault injection and reproducibility

`tests/validate_harness.c` has 18 subprocess tests. It verifies ordinary success and matching
semantic rejection; unrelated errors, wrong diagnostic lines and forwarded high exit codes; benign
sanitizer initialization; synthetic ASan, fatal-signal, leak, recovering UBSan and compiler-internal
reports; an actual process signal and timeout; setup and executable failures; quoted source text; a
source-only fake diagnostic; and a NUL followed by a hidden fault report.

Synthetic reports test the detector; they are not claims of discovered ASan memory bugs in the
compiler. The signal and timeout cases exercise real process supervision. Parent probes inspect the
runner's failure directly so an expected self-test failure cannot be mistaken for an ordinary
semantic rejection. Tests run normally and instrumented with ASan/UBSan through `scripts/verify.sh`,
and their logs are part of the archived campaign.

```bash
export CC=gcc-11 CXX=g++-11
export CKET_FUZZ_CASES=128 CKET_FUZZ_SEED=20261012
bash scripts/verify.sh
python3 scripts/archive-audit.py --log build/audit/verify-harness-full.log
```

The archive requires two complete sets of results, including the 18 runner tests, and refuses
visible infrastructure/timeout/compiler/sanitizer failures. Passing these checks remains finite
evidence: it neither certifies all C11 contexts nor supplies a general proof of GCC optimization,
library behavior, circuit correctness or absence of bugs.
