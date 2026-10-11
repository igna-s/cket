# Contributing to `|c>`

`|c>` (`cket`) is an experimental compiler. Changes must preserve C promotions and conversions, evaluate effects once, and maintain register ownership, coherence, and ancilla cleanup. The [1.0 contract](docs/v1-plan.md) distinguishes release requirements from current capabilities.

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

The driver, backend, runtime, simulator, and test programs are C. The GCC plugin is C++17 because it uses GCC's internal API. Shell scripts coordinate builds and commands. Python is not required for building, execution, or testing.

The plugin needs headers matching the GCC version in use. The README explains `GCC_PLUGIN_INCLUDE` and the local dependency alternative.

## Validate a change

Run checks relevant to the behavior you changed. For changes to semantics, synthesis, or execution, use the native suite and sanitizers:

```bash
bash scripts/test.sh
bash scripts/sanitize-all.sh
```

Build the additional C validation tools once, then run the checks relevant to your change:

```bash
sudo apt-get install -y curl ca-certificates
bash scripts/build-validation.sh
build/fuzz_frontend
build/benchmark_synthesis
build/validate_qasm
```

`build-validation.sh` downloads a pinned, checksum-verified QuEST 3.7.0 source archive and builds its serial C backend under `build/validation-deps/`. QuEST provides an independent simulator for exported QASM; it is not linked into the compiler or application runtime. Its upstream license stays with the downloaded source. The frontend generator accepts `--seed N` and `--cases N` and retains failure reproductions in `build/frontend-failure.*`.

All validation programs are C. They preserve the five optimization levels, 24 arithmetic growth circuits, and 11 independently checked QASM circuits. Scripts store evidence in `build/`; CI attaches logs and failure reproductions.

To instrument these C tools, their native reference library, and QuEST with ASan/UBSan:

```bash
VALIDATION_SANITIZERS=1 bash scripts/build-validation.sh
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 build/fuzz_frontend-sanitized
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 build/benchmark_synthesis-sanitized
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 build/validate_qasm-sanitized
```

## Issues and pull requests

A bug report should include a minimal program, the exact command, GCC and `cket` versions, and expected and observed behavior. For phases and entanglement, provide amplitudes or a circuit that distinguishes phase errors from probability errors.

A PR should explain the problem, the resulting behavior, and the validation performed. When extending the accepted subset, update the README and the relevant contract in `docs/`. Document bounds, defensive limits, and any ownership or ABI changes. A conservative rejection or a test over a small domain does not establish general support.

Generated files, downloaded dependencies, binaries, and credentials must stay out of Git. Example QIR modules are tracked as source. The original commits retain the dates assigned when the repository was initialized; use actual dates for subsequent changes.

Write documentation and contributor-facing templates in English. The project is distributed under the [MIT license](LICENSE). For potential vulnerabilities, follow [SECURITY.md](SECURITY.md).
