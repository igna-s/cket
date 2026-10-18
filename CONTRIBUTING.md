# Contributing to `|c>`

`|c>` (`cket`) is an experimental compiler. Changes must preserve C promotions and conversions,
evaluate effects once, and maintain register ownership, coherence, and ancilla cleanup. The
[1.0 contract](docs/v1-plan.md) distinguishes release requirements from current capabilities.

## Set up the environment

Use Linux or WSL. For the reference configuration on Ubuntu 22.04:

```bash
sudo apt-get update
sudo apt-get install -y gcc-11 g++-11 gcc-11-plugin-dev binutils
export CC=gcc-11
export CXX=g++-11
bash scripts/build.sh
bash scripts/test.sh
```

The driver, backend, runtime, simulator, and test programs are C. The GCC plugin is C++17 because it
uses GCC's internal API. Shell scripts coordinate builds and commands. The compiler/runtime suite
needs C/C++ and Bash; optional formatting, inventory, paper and supplementary evidence audits use
Python.

The plugin needs headers matching the GCC version in use. The README explains `GCC_PLUGIN_INCLUDE`
and the local dependency alternative.

## Validate a change

The complete verification command rebuilds the project, runs every example at five GCC optimization
levels, checks the native suite and independent QASM, and repeats the tests with ASan/UBSan:

```bash
sudo apt-get install -y curl ca-certificates
bash scripts/verify.sh
```

Evidence is saved to `build/verify-*.log`. For a larger reproducible frontend campaign, set
`CKET_FUZZ_CASES=128` and optionally `CKET_FUZZ_SEED=20261012`.

Run checks relevant to the behavior you changed. For changes to semantics, synthesis, or execution,
use the native suite and sanitizers:

```bash
bash scripts/test.sh
bash scripts/sanitize-all.sh
```

Build the additional C validation tools once, then run the checks relevant to your change:

```bash
sudo apt-get install -y curl ca-certificates locales
bash scripts/build-validation.sh
build/fuzz_frontend
build/benchmark_synthesis
build/validate_qasm
build/validate_library
build/validate_profiles
build/validate_numeric_locale
build/validate_memcmp
build/validate_strncmp
build/validate_byte_snapshots
build/validate_memset
build/validate_memory_sequences
```

`build-validation.sh` downloads a pinned, checksum-verified QuEST 3.7.0 source archive and builds
its serial C backend under `build/validation-deps/`. QuEST provides an independent simulator for
exported QASM; it is not linked into the compiler or application runtime. Its upstream license stays
with the downloaded source. The frontend generator accepts `--seed N` and `--cases N` and retains
failure reproductions in `build/frontend-failure.*`.

Each generated expression is compared with C over all 256 byte inputs at every optimization level.
The library boundary validator checks unsupported contexts across 210 interfaces with builtins
enabled and disabled, after an ordinary C syntax check; its expected rejections do not implement
those interfaces. The profile validator separately tests branch proofs, phase preservation and
inverses.

The numeric-locale validator checks decimal parsing, controlled/global QASM output and preservation
of global/thread locales, including output failures. Locale fixtures also need the existing
`localedef`/locale-source dependencies. The CI time budget accommodates the expanded byte corpus and
library boundary matrix; individual subprocess deadlines still apply.

The compiler/runtime validation programs built by these scripts are C. They exercise five
optimization levels, 24 arithmetic growth circuits, and independent QASM state/channel comparisons.
Scripts store evidence in `build/`; CI attaches logs and failure reproductions. The
[repository audit](docs/repository-audit.md) records the counts and scope of the latest completed
campaign.

`test.sh` also runs `build/test_examples`: all 36 examples, expected outputs, static export support
or rejection, both dynamic array indices, interactive boundary and invalid inputs at five
optimization levels, stochastic contracts over 16 seeds, correlated Bell outcomes, CLI errors, and
execution outside the repository. `examples/types.c` deliberately demonstrates a 32-bit quantum
integer: export succeeds, but execution must report the 24-qubit simulator limit.

Use `bash scripts/run-examples.sh --all` to run the 35 simulator-compatible programs, or pass
selected names such as `for_sum fibonacci maximum_scan`. The [examples guide](examples/README.md)
documents outputs and input formats.

To instrument these C tools, their native reference library, and QuEST with ASan/UBSan:

```bash
VALIDATION_SANITIZERS=1 bash scripts/build-validation.sh
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 build/fuzz_frontend-sanitized
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 build/benchmark_synthesis-sanitized
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 build/validate_qasm-sanitized
```

## Formatting and readability

Use four spaces for C, C++, Python and Bash, and two for JSON/YAML. Keep related steps on separate
lines and wrap prose and ordinary code at 100 columns. Long literal strings, URLs and Markdown
tables may exceed that limit. The [clang-format configuration](.clang-format) preserves include
order and literal strings; [EditorConfig](.editorconfig) supplies editor defaults.

Optional development tools format repository-owned source files, shell scripts, Python tools,
documentation and code examples embedded in documentation:

```bash
bash scripts/setup-format.sh
.deps/format-venv/bin/python scripts/format.py
.deps/format-venv/bin/python scripts/format.py --check
```

The formatter verifies C tokens, preprocessor boundaries and Python syntax trees before writing
those files. Run the relevant build and behavioral checks after formatting; these guards do not
prove semantic equivalence. Downloaded dependencies and historical evidence retain their original
bytes. Paper manuscripts preserve the layout tied to their audited PDFs; typesetting changes use the
separate [paper workflow](docs/paper/README.md).

## Issues and pull requests

A bug report should include a minimal program, the exact command, GCC and `cket` versions, and
expected and observed behavior. For phases and entanglement, provide amplitudes or a circuit that
distinguishes phase errors from probability errors.

A PR should explain the problem, the resulting behavior, and the validation performed. When
extending the accepted subset, update the README and the relevant contract in `docs/`. Document
bounds, defensive limits, and any ownership or ABI changes. A conservative rejection or a test over
a small domain does not establish general support.

Generated build files, downloaded dependencies, executables, and credentials must stay out of Git.
Example QIR modules are tracked as source. Audited paper PDFs in `output/pdf/` and their original
figures and reproducibility evidence in `docs/paper/` are publication artifacts tracked alongside
their editable sources. The original commits retain the dates assigned when the repository was
initialized; use actual dates for subsequent changes.

Write documentation and contributor-facing templates in English. The project is distributed under
the [MIT license](LICENSE). For potential vulnerabilities, follow [SECURITY.md](SECURITY.md).
