#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 build/check-memory-batch-root.py --prepare
CC=gcc-11 CXX=g++-11 bash scripts/build.sh >build/memory-batch-root-build.log 2>&1
repo_root="$PWD"
san_root="$repo_root/build/memory-batch-root-sanitized"
test ! -e "$san_root"
mkdir -p "$san_root/build"
cp cket "$san_root/cket"
for directory in include examples src tests; do
    ln -s "$repo_root/$directory" "$san_root/$directory"
done
plugin_dir="$(gcc-11 -print-file-name=plugin)"
headers="$plugin_dir/include"
if [[ ! -f "$headers/gcc-plugin.h" ]]; then
    headers=".deps/gcc$plugin_dir/include"
fi
test -f "$headers/gcc-plugin.h"
extra=()
if [[ -d .deps/gcc/usr/include ]]; then
    extra+=(-I.deps/gcc/usr/include -I.deps/gcc/usr/include/x86_64-linux-gnu)
fi
flags=(-std=c11 -O1 -g -Wall -Wextra -Werror -Isrc/engine -Itests
    -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie)
g++-11 -std=c++17 -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-diag \
    -fPIC -shared -fno-rtti -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"$headers" "${extra[@]}" src/cket_plugin.cpp -o "$san_root/build/cket_plugin.so"
objects=()
for component in json quantum verify simulator; do
    object="$san_root/build/$component.o"
    gcc-11 "${flags[@]}" -c "src/engine/$component.c" -o "$object"
    objects+=("$object")
done
ar rcs "$san_root/build/libcket_quantum.a" "${objects[@]}"
gcc-11 "${flags[@]}" -c src/cket_runtime.c -o "$san_root/build/cket_runtime.o"
gcc-11 "${flags[@]}" src/cket.c "$san_root/build/libcket_quantum.a" -lm -o "$san_root/build/cket"
gcc-11 "${flags[@]}" src/cket_backend.c "$san_root/build/libcket_quantum.a" -lm -o "$san_root/build/cket_backend"
gcc-11 -dumpfullversion >"$san_root/build/gcc-version.txt"
cat >"$san_root/build/gcc-wrapper" <<'WRAPPER'
#!/usr/bin/env bash
set -euo pipefail
export LD_PRELOAD="$(gcc-11 -print-file-name=libasan.so)"
# GCC itself retains process allocations; runtime/validator checks stay enabled.
export ASAN_OPTIONS="${ASAN_OPTIONS:-}:detect_leaks=0"
exec gcc-11 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie "$@"
WRAPPER
chmod +x "$san_root/build/gcc-wrapper"
python3 build/check-memory-batch-root.py --finish
