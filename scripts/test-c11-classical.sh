#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
compiler="${CC:-gcc}"
driver="${CKET_DRIVER:-build/cket}"
temporary=$(mktemp -d)
trap 'rm -f "$temporary/reference" "$temporary/quantum" "$temporary/reference.txt" "$temporary/quantum.txt"; rmdir "$temporary"' EXIT
for level in -O0 -O1 -O2 -O3 -Os; do
    "$compiler" -std=c11 "$level" tests/c11_classical.c -lm -pthread -o "$temporary/reference"
    "$temporary/reference" >"$temporary/reference.txt"
    "$driver" tests/c11_classical.c "$level" -pthread -o "$temporary/quantum"
    "$temporary/quantum" >"$temporary/quantum.txt"
    cmp "$temporary/reference.txt" "$temporary/quantum.txt"
    printf 'PASS C11 classical integration: %s, ordinary GCC reference and explicit quantum boundary\n' "$level"
done
printf '%s\n' '5 finite integration comparisons passed across the 29 C11 header families; quantum-library coverage remains incomplete.'
