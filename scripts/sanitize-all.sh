#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
real_cc="${CC:-gcc}"
real_cxx="${CXX:-g++}"
plugin_dir="$($real_cc -print-file-name=plugin)"
headers="${GCC_PLUGIN_INCLUDE:-$plugin_dir/include}"
if [[ ! -f "$headers/gcc-plugin.h" ]]; then
    headers=".deps/gcc$plugin_dir/include"
fi
if [[ ! -f "$headers/gcc-plugin.h" ]]; then
    echo "Missing matching GCC plugin headers" >&2
    exit 1
fi
san_root="${CKET_SAN_ROOT:-$PWD/build/sanitized}"
mkdir -p "$san_root/build"
cp cket "$san_root/cket"
for directory in include examples src tests; do
    # Reuse matching links: replacing existing DrvFS links can fail on WSL.
    if [[ ! "$san_root/$directory" -ef "$directory" ]]; then
        ln -sfn "$PWD/$directory" "$san_root/$directory"
    fi
done
extra=()
if [[ -d .deps/gcc/usr/include ]]; then
    extra+=(-I.deps/gcc/usr/include -I.deps/gcc/usr/include/x86_64-linux-gnu)
fi
flags=(-std=c11 -O1 -g -Wall -Wextra -Werror -Isrc/engine
    -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie)
"$real_cxx" -std=c++17 -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-diag \
    -fPIC -shared -fno-rtti -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"$headers" "${extra[@]}" src/cket_plugin.cpp -o "$san_root/build/cket_plugin.so"
for component in json quantum verify simulator; do
    "$real_cc" "${flags[@]}" -c "src/engine/$component.c" -o "$san_root/build/$component.o"
done
ar rcs "$san_root/build/libcket_quantum.a" "$san_root/build/json.o" \
    "$san_root/build/quantum.o" "$san_root/build/verify.o" "$san_root/build/simulator.o"
"$real_cc" "${flags[@]}" -c src/cket_runtime.c -o "$san_root/build/cket_runtime.o"
"$real_cc" "${flags[@]}" src/cket.c "$san_root/build/libcket_quantum.a" -lm -o "$san_root/build/cket"
"$real_cc" "${flags[@]}" src/cket_backend.c "$san_root/build/libcket_quantum.a" -lm -o "$san_root/build/cket_backend"
"$real_cc" "${flags[@]}" tests/test_native.c tests/validation.c "$san_root/build/libcket_quantum.a" -lm -o "$san_root/build/cket_tests"
"$real_cc" "${flags[@]}" tests/test_examples.c tests/validation.c -o "$san_root/build/test_examples"
"$real_cc" "${flags[@]}" tests/validate_harness.c tests/validation.c -o "$san_root/build/validate_harness"
"$real_cc" "${flags[@]}" tests/validate_shifts.c tests/validation.c "$san_root/build/libcket_quantum.a" -lm -o "$san_root/build/validate_shifts"
"$real_cc" "${flags[@]}" tests/validate_expressions.c tests/validation.c "$san_root/build/libcket_quantum.a" -lm -o "$san_root/build/validate_expressions"
"$real_cc" "${flags[@]}" tests/validate_inplace.c tests/validation.c "$san_root/build/libcket_quantum.a" -lm -o "$san_root/build/validate_inplace"
"$real_cc" "${flags[@]}" tests/validate_memcopy.c tests/validation.c "$san_root/build/libcket_quantum.a" -lm -o "$san_root/build/validate_memcopy"
"$real_cc" "${flags[@]}" tests/validate_memcmp.c tests/validation.c "$san_root/build/libcket_quantum.a" -lm -o "$san_root/build/validate_memcmp"
"$real_cc" "${flags[@]}" tests/validate_strncmp.c tests/validation.c "$san_root/build/libcket_quantum.a" -lm -o "$san_root/build/validate_strncmp"
"$real_cc" "${flags[@]}" tests/validate_strings.c tests/validation.c "$san_root/build/libcket_quantum.a" -lm -o "$san_root/build/validate_strings"
"$real_cc" "${flags[@]}" tests/validate_memset.c tests/validation.c "$san_root/build/libcket_quantum.a" -lm -o "$san_root/build/validate_memset"
"$real_cc" "${flags[@]}" tests/validate_memory_sequences.c tests/validation.c "$san_root/build/libcket_quantum.a" -lm -o "$san_root/build/validate_memory_sequences"
"$real_cc" "${flags[@]}" tests/validate_memory_returns.c tests/validation.c "$san_root/build/libcket_quantum.a" -lm -o "$san_root/build/validate_memory_returns"
"$real_cc" "${flags[@]}" tests/validate_pointer_differences.c tests/validation.c "$san_root/build/libcket_quantum.a" -lm -o "$san_root/build/validate_pointer_differences"
"$real_cc" "${flags[@]}" tests/validate_controlled_operands.c tests/validation.c -o "$san_root/build/validate_controlled_operands"
"$real_cc" "${flags[@]}" tests/validate_phase.c tests/validation.c "$san_root/build/libcket_quantum.a" -lm -o "$san_root/build/validate_phase"
"$real_cc" "${flags[@]}" tests/validate_byte_snapshots.c tests/validation.c "$san_root/build/libcket_quantum.a" -lm -o "$san_root/build/validate_byte_snapshots"
"$real_cc" "${flags[@]}" tests/validate_division.c tests/validation.c "$san_root/build/libcket_quantum.a" -lm -o "$san_root/build/validate_division"
"$real_cc" "${flags[@]}" tests/validate_bindings.c tests/validation.c "$san_root/build/libcket_quantum.a" -lm -o "$san_root/build/validate_bindings"
"$real_cc" "${flags[@]}" tests/validate_history.c tests/validation.c "$san_root/build/libcket_quantum.a" -lm -o "$san_root/build/validate_history"
"$real_cc" "${flags[@]}" tests/validate_ctype.c tests/validation.c "$san_root/build/libcket_quantum.a" -lm -o "$san_root/build/validate_ctype"
"$real_cc" "${flags[@]}" tests/validate_profiles.c tests/validation.c "$san_root/build/libcket_quantum.a" -lm -o "$san_root/build/validate_profiles"
"$real_cc" "${flags[@]}" tests/validate_numeric_locale.c tests/validation.c "$san_root/build/libcket_quantum.a" -lm -o "$san_root/build/validate_numeric_locale"
"$real_cc" "${flags[@]}" tests/validate_library.c tests/validation.c -o "$san_root/build/validate_library"
"$real_cc" -dumpfullversion >"$san_root/build/gcc-version.txt"
cat >"$san_root/build/gcc-wrapper" <<'WRAPPER'
#!/usr/bin/env bash
set -euo pipefail
compiler="${CKET_REAL_GCC:-gcc}"
# GCC itself is not instrumented. Load ASan before its instrumented plugin.
# GCC retains allocations until exit, so do not attribute its leaks to the plugin.
export LD_PRELOAD="$($compiler -print-file-name=libasan.so)"
export ASAN_OPTIONS="${ASAN_OPTIONS:-}:detect_leaks=0"
exec "$compiler" -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie "$@"
WRAPPER
chmod +x "$san_root/build/gcc-wrapper"
export CKET_REAL_GCC="$real_cc"
export CC="$san_root/build/gcc-wrapper"
export UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1"
"$san_root/build/cket_tests" "$san_root"
"$san_root/build/validate_harness"
"$san_root/build/validate_shifts"
"$san_root/build/validate_expressions"
"$san_root/build/validate_inplace"
"$san_root/build/validate_memcopy"
"$san_root/build/validate_memcmp"
"$san_root/build/validate_strncmp"
"$san_root/build/validate_strings"
"$san_root/build/validate_memset"
"$san_root/build/validate_memory_sequences"
"$san_root/build/validate_memory_returns"
"$san_root/build/validate_pointer_differences"
"$san_root/build/validate_controlled_operands"
"$san_root/build/validate_phase"
"$san_root/build/validate_byte_snapshots"
"$san_root/build/validate_division"
"$san_root/build/validate_bindings"
"$san_root/build/validate_history"
LSAN_OPTIONS="${LSAN_OPTIONS:+$LSAN_OPTIONS:}suppressions=$PWD/tests/glibc-locale.lsan:print_suppressions=0" \
    "$san_root/build/validate_ctype"
"$san_root/build/validate_profiles"
LSAN_OPTIONS="${LSAN_OPTIONS:+$LSAN_OPTIONS:}suppressions=$PWD/tests/glibc-locale.lsan:print_suppressions=0" \
    "$san_root/build/validate_numeric_locale"
"$san_root/build/validate_library"
exec "$san_root/build/test_examples"
