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
build/validate_readonly_borrows 2>&1 | tee build/verify-readonly-borrows.log
build/validate_function_entries 2>&1 | tee build/verify-function-entries.log
procedure_return_proof="$(mktemp -d "$PWD/build/procedure-return-proof.XXXXXX")"
python3 scripts/validate-procedure-returns.py --output "$procedure_return_proof/normal" \
    2>&1 | tee build/verify-procedure-returns.log
coherent_return_proof="$(mktemp -d "$PWD/build/coherent-return-proof.XXXXXX")"
python3 scripts/validate-coherent-procedure-returns.py --output "$coherent_return_proof/normal" \
    2>&1 | tee build/verify-coherent-returns.log
build/validate_history 2>&1 | tee build/verify-history.log
build/validate_heap 2>&1 | tee build/verify-heap.log
build/validate_heap_identities 2>&1 | tee build/verify-heap-identities.log
build/validate_inactive_borrows 2>&1 | tee build/verify-inactive-borrows.log
build/validate_swap_rebind 2>&1 | tee build/verify-swap-rebind.log
build/validate_patches 2>&1 | tee build/verify-patches.log
fixed_patch_proof="$(mktemp -d "$PWD/build/fixed-patch-proof.XXXXXX")"
python3 scripts/validate-fixed-patches.py --output "$fixed_patch_proof/normal" --levels O0 O2 \
    2>&1 | tee build/verify-fixed-patch-states.log
string_write_proof="$(mktemp -d "$PWD/build/string-write-proof.XXXXXX")"
python3 scripts/validate-string-writes.py --output "$string_write_proof/normal" --levels O0 O2 \
    2>&1 | tee build/verify-string-write-states.log
heap_coherence_proof="$(mktemp -d "$PWD/build/heap-coherence-proof.XXXXXX")"
python3 scripts/validate-heap-coherence.py --output "$heap_coherence_proof/normal" --levels O0 O2 \
    2>&1 | tee build/verify-heap-coherence-states.log
rebound_borrow_proof="$(mktemp -d "$PWD/build/rebound-borrow-proof.XXXXXX")"
python3 scripts/validate-rebound-borrows.py --output "$rebound_borrow_proof/normal" --levels O0 O2 \
    2>&1 | tee build/verify-rebound-borrow-states.log
inactive_borrow_proof="$(mktemp -d "$PWD/build/inactive-borrow-proof.XXXXXX")"
python3 scripts/validate-inactive-borrows.py --output "$inactive_borrow_proof/normal" --levels O0 O2 \
    2>&1 | tee build/verify-inactive-borrow-states.log
heap_identity_proof="$(mktemp -d "$PWD/build/heap-identity-proof.XXXXXX")"
python3 scripts/validate-heap-identities.py --output "$heap_identity_proof/normal" --levels O0 O2 \
    2>&1 | tee build/verify-heap-identity-states.log
build/validate_runtime_versions 2>&1 | tee build/verify-runtime-versions.log
build/validate_runtime_memory 2>&1 | tee build/verify-runtime-memory-engine.log
runtime_memory_proof="$(mktemp -d "$PWD/build/runtime-memory-proof.XXXXXX")"
python3 scripts/validate-runtime-memory.py --output "$runtime_memory_proof/normal" --levels O0 O2 \
    2>&1 | tee build/verify-runtime-memory.log
coherent_store_proof="$(mktemp -d "$PWD/build/coherent-store-proof.XXXXXX")"
python3 scripts/validate-coherent-overwrites.py --output "$coherent_store_proof/normal" --levels O0 O2 \
    2>&1 | tee build/verify-coherent-overwrites.log
fixed_memory_proof="$(mktemp -d "$PWD/build/fixed-memory-proof.XXXXXX")"
python3 scripts/validate-fixed-memory-loops.py --output "$fixed_memory_proof/normal" --levels O0 O2 \
    2>&1 | tee build/verify-fixed-memory-loops.log
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
