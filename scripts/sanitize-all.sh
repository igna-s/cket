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
for directory in include examples; do
    # Reuse matching links: replacing existing DrvFS links can fail on WSL.
    if [[ ! "$san_root/$directory" -ef "$directory" ]]; then
        ln -sfn "$PWD/$directory" "$san_root/$directory"
    fi
done
extra=()
if [[ -d .deps/gcc/usr/include ]]; then
    extra+=(-I.deps/gcc/usr/include -I.deps/gcc/usr/include/x86_64-linux-gnu)
fi
flags=(-std=c11 -O1 -g -Wall -Wextra -Werror -Wno-misleading-indentation -Isrc/engine
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
"$real_cc" "${flags[@]}" tests/test_native.c "$san_root/build/libcket_quantum.a" -lm -o "$san_root/build/cket_tests"
"$real_cc" "${flags[@]}" tests/test_examples.c tests/validation.c -o "$san_root/build/test_examples"
"$real_cc" -dumpfullversion > "$san_root/build/gcc-version.txt"
cat > "$san_root/build/gcc-wrapper" <<'WRAPPER'
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
exec "$san_root/build/test_examples"
