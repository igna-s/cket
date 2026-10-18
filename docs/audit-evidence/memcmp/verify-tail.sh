#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
fuzz_options=(--seed 20261012 --cases 128)
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
