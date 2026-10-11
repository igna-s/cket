#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
for example in entanglement find; do
    build/cket "examples/$example.c" --emit-ir -o "docs/paper/evidence/$example.qir.json"
    build/cket_backend --state "docs/paper/evidence/$example.qir.json" > "docs/paper/evidence/$example.state.json"
done
python3 - <<'PY'
from pathlib import Path
import subprocess
root = Path('docs/paper/evidence')
for name in ('entanglement', 'find'):
    (root / (name + '.qir.txt')).write_bytes((root / (name + '.qir.json')).read_bytes())
    (root / (name + '.qir.json')).unlink()
PY
