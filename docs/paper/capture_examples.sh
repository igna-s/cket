#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
# These files belong to the published historical campaign. Current compiler
# examples are validated in tmp/pdfs/ by audit_paper.py instead.
baseline=1ebb2e308e61bed5632798ded5beecf77b6b9a9c
if [[ "$(git rev-parse HEAD)" != "$baseline" ]] ||
    ! git diff --quiet HEAD -- src include tests examples scripts; then
    printf '%s\n' "Paper example evidence requires the clean revision $baseline; current captures must use a separate output directory." >&2
    exit 1
fi
for example in entanglement find; do
    build/cket "examples/$example.c" --emit-ir -o "docs/paper/evidence/$example.qir.json"
    build/cket_backend --state "docs/paper/evidence/$example.qir.json" >"docs/paper/evidence/$example.state.json"
done
python3 - <<'PY'
from pathlib import Path
import subprocess
root = Path('docs/paper/evidence')
for name in ('entanglement', 'find'):
    (root / (name + '.qir.txt')).write_bytes((root / (name + '.qir.json')).read_bytes())
    (root / (name + '.qir.json')).unlink()
PY
