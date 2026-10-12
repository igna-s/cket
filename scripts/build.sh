#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
compiler="${CXX:-g++}"
gcc_compiler="${CC:-gcc}"
plugin_dir="$($gcc_compiler -print-file-name=plugin)"
plugin_headers="${GCC_PLUGIN_INCLUDE:-$plugin_dir/include}"
if [[ ! -f "$plugin_headers/gcc-plugin.h" ]]; then
    local_headers=".deps/gcc$plugin_dir/include"
    if [[ -f "$local_headers/gcc-plugin.h" ]]; then
        plugin_headers="$local_headers"
    else
        echo "Missing GCC plugin headers. Install matching gcc-plugin-dev, or set GCC_PLUGIN_INCLUDE." >&2
        exit 1
    fi
fi
mkdir -p build
extra=()
if [[ -d .deps/gcc/usr/include ]]; then
    extra+=(-I.deps/gcc/usr/include -I.deps/gcc/usr/include/x86_64-linux-gnu)
fi
"$compiler" -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-format-diag -fPIC -shared \
    -fno-rtti -I"$plugin_headers" "${extra[@]}" \
    src/cket_plugin.cpp -o build/cket_plugin.so
flags=(-std=c11 -O2 -Wall -Wextra -Werror -Isrc/engine)
"$gcc_compiler" "${flags[@]}" -c src/engine/json.c -o build/json.o
"$gcc_compiler" "${flags[@]}" -c src/engine/quantum.c -o build/quantum.o
"$gcc_compiler" "${flags[@]}" -c src/engine/verify.c -o build/verify.o
"$gcc_compiler" "${flags[@]}" -c src/engine/simulator.c -o build/simulator.o
ar rcs build/libcket_quantum.a build/json.o build/quantum.o build/verify.o build/simulator.o
"$gcc_compiler" "${flags[@]}" -c src/cket_runtime.c -o build/cket_runtime.o
"$gcc_compiler" "${flags[@]}" src/cket.c build/libcket_quantum.a -lm -o build/cket
"$gcc_compiler" "${flags[@]}" src/cket_backend.c build/libcket_quantum.a -lm -o build/cket_backend
"$gcc_compiler" "${flags[@]}" tests/test_native.c tests/validation.c build/libcket_quantum.a -lm -o build/cket_tests
"$gcc_compiler" "${flags[@]}" tests/test_examples.c tests/validation.c -o build/test_examples
"$gcc_compiler" "${flags[@]}" tests/validate_harness.c tests/validation.c -o build/validate_harness
"$gcc_compiler" "${flags[@]}" tests/validate_shifts.c tests/validation.c build/libcket_quantum.a -lm -o build/validate_shifts
"$gcc_compiler" "${flags[@]}" tests/validate_expressions.c tests/validation.c build/libcket_quantum.a -lm -o build/validate_expressions
"$gcc_compiler" "${flags[@]}" tests/validate_inplace.c tests/validation.c build/libcket_quantum.a -lm -o build/validate_inplace
"$gcc_compiler" "${flags[@]}" tests/validate_memcopy.c tests/validation.c build/libcket_quantum.a -lm -o build/validate_memcopy
"$gcc_compiler" "${flags[@]}" tests/validate_division.c tests/validation.c build/libcket_quantum.a -lm -o build/validate_division
"$gcc_compiler" "${flags[@]}" tests/validate_bindings.c tests/validation.c build/libcket_quantum.a -lm -o build/validate_bindings
"$gcc_compiler" -dumpfullversion >build/gcc-version.txt
echo "Built |c> native C driver, reversible backend, statevector motor and GCC plugin"
