#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

block="${1:-01}"
if [[ "$block" != 01 && "$block" != 02 || $# -gt 1 ]]; then
    printf 'Usage: bash scripts/run-block.sh [01|02]\n' >&2
    exit 1
fi

[[ -x build/cket ]] || bash scripts/build.sh
destination="build/blocks/$block"
mkdir -p "$destination"
export CKET_SEED="${CKET_SEED:-7}"
if [[ "$block" == 02 ]]; then
    names=(wide_integers wide_size)
    source_directory=examples/blocks/02
    export CKET_SIMULATOR=sparse
else
    names=(hello entanglement fourier phase_estimation functions modular_arithmetic coherent_accumulate quantum_buckets)
    source_directory=examples
fi

for name in "${names[@]}"; do
    printf '\nBlock %s: %s\n' "$block" "$name"
    build/cket "$source_directory/$name.c" -O2 -o "$destination/$name"
    "$destination/$name"
    build/cket "$source_directory/$name.c" -O2 --emit-qasm -o "$destination/$name.qasm"
    build/cket "$source_directory/$name.c" -O2 --resources -o "$destination/$name.resources.txt"
done

printf '\nBlock %s ready: %u executables, QASM circuits and resource reports in %s.\n' \
    "$block" "${#names[@]}" "$destination"
