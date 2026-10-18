#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/memory-sequence-prototype"
export CC=gcc-11 CXX=g++-11 CKET_FUZZ_CASES=128 CKET_FUZZ_SEED=20261012
san_root="$PWD/build/sanitized"
export CKET_REAL_GCC=gcc-11 CC="$san_root/build/gcc-wrapper"
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
printf '%s\n' 'Resume: 134 frozen inputs match; rerunning the whole interrupted sanitizer library suite, then remaining suites.'
"$san_root/build/validate_library" 2>&1 | tee build/verify-library-sanitized-resumed.log
"$san_root/build/test_examples" 2>&1 | tee build/verify-examples-sanitized-resumed.log
CKET_DRIVER="$san_root/build/cket" bash scripts/test-c11-classical.sh 2>&1 | tee build/verify-classical-sanitized.log
CC=gcc-11 VALIDATION_SANITIZERS=1 bash scripts/build-validation.sh 2>&1 | tee build/verify-validation-sanitized-build.log
for validator in fuzz_frontend benchmark_synthesis validate_qasm qasm_samples; do
    cp "build/$validator-sanitized" "$san_root/build/"
done
"$san_root/build/fuzz_frontend-sanitized" --seed "$CKET_FUZZ_SEED" --cases "$CKET_FUZZ_CASES" 2>&1 | tee build/verify-frontend-sanitized.log
"$san_root/build/benchmark_synthesis-sanitized" 2>&1 | tee build/verify-growth-sanitized.log
cmp build/synthesis-growth.csv "$san_root/build/synthesis-growth.csv"
LSAN_OPTIONS="suppressions=$PWD/tests/glibc-locale.lsan:print_suppressions=0" \
    "$san_root/build/validate_qasm-sanitized" 2>&1 | tee build/verify-qasm-sanitized.log
printf '%s\n' 'All compiler, runtime, example, synthesis and independent QASM checks passed.'
