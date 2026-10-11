#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
evidence=docs/paper/evidence
mkdir -p "$evidence"
{
    date -u +'%Y-%m-%dT%H:%M:%SZ'
    git rev-parse HEAD
    uname -srmo
    gcc --version | head -n 1
    g++ --version | head -n 1
    python3 --version
    python3 - <<'PY'
import importlib.util
for name in ('reportlab', 'matplotlib', 'PIL', 'fitz', 'pypdf', 'pdfplumber'):
    print(name, bool(importlib.util.find_spec(name)))
PY
    command -v pdftoppm || true
    command -v uv || true
    ls /usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf
} > "$evidence/environment.txt"
bash scripts/build.sh > "$evidence/build.log" 2>&1
bash scripts/test.sh > "$evidence/native.log" 2>&1
bash scripts/build-validation.sh > "$evidence/validation-build.log" 2>&1
build/fuzz_frontend --seed 20261011 --cases 24 > "$evidence/frontend.log" 2>&1
build/benchmark_synthesis > "$evidence/growth.log" 2>&1
cp build/synthesis-growth.csv "$evidence/synthesis-growth.csv"
build/validate_qasm > "$evidence/qasm.log" 2>&1
printf '%s\n' 'Audit project checks passed.'
