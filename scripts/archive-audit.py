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


def source_hashes():
    paths = [
        ROOT / p
        for p in ('cket', '.clang-format', '.editorconfig', 'scripts/requirements-format.txt')
    ]
    paths.append(ROOT / 'tests/glibc-locale.lsan')
    for area in ('src', 'include', 'tests', 'scripts', 'examples'):
        paths.extend(
            p
            for p in (ROOT / area).rglob('*')
            if p.is_file() and p.suffix in ('.c', '.cpp', '.h', '.inc', '.sh', '.py', '.json')
        )
    paths.extend((ROOT / '.github/workflows').glob('*.yml'))
    return {
        p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
        for p in sorted(set(paths))
    }


def memory_loop_jump_results(content):
    rows = []
    for line in content.splitlines():
        if not line.startswith('{'):
            continue
        try:
            value = json.loads(line)
        except json.JSONDecodeError:
            continue
        if isinstance(value, dict) and value.get('scope', '').startswith(
            'Finite runtime byte prefixes through CPU break/continue'
        ):
            rows.append(value)
    results = {}
    for sanitized, label, counts, levels in (
        (False, 'normal', (9480, 2400, 1896, 1), ['O0', 'O2']),
        (True, 'ASan/UBSan', (4740, 1200, 948, 1), ['O1']),
    ):
        matches = [row for row in rows if row.get('sanitized') is sanitized]
        if len(matches) != 1:
            raise SystemExit('Missing or repeated memory loop jump matrix: ' + label)
        row = matches[0]
        keys = (
            'full_state_checkpoints',
            'complete_gate_inverses',
            'untouched_low_byte_density_comparisons',
            'unwanted_low_bit_copy_mutations_rejected',
        )
        if (
            row.get('status') != 'passed'
            or tuple(row.get(key) for key in keys) != counts
            or row.get('levels') != levels
        ):
            raise SystemExit('Incomplete memory loop jump matrix: ' + label)
        results[label] = row
    if len(rows) != 2:
        raise SystemExit('Unexpected memory loop jump matrix configuration')
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--log', type=Path)
    mode.add_argument(
        '--freeze',
        type=Path,
        help='Create a source snapshot before verification; never overwrite it.',
    )
    parser.add_argument(
        '--sources',
        type=Path,
        help='Require an unchanged pre-verification source snapshot before archival.',
    )
    parser.add_argument(
        '--destination',
        type=Path,
        default=ROOT / 'docs/audit-evidence',
        help='Directory for the completed campaign; choose a new directory to preserve earlier evidence.',
    )
    args = parser.parse_args()
    if args.freeze:
        if args.sources:
            raise SystemExit('--sources belongs to archival mode.')
        with args.freeze.open('x', encoding='utf-8') as stream:
            json.dump(source_hashes(), stream, indent=2)
            stream.write('\n')
        print('Frozen verification source hashes; snapshot was not overwritten.')
        return
    log = args.log.resolve()
    content = log.read_text(encoding='utf-8')
    # Each result must occur once in the normal run and once with sanitizers.
    patterns = {
        'native': r'^395 passed, 0 failed \(native C tests, no external quantum SDK\)$',
        'examples': r'^1509 checks passed across 37 examples, five optimization levels and CLI/input contracts$',
        'classical': r'^5 finite integration comparisons passed across the 29 C11 header families; quantum-library coverage remains incomplete\.$',
        'frontend': r'^665 generated frontend compilations passed; seed=20261012 \(C generator, SplitMix64\)$',
        'synthesis': r'^24 circuits compiled without tables;',
        'qasm': r'^70 exported circuits independently validated with QuEST 3\.7\.0 \(C\): 64 states, 6 channels; table-free export$',
        'shifts': r'^203 shift checks passed; generated wide circuits are distinct from state-validated small domains\.$',
        'sparse': r'^88 sparse simulation checks passed; phased gates/inverses, 128 wires, explicit channels, support bounds and native int32/size_t programs\.$',
        'expressions': r'^3509 expression checks passed; generated circuits and complete-state comparisons have separate scopes\.$',
        'inplace': r'^271 in-place checks passed; wide circuit generation is distinct from complete-state validation\.$',
        'memcopy': r'^414 memory-copy checks passed; basis-value transfer is distinct from state cloning and wide generation\.$',
        'memcmp': r'^302 memcmp checks passed; coherent byte comparisons are distinct from general memory/library completeness\.$',
        'strncmp': r'^246 strncmp checks passed; NUL-terminated comparisons remain distinct from complete string/library support\.$',
        'strings': r'^460 string family checks passed; bounded strlen/strcmp adapters are distinct from complete C string compatibility\.$',
        'byte_snapshots': r'^650 CPU byte snapshot checks passed; dynamic addresses, volatile/atomic access and complete quantum C11 remain pending\.$',
        'memset': r'^940 memset checks passed; dense/sparse states, labeled paths, native executions, rejections and wide generation have distinct proof scopes\.$',
        'memory_sequences': r'^360 memory sequence checks passed; complete labeled action, native integration and expected rejections have distinct proof scopes\.$',
        'memory_returns': r'^290 memory return checks passed; complete labeled action, native integration and expected rejections have distinct proof scopes\.$',
        'pointer_differences': r'^170 pointer difference checks passed; native distances, finite coherent action and unsupported contexts have separate scopes\.$',
        'controlled_operands': r'^1270 controlled operand checks passed; native values, undefined-domain diagnostics, effect boundaries and independent archived states have separate scopes\.$',
        'phase': r'^3380 phase checks passed; compiled CPU angles, complete states, native domains and independent exported proofs have separate scopes\.$',
        'harness': r'^18 validation harness checks passed; infrastructure faults cannot count as semantic rejections\.$',
        'division': r'^990 division checks passed; full-state validation, explicit channels and wide generation have separate scopes\.$',
        'bindings': r'^519 CPU binding checks passed; structural templates and bound circuit correctness have separate scopes\.$',
        'readonly_borrows': r'^305 readonly borrow checks passed; 30 IR compilations, 30 native executions, 60 C states, 60 independent gate states, 60 complete inverses and 65 mutation/ownership rejections\.$',
        'function_entries': r'^90 function entry checks passed; 30 native C/quantum compositions, 55 compile-time ABI rejections and 5 explicit runtime entry rejections\.$',
        'procedure_returns': r'^Structured CPU procedure returns passed; finite native/state/inverse scopes recorded in summary\.json\.$',
        'coherent_returns': r'^Coherent caller CPU returns passed; finite state, inverse and inactive-operand scopes recorded in summary\.json\.$',
        'memory_loop_jumps': r'^Memory loop jumps passed; finite states, C byte masks and complete inverses recorded\.$',
        'history': r'^2332 versioned history checks passed; retained outputs, known basis cleanup and resource bounds remain distinct\.$',
        'heap': r'^16878 heap checks passed; dynamic identities, complete phased states, saved inverses, retained outputs and resource failure have separate scopes\.$',
        'runtime_versions': r'^15 runtime history contracts passed; finite states, schema guards and resource bounds have separate scopes\.$',
        'runtime_memory': r'^23 runtime memory preflight contracts passed; schemas, bounds, overlap, typed binding and inactive controls checked before mutation\.$',
        'ctype': r'^1815 ctype checks passed; immutable byte-locale coverage is distinct from wide-character, concurrency and source-folding contracts\.$',
        'profiles': r'^96 immutable profile proof checks passed; distinct locale functions cannot share branch facts\.$',
        'numeric_locale': r'^70 numeric locale checks passed; JSON phases and QASM decimals preserve global/thread locales\.$',
        'library': r'^2100 libc boundary checks passed across 210 interfaces; rejections are not quantum implementations\.$',
    }
    for name, pattern in patterns.items():
        if len(re.findall(pattern, content, re.M)) != 2:
            raise SystemExit('Missing normal/sanitized result: ' + name)
    memory_jumps = memory_loop_jump_results(content)
    if len(re.findall(r'^PASS locale sanitizer boundary:', content, re.M)) != 1:
        raise SystemExit('Missing scoped locale sanitizer guard.')
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
    hashes = source_hashes()
    if args.sources and hashes != json.loads(args.sources.read_text(encoding='utf-8')):
        raise SystemExit(
            'Source hashes changed since the pre-verification snapshot; archive was not replaced.'
        )
    destination = args.destination.resolve()
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
        'libc': output('getconf', 'GNU_LIBC_VERSION'),
        'locale_fixture': 'localedef --no-archive -i en_US -f ISO-8859-1 PATH',
        'locale_sanitizer_exception': {
            'frame': 'argz_add_sep',
            'file': 'tests/glibc-locale.lsan',
            'upstream': 'https://sourceware.org/pipermail/glibc-bugs/2020-April/047605.html',
            'scope': 'Only ctype, numeric-locale and QuEST locale-fixture validators. Real project and locale-handle leaks remain detectable; a separate guard verifies both.',
        },
        'command': 'CC=gcc-11 CXX=g++-11 CKET_FUZZ_CASES=128 CKET_FUZZ_SEED=20261012 bash scripts/verify.sh',
        'configurations': ['normal', 'ASan/UBSan'],
        'configuration_specific_results': {'memory_loop_jumps': memory_jumps},
        'counts_per_configuration': {
            'native': 395,
            'example_checks': 1509,
            'examples': 37,
            'classical_header_integration': 5,
            'frontend_compilations': 665,
            'synthesis_circuits': 24,
            'shift_checks': 203,
            'sparse_checks': 88,
            'expression_checks': 3509,
            'inplace_checks': 271,
            'memory_copy_checks': 414,
            'quantum_heap_checks': 16878,
            'memory_compare_checks': 302,
            'string_compare_checks': 246,
            'cpu_byte_snapshot_checks': 650,
            'memory_set_checks': 940,
            'memory_sequence_checks': 360,
            'memory_return_checks': 290,
            'pointer_difference_checks': 170,
            'controlled_operand_checks': 1270,
            'harness_checks': 18,
            'division_checks': 990,
            'binding_checks': 519,
            'readonly_borrow_checks': 305,
            'function_entry_checks': 90,
            'history_checks': 2332,
            'ctype_checks': 1815,
            'profile_proof_checks': 96,
            'numeric_locale_checks': 70,
            'libc_boundary_checks': 2100,
            'libc_interfaces_probed': 210,
            'frontend_byte_inputs_per_expression': 256,
            'external_qasm_states': 64,
            'external_qasm_channels': 6,
        },
        'source_sha256': hashes,
        'artifact_sha256': artifacts,
        'source_identity_scope': (
            'Hashes match a supplied pre-verification frozen snapshot; endpoints compared, not continuous monitoring.'
            if args.sources
            else 'Hashes captured at archival time; not a monitored snapshot during compilation.'
        ),
        'scope': 'Finite evidence for the current subset; neither full C11 quantum coverage nor a new published commit. Source hashes identify the working tree, not merely its base commit.',
    }
    (destination / 'results.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print('Archived complete normal/sanitized campaign; full C11 quantum coverage remains open.')


if __name__ == '__main__':
    main()
