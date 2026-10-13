#!/usr/bin/env python3
"""Archive a completed, finite verification campaign and its source identity."""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
import shutil
import subprocess
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--log', type=Path, required=True)
    args = parser.parse_args()
    log = args.log.resolve()
    content = log.read_text(encoding='utf-8')
    # Each result must occur once in the normal run and once with sanitizers.
    patterns = {
        'native': r'^391 passed, 0 failed \(native C tests, no external quantum SDK\)$',
        'examples': r'^1452 checks passed across 36 examples, five optimization levels and CLI/input contracts$',
        'classical': r'^5 finite integration comparisons passed across the 29 C11 header families; quantum-library coverage remains incomplete\.$',
        'frontend': r'^665 generated frontend compilations passed; seed=20261012 \(C generator, SplitMix64\)$',
        'synthesis': r'^24 circuits compiled without tables;',
        'qasm': r'^40 exported circuits independently validated with QuEST 3\.7\.0 \(C\): 37 states, 3 channels; table-free export$',
        'shifts': r'^55 shift checks passed; generated wide circuits are distinct from state-validated small domains\.$',
        'expressions': r'^97 expression checks passed; generated circuits and complete-state comparisons have separate scopes\.$',
        'inplace': r'^271 in-place checks passed; wide circuit generation is distinct from complete-state validation\.$',
        'memcopy': r'^414 memory-copy checks passed; basis-value transfer is distinct from state cloning and wide generation\.$',
        'harness': r'^18 validation harness checks passed; infrastructure faults cannot count as semantic rejections\.$',
        'division': r'^990 division checks passed; full-state validation, explicit channels and wide generation have separate scopes\.$',
        'bindings': r'^399 CPU binding checks passed; structural templates and bound circuit correctness have separate scopes\.$',
        'history': r'^809 versioned history checks passed; retained outputs, known basis cleanup and resource bounds remain distinct\.$',
    }
    for name, pattern in patterns.items():
        if len(re.findall(pattern, content, re.M)) != 2:
            raise SystemExit('Missing normal/sanitized result: ' + name)
    if re.search(
        r'^FAIL\b|ERROR: (?:AddressSanitizer|LeakSanitizer)|runtime error:|internal compiler error:|validation infrastructure failure|validation command timed out',
        content,
        re.M,
    ) or not content.rstrip().endswith(
        'All compiler, runtime, example, synthesis and independent QASM checks passed.'
    ):
        raise SystemExit('Verification transcript has a failure or lacks completion.')
    growth = ROOT / 'build/synthesis-growth.csv'
    with growth.open() as stream:
        if len(list(csv.DictReader(stream))) != 24:
            raise SystemExit('Expected 24 synthesis resource rows.')
    paths = [
        ROOT / 'cket',
        ROOT / '.clang-format',
        ROOT / '.editorconfig',
        ROOT / 'scripts/requirements-format.txt',
    ]
    for area in ('src', 'include', 'tests', 'scripts', 'examples'):
        paths.extend(
            p
            for p in (ROOT / area).rglob('*')
            if p.is_file() and p.suffix in ('.c', '.cpp', '.h', '.inc', '.sh', '.py')
        )
    paths.extend((ROOT / '.github/workflows').glob('*.yml'))
    hashes = {
        p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
        for p in sorted(set(paths))
    }
    destination = ROOT / 'docs/audit-evidence'
    destination.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(log, destination / 'verify.log')
    shutil.copyfile(growth, destination / 'synthesis-growth.csv')
    artifacts = {
        p.name: hashlib.sha256(p.read_bytes()).hexdigest()
        for p in (destination / 'verify.log', destination / 'synthesis-growth.csv')
    }

    def output(*command):
        return subprocess.check_output(command, cwd=ROOT, text=True).strip()

    result = {
        'status': 'passed',
        'archived_utc': datetime.now(timezone.utc).isoformat(),
        'base_commit': output('git', 'rev-parse', 'HEAD'),
        'uncommitted_changes': bool(output('git', 'status', '--porcelain')),
        'compiler': output('gcc-11', '-dumpfullversion'),
        'cxx_compiler': output('g++-11', '-dumpfullversion'),
        'system': output('uname', '-srmo'),
        'command': 'CC=gcc-11 CXX=g++-11 CKET_FUZZ_CASES=128 CKET_FUZZ_SEED=20261012 bash scripts/verify.sh',
        'configurations': ['normal', 'ASan/UBSan'],
        'counts_per_configuration': {
            'native': 391,
            'example_checks': 1452,
            'examples': 36,
            'classical_header_integration': 5,
            'frontend_compilations': 665,
            'synthesis_circuits': 24,
            'shift_checks': 55,
            'expression_checks': 97,
            'inplace_checks': 271,
            'memory_copy_checks': 414,
            'harness_checks': 18,
            'division_checks': 990,
            'binding_checks': 399,
            'history_checks': 809,
            'external_qasm_states': 37,
            'external_qasm_channels': 3,
        },
        'source_sha256': hashes,
        'artifact_sha256': artifacts,
        'source_identity_scope': 'Hashes captured at archival time; not a monitored snapshot during compilation.',
        'scope': 'Finite evidence for the current subset; neither full C11 quantum coverage nor a new published commit. Source hashes identify the working tree, not merely its base commit.',
    }
    (destination / 'results.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print('Archived complete normal/sanitized campaign; full C11 quantum coverage remains open.')


if __name__ == '__main__':
    main()
