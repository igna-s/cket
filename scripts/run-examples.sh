#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

usage() {
    cat <<'EOF'
Usage: bash scripts/run-examples.sh [--all | --list | NAME ...]
Run all simulator-compatible examples, or selected names without .c.
Interactive examples receive the sample inputs documented in examples/README.md.
Set CKET_SEED to choose the measurement seed (default: 7).
types.c is export-only: its 32-bit register exceeds the simulator limit.
EOF
}

names=()
for source in examples/*.c; do
    name="${source##*/}"
    name="${name%.c}"
    [[ "$name" == types ]] || names+=("$name")
done

case "${1:---all}" in
    --help | -h)
        usage
        exit 0
        ;;
    --list)
        printf '%s\n' "${names[@]}"
        exit 0
        ;;
    --all)
        if (($# > 1)); then
            usage >&2
            exit 1
        fi
        ;;
    *) names=("$@") ;;
esac

for name in "${names[@]}"; do
    if [[ ! "$name" =~ ^[a-z][a-z0-9_]*$ || ! -f "examples/$name.c" ]]; then
        printf 'Unknown example: %s\n' "$name" >&2
        exit 1
    fi
    if [[ "$name" == types ]]; then
        printf 'types is export-only; use: build/cket examples/types.c --emit-qasm\n' >&2
        exit 1
    fi
done

[[ -x build/cket ]] || bash scripts/build.sh
mkdir -p build/examples
export CKET_SEED="${CKET_SEED:-7}"
for name in "${names[@]}"; do
    printf '\nRunning %s\n' "$name"
    build/cket "examples/$name.c" -o "build/examples/$name"
    case "$name" in
        dynamic_vector) printf '0\n' | "build/examples/$name" ;;
        input_sum) printf '10\n' | "build/examples/$name" ;;
        input_minmax) printf '8 42 17 42 0 128 255 9 255\n' | "build/examples/$name" ;;
        *) "build/examples/$name" ;;
    esac
done
printf '\nCompleted %u examples.\n' "${#names[@]}"
