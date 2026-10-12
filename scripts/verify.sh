#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
bash scripts/build.sh 2>&1 | tee build/verify-build.log
bash scripts/test.sh 2>&1 | tee build/verify-native.log
build/validate_harness 2>&1 | tee build/verify-harness.log
build/validate_shifts 2>&1 | tee build/verify-shifts.log
build/validate_expressions 2>&1 | tee build/verify-expressions.log
build/validate_inplace 2>&1 | tee build/verify-inplace.log
build/validate_memcopy 2>&1 | tee build/verify-memcopy.log
bash scripts/test-c11-classical.sh 2>&1 | tee build/verify-classical.log
bash scripts/build-validation.sh 2>&1 | tee build/verify-validation-build.log
fuzz_options=(--seed "${CKET_FUZZ_SEED:-20261011}" --cases "${CKET_FUZZ_CASES:-24}")
build/fuzz_frontend "${fuzz_options[@]}" 2>&1 | tee build/verify-frontend.log
build/benchmark_synthesis 2>&1 | tee build/verify-growth.log
build/validate_qasm 2>&1 | tee build/verify-qasm.log
bash scripts/sanitize-all.sh 2>&1 | tee build/verify-sanitized.log
classical_san_root="${CKET_SAN_ROOT:-$PWD/build/sanitized}"
CKET_REAL_GCC="${CC:-gcc}" CC="$classical_san_root/build/gcc-wrapper" \
    CKET_DRIVER="$classical_san_root/build/cket" \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    bash scripts/test-c11-classical.sh 2>&1 | tee build/verify-classical-sanitized.log
VALIDATION_SANITIZERS=1 bash scripts/build-validation.sh 2>&1 | tee build/verify-validation-sanitized-build.log
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
build/fuzz_frontend-sanitized "${fuzz_options[@]}" 2>&1 | tee build/verify-frontend-sanitized.log
build/benchmark_synthesis-sanitized 2>&1 | tee build/verify-growth-sanitized.log
build/validate_qasm-sanitized 2>&1 | tee build/verify-qasm-sanitized.log
printf '%s\n' 'All compiler, runtime, example, synthesis and independent QASM checks passed.'
