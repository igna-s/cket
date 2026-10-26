#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

block="${1:-01}"
if [[ "$block" != 01 && "$block" != 02 && "$block" != 03 && "$block" != 04 && "$block" != 05 && "$block" != 06 || $# -gt 1 ]]; then
    printf 'Usage: bash scripts/run-block.sh [01|02|03|04|05|06]\n' >&2
    exit 1
fi

[[ -x build/cket ]] || bash scripts/build.sh
destination="build/blocks/$block"
mkdir -p "$destination"
export CKET_SEED="${CKET_SEED:-7}"
if [[ "$block" == 06 ]]; then
    names=(string_spans string_search search_positions saved_search string_copy string_concat controlled_concat byte_copy decimal_conversion search_write)
    source_directory=examples/blocks/06
    export CKET_SIMULATOR=sparse
elif [[ "$block" == 03 ]]; then
    names=(buffer_functions buffer_words cpu_vector_scan cpu_vector_return)
    source_directory=examples/blocks/03
    export CKET_SIMULATOR=sparse
elif [[ "$block" == 05 ]]; then
    names=(dynamic_heap coherent_heap heap_bytes heap_transfer heap_iterations fixed_buffers buffer_lengths borrow_cleanup heap_coherence fixed_coherence)
    source_directory=examples/blocks/05
    export CKET_SIMULATOR=sparse
elif [[ "$block" == 04 ]]; then
    names=(quantum_index)
    source_directory=examples/blocks/04
    export CKET_SIMULATOR=sparse
elif [[ "$block" == 02 ]]; then
    names=(wide_integers wide_size variable_shift shift_predicate)
    source_directory=examples/blocks/02
    export CKET_SIMULATOR=sparse
else
    names=(hello entanglement fourier phase_estimation functions modular_arithmetic coherent_accumulate quantum_buckets)
    source_directory=examples
fi

exports=0
for name in "${names[@]}"; do
    printf '\nBlock %s: %s\n' "$block" "$name"
    build/cket "$source_directory/$name.c" -O2 -o "$destination/$name"
    "$destination/$name"
    if [[ "$name" == cpu_vector_scan || "$name" == cpu_vector_return || "$name" == dynamic_heap || "$name" == coherent_heap || "$name" == heap_bytes || "$name" == heap_transfer || "$name" == heap_iterations || "$name" == fixed_buffers || "$name" == buffer_lengths || "$name" == heap_coherence || "$name" == fixed_coherence ]]; then
        printf 'Runtime CPU loop/heap: native executable; static QASM export is unsupported.\n'
        continue
    fi
    static_profile=()
    if [[ "$name" == decimal_conversion ]]; then
        static_profile=(--ctype-locale C)
        printf 'Static character profile: C (native execution captures its effective locale)\n'
    fi
    bindings="$source_directory/$name.bindings.json"
    if [[ -f "$bindings" ]]; then
        printf 'Static export uses classical bindings from %s\n' "$bindings"
        build/cket "$source_directory/$name.c" -O2 --emit-ir -o "$destination/$name.ir.json"
        build/cket_backend --emit-qasm "$destination/$name.ir.json" --bindings "$bindings" >"$destination/$name.qasm"
        build/cket_backend --resources "$destination/$name.ir.json" --bindings "$bindings" >"$destination/$name.resources.txt"
    else
        build/cket "$source_directory/$name.c" -O2 --emit-qasm "${static_profile[@]}" -o "$destination/$name.qasm"
        build/cket "$source_directory/$name.c" -O2 --resources "${static_profile[@]}" -o "$destination/$name.resources.txt"
    fi
    exports=$((exports + 1))
done

printf '\nBlock %s ready: %u executables, %u QASM circuits and resource reports in %s.\n' \
    "$block" "${#names[@]}" "$exports" "$destination"
