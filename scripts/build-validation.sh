#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
compiler="${CC:-gcc}"
flags=(-std=c11 -O2 -Wall -Wextra -Werror -Isrc/engine -Itests)
suffix=""
native_library=build/libcket_quantum.a
san_flags=()
if [[ "${VALIDATION_SANITIZERS:-0}" == 1 ]]; then
    suffix=-sanitized
    san_flags=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie)
    flags+=("${san_flags[@]}")
    mkdir -p build/validation-deps/sanitized
    native_objects=()
    for component in json quantum verify simulator; do
        object="build/validation-deps/sanitized/cket-$component.o"
        "$compiler" "${flags[@]}" -Wno-misleading-indentation -c "src/engine/$component.c" -o "$object"
        native_objects+=("$object")
    done
    native_library=build/validation-deps/sanitized/libcket_quantum.a
    ar rcs "$native_library" "${native_objects[@]}"
fi
mkdir -p build
"$compiler" "${flags[@]}" tests/fuzz_frontend.c tests/validation.c -o "build/fuzz_frontend$suffix"
"$compiler" "${flags[@]}" tests/benchmark_synthesis.c tests/validation.c -o "build/benchmark_synthesis$suffix"
"$compiler" "${flags[@]}" tests/qasm_samples.c "$native_library" -lm -o "build/qasm_samples$suffix"

# QuEST's pinned v3 serial backend is C. It is only a validation dependency.
quest_commit=d4f75f724993b4af8e43a796e3c09ce24ae11670
quest_hash=1a5939798a70b749dfa57cfaa116ece03a2d36aa59c1b712cb3dc2103ab1cded
quest_archive=build/validation-deps/quest-3.7.0.tar.gz
quest_root="build/validation-deps/QuEST-$quest_commit/QuEST"
mkdir -p build/validation-deps
if [[ ! -f "$quest_archive" ]]; then
    curl --fail --location --retry 3 --connect-timeout 15 --max-time 120 \
        "https://codeload.github.com/QuEST-Kit/QuEST/tar.gz/$quest_commit" -o "$quest_archive.part"
    printf '%s  %s\n' "$quest_hash" "$quest_archive.part" | sha256sum --check --status
    mv "$quest_archive.part" "$quest_archive"
fi
if ! printf '%s  %s\n' "$quest_hash" "$quest_archive" | sha256sum --check --status; then
    printf 'QuEST archive checksum mismatch: %s\n' "$quest_archive" >&2
    exit 1
fi
if [[ ! -f "$quest_root/include/QuEST.h" ]]; then
    tar -xzf "$quest_archive" -C build/validation-deps
fi
quest_flags=(-std=c99 -O2 -D_DEFAULT_SOURCE -DQuEST_PREC=2 -I"$quest_root/include" -I"$quest_root/src")
quest_flags+=("${san_flags[@]}")
quest_sources=(QuEST QuEST_common QuEST_qasm QuEST_validation mt19937ar CPU/QuEST_cpu CPU/QuEST_cpu_local)
quest_objects=()
for component in "${quest_sources[@]}"; do
    object="build/validation-deps/${component//\//_}$suffix.o"
    "$compiler" "${quest_flags[@]}" -c "$quest_root/src/$component.c" -o "$object"
    quest_objects+=("$object")
done
ar rcs "build/validation-deps/libQuEST$suffix.a" "${quest_objects[@]}"
"$compiler" "${flags[@]}" -DQuEST_PREC=2 -DVALIDATION_SANITIZED="${VALIDATION_SANITIZERS:-0}" -I"$quest_root/include" \
    tests/validate_qasm.c tests/qasm_reference.c tests/validation.c \
    "$native_library" "build/validation-deps/libQuEST$suffix.a" -lm -o "build/validate_qasm$suffix"
printf '%s\n' 'Built C frontend fuzzing, synthesis growth and independent QuEST QASM validation'
