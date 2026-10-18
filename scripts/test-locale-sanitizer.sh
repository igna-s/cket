#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/audit/locale-sanitizer
compiler="${CC:-gcc}"
binary="$PWD/build/audit/locale-sanitizer/repro"
"$compiler" -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
    -fno-omit-frame-pointer -fno-pie -no-pie tests/repro_glibc_locale.c -o "$binary"
export LOCPATH="$PWD/build/audit/locale-sanitizer"
export ASAN_OPTIONS=detect_leaks=1:fast_unwind_on_malloc=0
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
suppression="$PWD/tests/glibc-locale.lsan"
baseline=0
env -u LSAN_OPTIONS "$binary" libc >build/audit/locale-sanitizer/baseline.log 2>&1 || baseline=$?
if [[ "$baseline" != 0 ]]; then
    [[ "$baseline" == 1 ]]
    grep -q 'argz_add_sep' build/audit/locale-sanitizer/baseline.log
    grep -q 'ERROR: LeakSanitizer' build/audit/locale-sanitizer/baseline.log
fi
LSAN_OPTIONS="suppressions=$suppression:print_suppressions=0" \
    "$binary" libc >build/audit/locale-sanitizer/scoped.log 2>&1
for mode in project locale-handle; do
    status=0
    LSAN_OPTIONS="suppressions=$suppression:print_suppressions=0" \
        "$binary" "$mode" >"build/audit/locale-sanitizer/$mode.log" 2>&1 || status=$?
    [[ "$status" == 1 ]]
    grep -q 'ERROR: LeakSanitizer' "build/audit/locale-sanitizer/$mode.log"
done
printf 'PASS locale sanitizer boundary: libc-only LOCPATH reproduction status=%s; scoped exclusion preserves project and locale-handle leak detection\n' "$baseline"
