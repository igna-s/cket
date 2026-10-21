#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

block="${1:-01}"
if [[ "$block" != 01 && "$block" != 02 && "$block" != 06 || $# -gt 1 ]]; then
    printf 'Usage: bash scripts/run-block.sh [01|02|06]\n' >&2
    exit 1
fi

[[ -x build/cket ]] || bash scripts/build.sh
destination="build/blocks/$block"
mkdir -p "$destination"
export CKET_SEED="${CKET_SEED:-7}"
if [[ "$block" == 06 ]]; then
    names=(string_spans string_search search_positions saved_search string_copy string_concat controlled_concat byte_copy)
    source_directory=examples/blocks/06
    export CKET_SIMULATOR=sparse
elif [[ "$block" == 02 ]]; then
    names=(wide_integers wide_size variable_shift shift_predicate)
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
    bindings="$source_directory/$name.bindings.json"
    if [[ -f "$bindings" ]]; then
        printf 'Static export uses classical bindings from %s\n' "$bindings"
        build/cket "$source_directory/$name.c" -O2 --emit-ir -o "$destination/$name.ir.json"
        build/cket_backend --emit-qasm "$destination/$name.ir.json" --bindings "$bindings" >"$destination/$name.qasm"
        build/cket_backend --resources "$destination/$name.ir.json" --bindings "$bindings" >"$destination/$name.resources.txt"
    else
        build/cket "$source_directory/$name.c" -O2 --emit-qasm -o "$destination/$name.qasm"
        build/cket "$source_directory/$name.c" -O2 --resources -o "$destination/$name.resources.txt"
    fi
done

printf '\nBlock %s ready: %u executables, QASM circuits and resource reports in %s.\n' \
    "$block" "${#names[@]}" "$destination"
