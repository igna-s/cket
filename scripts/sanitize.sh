#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
"${CC:-gcc}" -std=c11 -O1 -g -Wall -Wextra -Werror -Wno-misleading-indentation \
    -fsanitize=address,undefined -fno-omit-frame-pointer -Inative \
    tests/test_native.c native/json.c native/quantum.c native/verify.c native/simulator.c \
    -lm -o build/cket_tests_sanitized
exec build/cket_tests_sanitized "$PWD"
