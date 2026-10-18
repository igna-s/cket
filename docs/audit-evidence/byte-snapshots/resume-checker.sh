#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/byte-snapshot-prototype"
record_exit() {
    status="$?"
    trap - EXIT
    python3 ../record-byte-snapshot-exit.py "$status"
    exit "$status"
}
trap record_exit EXIT
export CC=gcc-11 CXX=g++-11 CKET_FUZZ_CASES=128 CKET_FUZZ_SEED=20261012
python3 ../check-byte-snapshot-resume.py
printf '%s\n' 'Resumed verification: unchanged frozen 129-input source state; interrupted byte-snapshot suite restarted in full.'
build/validate_byte_snapshots 2>&1 | tee build/verify-byte-snapshots-resumed.log
build/validate_division 2>&1 | tee build/verify-division.log
build/validate_bindings 2>&1 | tee build/verify-bindings.log
build/validate_history 2>&1 | tee build/verify-history.log
build/validate_ctype 2>&1 | tee build/verify-ctype.log
build/validate_profiles 2>&1 | tee build/verify-profiles.log
build/validate_numeric_locale 2>&1 | tee build/verify-numeric-locale.log
build/validate_library 2>&1 | tee build/verify-library.log
bash scripts/test-c11-classical.sh 2>&1 | tee build/verify-classical.log
bash scripts/build-validation.sh 2>&1 | tee build/verify-validation-build.log
fuzz_options=(--seed "$CKET_FUZZ_SEED" --cases "$CKET_FUZZ_CASES")
build/fuzz_frontend "${fuzz_options[@]}" 2>&1 | tee build/verify-frontend.log
build/benchmark_synthesis 2>&1 | tee build/verify-growth.log
build/validate_qasm 2>&1 | tee build/verify-qasm.log
bash scripts/test-locale-sanitizer.sh 2>&1 | tee build/verify-locale-sanitizer.log
bash scripts/sanitize-all.sh 2>&1 | tee build/verify-sanitized.log
classical_san_root="$PWD/build/sanitized"
CKET_REAL_GCC="$CC" CC="$classical_san_root/build/gcc-wrapper" \
    CKET_DRIVER="$classical_san_root/build/cket" \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    bash scripts/test-c11-classical.sh 2>&1 | tee build/verify-classical-sanitized.log
VALIDATION_SANITIZERS=1 bash scripts/build-validation.sh 2>&1 | tee build/verify-validation-sanitized-build.log
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
for validator in fuzz_frontend benchmark_synthesis validate_qasm qasm_samples; do
    cp "build/$validator-sanitized" "$classical_san_root/build/"
done
export CKET_REAL_GCC="$CC"
export CC="$classical_san_root/build/gcc-wrapper"
"$classical_san_root/build/fuzz_frontend-sanitized" "${fuzz_options[@]}" 2>&1 | tee build/verify-frontend-sanitized.log
"$classical_san_root/build/benchmark_synthesis-sanitized" 2>&1 | tee build/verify-growth-sanitized.log
cmp build/synthesis-growth.csv "$classical_san_root/build/synthesis-growth.csv"
LSAN_OPTIONS="suppressions=$PWD/tests/glibc-locale.lsan:print_suppressions=0" \
    "$classical_san_root/build/validate_qasm-sanitized" 2>&1 | tee build/verify-qasm-sanitized.log
python3 ../check-byte-snapshot-resume.py
printf '%s\n' 'All compiler, runtime, example, synthesis and independent QASM checks passed.'
