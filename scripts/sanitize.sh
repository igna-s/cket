#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
"${CC:-gcc}" -std=c11 -O1 -g -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -Isrc/engine \
    tests/test_native.c src/engine/json.c src/engine/quantum.c src/engine/verify.c src/engine/simulator.c \
    -lm -o build/cket_tests_sanitized
exec build/cket_tests_sanitized "$PWD"
