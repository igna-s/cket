#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
bash scripts/build.sh 2>&1 | tee build/verify-build.log
bash scripts/test.sh 2>&1 | tee build/verify-native.log
build/validate_harness 2>&1 | tee build/verify-harness.log
build/validate_shifts 2>&1 | tee build/verify-shifts.log
build/validate_sparse 2>&1 | tee build/verify-sparse.log
build/validate_expressions 2>&1 | tee build/verify-expressions.log
build/validate_inplace 2>&1 | tee build/verify-inplace.log
build/validate_memcopy 2>&1 | tee build/verify-memcopy.log
build/validate_memcmp 2>&1 | tee build/verify-memcmp.log
build/validate_strncmp 2>&1 | tee build/verify-strncmp.log
build/validate_strings 2>&1 | tee build/verify-strings.log
build/validate_memset 2>&1 | tee build/verify-memset.log
build/validate_memory_sequences 2>&1 | tee build/verify-memory-sequences.log
build/validate_memory_returns 2>&1 | tee build/verify-memory-returns.log
build/validate_pointer_differences 2>&1 | tee build/verify-pointer-differences.log
build/validate_controlled_operands 2>&1 | tee build/verify-controlled-operands.log
build/validate_phase 2>&1 | tee build/verify-phase.log
build/validate_byte_snapshots 2>&1 | tee build/verify-byte-snapshots.log
build/validate_division 2>&1 | tee build/verify-division.log
build/validate_bindings 2>&1 | tee build/verify-bindings.log
build/validate_history 2>&1 | tee build/verify-history.log
build/validate_heap 2>&1 | tee build/verify-heap.log
build/validate_ctype 2>&1 | tee build/verify-ctype.log
build/validate_profiles 2>&1 | tee build/verify-profiles.log
build/validate_numeric_locale 2>&1 | tee build/verify-numeric-locale.log
build/validate_library 2>&1 | tee build/verify-library.log
bash scripts/test-c11-classical.sh 2>&1 | tee build/verify-classical.log
bash scripts/build-validation.sh 2>&1 | tee build/verify-validation-build.log
fuzz_options=(--seed "${CKET_FUZZ_SEED:-20261011}" --cases "${CKET_FUZZ_CASES:-24}")
build/fuzz_frontend "${fuzz_options[@]}" 2>&1 | tee build/verify-frontend.log
build/benchmark_synthesis 2>&1 | tee build/verify-growth.log
build/validate_qasm 2>&1 | tee build/verify-qasm.log
bash scripts/test-locale-sanitizer.sh 2>&1 | tee build/verify-locale-sanitizer.log
bash scripts/sanitize-all.sh 2>&1 | tee build/verify-sanitized.log
classical_san_root="${CKET_SAN_ROOT:-$PWD/build/sanitized}"
CKET_REAL_GCC="${CC:-gcc}" CC="$classical_san_root/build/gcc-wrapper" \
    CKET_DRIVER="$classical_san_root/build/cket" \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    bash scripts/test-c11-classical.sh 2>&1 | tee build/verify-classical-sanitized.log
VALIDATION_SANITIZERS=1 bash scripts/build-validation.sh 2>&1 | tee build/verify-validation-sanitized-build.log
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
# Validators resolve their compiler/backend relative to their own executable.
# Stage them beside the instrumented tools so sanitizing the harness also
# exercises the instrumented plugin, runtime and generated C executables.
for validator in fuzz_frontend benchmark_synthesis validate_qasm qasm_samples; do
    cp "build/$validator-sanitized" "$classical_san_root/build/"
done
export CKET_REAL_GCC="${CC:-gcc}"
export CC="$classical_san_root/build/gcc-wrapper"
"$classical_san_root/build/fuzz_frontend-sanitized" "${fuzz_options[@]}" 2>&1 | tee build/verify-frontend-sanitized.log
"$classical_san_root/build/benchmark_synthesis-sanitized" 2>&1 | tee build/verify-growth-sanitized.log
cmp build/synthesis-growth.csv "$classical_san_root/build/synthesis-growth.csv"
LSAN_OPTIONS="${LSAN_OPTIONS:+$LSAN_OPTIONS:}suppressions=$PWD/tests/glibc-locale.lsan:print_suppressions=0" \
    "$classical_san_root/build/validate_qasm-sanitized" 2>&1 | tee build/verify-qasm-sanitized.log
printf '%s\n' 'All compiler, runtime, example, synthesis and independent QASM checks passed.'
