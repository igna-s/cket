#!/usr/bin/env bash
# Reproduce the paper's fixed campaign without touching its published logs.
set -euo pipefail
cd "$(dirname "$0")/.."
workspace="$PWD"
revision=1ebb2e308e61bed5632798ded5beecf77b6b9a9c
export CC="${CC:-gcc-11}" CXX="${CXX:-g++-11}"
if [[ "$("$CC" -dumpfullversion)" != 11.4.0 ]] || [[ "$("$CXX" -dumpfullversion)" != 11.4.0 ]]; then
    printf '%s\n' 'The historical paper campaign requires GCC/G++ 11.4.0.' >&2
    exit 1
fi
snapshot="$workspace/build/audit/paper-baseline"
if [[ -e "$snapshot" ]]; then
    printf '%s\n' "Snapshot already exists: $snapshot. Preserve or remove it explicitly before a new campaign." >&2
    exit 1
fi
git cat-file -e "$revision^{commit}"
mkdir -p "$snapshot"
git archive "$revision" | tar -x -C "$snapshot"
if [[ -d "$workspace/.deps" ]]; then
    ln -s "$workspace/.deps" "$snapshot/.deps"
fi
# Reuse the pinned download/source, but build independent engine/QuEST objects.
dependency=build/validation-deps
mkdir -p "$snapshot/$dependency"
if [[ -f "$workspace/$dependency/quest-3.7.0.tar.gz" ]]; then
    cp "$workspace/$dependency/quest-3.7.0.tar.gz" "$snapshot/$dependency/"
fi
quest_name=QuEST-d4f75f724993b4af8e43a796e3c09ce24ae11670
if [[ -d "$workspace/$dependency/$quest_name" ]]; then
    ln -s "$workspace/$dependency/$quest_name" "$snapshot/$dependency/$quest_name"
fi
cd "$snapshot"
printf 'Archived paper revision: %s\n' "$revision"
bash scripts/build.sh >baseline-build.log 2>&1
bash scripts/test.sh >baseline-native.log 2>&1
bash scripts/build-validation.sh >baseline-validation-build.log 2>&1
build/fuzz_frontend --seed 20261011 --cases 24 >baseline-frontend.log 2>&1
build/benchmark_synthesis >baseline-growth.log 2>&1
build/validate_qasm >baseline-qasm.log 2>&1
python3 - "$workspace" <<'PY'
import csv
import re
import sys
from pathlib import Path
expected = {
    'baseline-native.log': [r'354 passed, 0 failed', r'426 checks passed across 16 examples'],
    'baseline-frontend.log': [r'145 generated frontend compilations passed; seed=20261011'],
    'baseline-growth.log': [r'24 circuits compiled without tables'],
    'baseline-qasm.log': [r'11 exported circuits independently validated with QuEST 3.7.0'],
}
for name, patterns in expected.items():
    content = Path(name).read_text()
    if re.search(r'^FAIL\b', content, re.M) or not all(re.search(p, content) for p in patterns):
        raise SystemExit('Historical campaign differs: ' + name)
published = Path(sys.argv[1]) / 'docs/paper/evidence/synthesis-growth.csv'
with published.open() as f:
    original = list(csv.DictReader(f))
with Path('build/synthesis-growth.csv').open() as f:
    reproduced = list(csv.DictReader(f))
if original != reproduced:
    raise SystemExit('Historical synthesis rows differ from the paper evidence')
print('Paper baseline independently rerun: 354 native, 426 example, 145 frontend, 24 synthesis and 11 QuEST cases; published CSV reproduced exactly.')
PY
