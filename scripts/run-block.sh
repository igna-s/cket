#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

if [[ "${1:-01}" != 01 || $# -gt 1 ]]; then
    printf 'Usage: bash scripts/run-block.sh [01]\n' >&2
    exit 1
fi

[[ -x build/cket ]] || bash scripts/build.sh
destination=build/blocks/01
mkdir -p "$destination"
export CKET_SEED="${CKET_SEED:-7}"
names=(hello entanglement fourier phase_estimation functions modular_arithmetic coherent_accumulate quantum_buckets)

for name in "${names[@]}"; do
    printf '\nBlock 01: %s\n' "$name"
    build/cket "examples/$name.c" -O2 -o "$destination/$name"
    "$destination/$name"
    build/cket "examples/$name.c" -O2 --emit-qasm -o "$destination/$name.qasm"
    build/cket "examples/$name.c" -O2 --resources -o "$destination/$name.resources.txt"
done

printf '\nBlock 01 ready: %u executables, QASM circuits and resource reports in %s.\n' \
    "${#names[@]}" "$destination"
