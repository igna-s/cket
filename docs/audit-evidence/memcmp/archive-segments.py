"""Preserve successful scoped checks together with the earlier failed prefix."""
from datetime import datetime, timezone
from pathlib import Path
import csv
import hashlib
import io
import json
import re
import shutil
import subprocess
import tarfile

root = Path(__file__).resolve().parent.parent
destination = root / 'docs/audit-evidence/memcmp'
prefix = json.loads((root / 'build/memcmp-final-sources.json').read_text())
final = json.loads((root / 'build/memcmp-verified-sources.json').read_text())
changed = sorted(p for p in prefix.keys() | final.keys() if prefix.get(p) != final.get(p))
assert changed == ['tests/qasm_reference.c', 'tests/qasm_reference.h', 'tests/validate_qasm.c'], changed
inputs = {p: (root / p).read_bytes() for p in final}
assert all(hashlib.sha256(inputs[p]).hexdigest() == digest for p, digest in final.items())
with tarfile.open(root / 'build/memcmp-normal-prefix-sources.tar.gz') as archive:
    assert set(archive.getnames()) == set(prefix) | {'LICENSE', 'verified-sources.json'}
    for path, digest in prefix.items():
        assert hashlib.sha256(archive.extractfile(path).read()).hexdigest() == digest, path
    assert json.load(archive.extractfile('verified-sources.json')) == prefix

logs = {
    'normal-prefix.log': root / 'build/verify-memcmp-full.log',
    'reference-normal.log': root / 'build/verify-memcmp-reference.log',
    'sanitized.log': root / 'build/verify-memcmp-sanitized.log',
}
content = {name: path.read_text() for name, path in logs.items()}
expected = {
    'native': '391 passed, 0 failed (native C tests, no external quantum SDK)',
    'examples': '1452 checks passed across 36 examples, five optimization levels and CLI/input contracts',
    'classical': '5 finite integration comparisons passed across the 29 C11 header families; quantum-library coverage remains incomplete.',
    'frontend': '665 generated frontend compilations passed; seed=20261012 (C generator, SplitMix64)',
    'synthesis': '24 circuits compiled without tables;',
    'shifts': '55 shift checks passed; generated wide circuits are distinct from state-validated small domains.',
    'expressions': '97 expression checks passed; generated circuits and complete-state comparisons have separate scopes.',
    'inplace': '271 in-place checks passed; wide circuit generation is distinct from complete-state validation.',
    'memcopy': '414 memory-copy checks passed; basis-value transfer is distinct from state cloning and wide generation.',
    'memcmp': '302 memcmp checks passed; coherent byte comparisons are distinct from general memory/library completeness.',
    'harness': '18 validation harness checks passed; infrastructure faults cannot count as semantic rejections.',
    'division': '990 division checks passed; full-state validation, explicit channels and wide generation have separate scopes.',
    'bindings': '399 CPU binding checks passed; structural templates and bound circuit correctness have separate scopes.',
    'history': '2332 versioned history checks passed; retained outputs, known basis cleanup and resource bounds remain distinct.',
    'ctype': '1815 ctype checks passed; immutable byte-locale coverage is distinct from wide-character, concurrency and source-folding contracts.',
    'profiles': '96 immutable profile proof checks passed; distinct locale functions cannot share branch facts.',
    'numeric_locale': '70 numeric locale checks passed; JSON phases and QASM decimals preserve global/thread locales.',
    'library': '2100 libc boundary checks passed across 210 interfaces; rejections are not quantum implementations.',
}
for log in ('normal-prefix.log', 'sanitized.log'):
    lines = content[log].splitlines()
    for suite, result in expected.items():
        count = sum(line.startswith(result) if suite == 'synthesis' else line == result for line in lines)
        assert count == 1, (log, suite, count)
qasm_result = '57 exported circuits independently validated with QuEST 3.7.0 (C): 51 states, 6 channels; table-free export'
for log in ('reference-normal.log', 'sanitized.log'):
    lines = content[log].splitlines()
    assert lines.count(qasm_result) == 1, log
    assert sum(line.startswith('PASS independent QASM state and probabilities:') for line in lines) == 51, log
    assert sum(line.startswith('PASS independent QASM channel:') for line in lines) == 6, log
assert qasm_result not in content['normal-prefix.log']
failure = 'independent QASM import failed: unsupported or malformed exported QASM at byte 46'
assert content['normal-prefix.log'].rstrip().endswith(failure)
assert content['normal-prefix.log'].count(failure) == 1
errors = r'^FAIL\b|ERROR: (?:AddressSanitizer|LeakSanitizer)|runtime error:|internal compiler error:|validation infrastructure failure|validation command timed out'
for name, text in content.items():
    assert not re.search(errors, text, re.M), name
assert len(re.findall(r'^PASS locale sanitizer boundary:', content['sanitized.log'], re.M)) == 1
assert content['sanitized.log'].rstrip().endswith('All compiler, runtime, example, synthesis and independent QASM checks passed.')
normal_growth = root / 'build/synthesis-growth.csv'
sanitized_growth = root / 'build/sanitized/build/synthesis-growth.csv'
assert normal_growth.read_bytes() == sanitized_growth.read_bytes()
with normal_growth.open() as stream:
    assert len(list(csv.DictReader(stream))) == 24
verification = (root / 'scripts/verify.sh').read_text()
tail = (root / 'build/verify-memcmp-tail.sh').read_text()
marker = 'bash scripts/test-locale-sanitizer.sh'
assert tail.split(marker, 1)[1] == verification.split(marker, 1)[1]
inventory = json.loads((root / 'build/memcmp-inventory-check.log').read_text())
assert inventory['interfaces'] == 1224
assert inventory['quantum_status'] == {'pending': 784, 'partial': 72, 'metadata-only': 368}
assert 'all entries and contracts match' in (root / 'build/memcmp-inventory-regenerated.log').read_text()
assert '784 pending and 72 partial interfaces' in (root / 'build/memcmp-inventory-completeness.log').read_text()
example = json.loads((root / 'build/memcmp-contract-example-results.json').read_text())
assert example['status'] == 'passed' and len(example['checks']) == 2
oracle_reports = {}
for configuration in ('normal', 'sanitized'):
    directory = root / 'build/memcmp-oracle-results' / configuration
    report = json.loads((directory / 'summary.json').read_text())
    assert report['status'] == 'passed' and len(report['checks']) == 20
    assert report['source_sha256'] == hashlib.sha256((root / 'build/memcmp-oracle.c').read_bytes()).hexdigest()
    assert max(row['maximum_amplitude_error'] for row in report['checks']) < 1e-10
    for row in report['checks']:
        stem = row['optimization'][1:] + row['builtins'][1:]
        state = stem + ('-tables' if row['tables'] else '-no-tables') + '.state.json'
        assert hashlib.sha256((directory / state).read_bytes()).hexdigest() == row['state_sha256']
        assert hashlib.sha256((directory / (stem + '.json')).read_bytes()).hexdigest() == row['ir_sha256']
    oracle_reports[configuration] = report

# Refuse to replace an earlier archive, including a partial one.
destination.mkdir(parents=True, exist_ok=False)
for name, path in logs.items():
    shutil.copyfile(path, destination / name)
for source, name in (
    ('build/memcmp-normal-prefix-sources.tar.gz', 'normal-prefix-sources.tar.gz'),
    ('build/synthesis-growth.csv', 'synthesis-growth.csv'),
    ('build/verify-memcmp-tail.sh', 'verify-tail.sh'),
    ('build/memcmp-inventory-check.log', 'inventory.log'),
    ('build/memcmp-inventory-regenerated.log', 'inventory-regenerated.log'),
    ('build/memcmp-inventory-completeness.log', 'inventory-incomplete.log'),
    ('build/memcmp-contract-example.c', 'contract-example.c'),
    ('build/memcmp-contract-example-results.json', 'contract-example-results.json'),
    ('build/check-memcmp-example.py', 'check-contract-example.py'),
):
    shutil.copyfile(root / source, destination / name)
shutil.copyfile(__file__, destination / 'archive-segments.py')
with tarfile.open(destination / 'oracle-states.tar.gz', 'x:gz') as archive:
    for path in sorted((root / 'build/memcmp-oracle-results').rglob('*')):
        if path.is_file():
            archive.add(path, arcname=path.relative_to(root / 'build').as_posix())
    for name in ('check-memcmp-oracle.py', 'memcmp-oracle.c', 'memcmp-oracle-normal.log', 'memcmp-oracle-sanitized.log'):
        archive.add(root / 'build' / name, arcname=name)
with tarfile.open(destination / 'sources.tar.gz', 'x:gz') as archive:
    for name, data in (
        *inputs.items(),
        ('LICENSE', (root / 'LICENSE').read_bytes()),
        ('verified-sources.json', (json.dumps(final, indent=2) + '\n').encode()),
    ):
        info = tarfile.TarInfo(name)
        info.size = len(data)
        info.mode = 0o755 if name == 'cket' or name.endswith('.sh') else 0o644
        archive.addfile(info, io.BytesIO(data))
with tarfile.open(destination / 'sources.tar.gz') as archive:
    assert set(archive.getnames()) == set(final) | {'LICENSE', 'verified-sources.json'}
    for name, digest in final.items():
        assert hashlib.sha256(archive.extractfile(name).read()).hexdigest() == digest, name

def output(*command):
    return subprocess.check_output(command, cwd=root, text=True).strip()

report = {
    'status': 'passed-in-segments',
    'archived_utc': datetime.now(timezone.utc).isoformat(),
    'scope': 'Normal core suites completed before a reference-importer capacity failure; only the three listed reference-validation source files then changed. The complete normal independent suite and complete ASan/UBSan phase passed on the final frozen source state. This is not an uninterrupted full verify.sh run.',
    'base_commit': output('git', 'rev-parse', 'HEAD'),
    'uncommitted_changes': bool(output('git', 'status', '--porcelain')),
    'compiler': output('gcc-11', '-dumpfullversion'),
    'cxx_compiler': output('g++-11', '-dumpfullversion'),
    'system': output('uname', '-srmo'),
    'libc': output('getconf', 'GNU_LIBC_VERSION'),
    'source_files': len(final),
    'source_identity_scope': 'Pre-build snapshots compared with source/capsule bytes at archival; endpoint identity, not continuous monitoring or proof of correctness.',
    'source_sha256': final,
    'normal_prefix_source_sha256': prefix,
    'changed_inputs_after_normal_prefix': changed,
    'segments': [
        {'log': 'normal-prefix.log', 'sources': 'normal-prefix-sources.tar.gz', 'exit_status': 1, 'command': 'CC=gcc-11 CXX=g++-11 CKET_FUZZ_CASES=128 CKET_FUZZ_SEED=20261012 bash scripts/verify.sh', 'completed': list(expected), 'terminal_failure': failure, 'reason': 'The former reference importer capped qubits at 20; the new two-byte memcmp circuit requires 22.'},
        {'log': 'reference-normal.log', 'sources': 'sources.tar.gz', 'exit_status': 0, 'command': 'CC=gcc-11 bash scripts/build-validation.sh && CC=gcc-11 build/validate_qasm', 'completed': ['all 57 independent circuits', 'reference importer acceptance/rejection resource boundaries']},
        {'log': 'sanitized.log', 'sources': 'sources.tar.gz', 'exit_status': 0, 'command': 'CC=gcc-11 CXX=g++-11 bash build/verify-memcmp-tail.sh', 'completed': ['locale sanitizer guard', *expected, 'all 57 independent circuits', 'identical normal/sanitized growth CSVs'], 'tail': 'verify-tail.sh'},
    ],
    'memcmp_checks_per_configuration': {'complete_phased_state_and_inverse': 92, 'native_programs': 110, 'negative_contracts': 70, 'wide_no_table_generation': 30, 'total': 302},
    'memcmp_state_scope': 'All 256 byte patterns, signed representations and exact aliases across five GCC levels/builtin modes; all 65536 two-byte inputs, byte views and two coherent inputs at O2; small raw-result domains; both table policies and inverse cleanup.',
    'native_scope': 'Six relations against zero and an unsigned-byte raw result each exhaust all 256 byte values at five optimization levels and both builtin modes; separate const borrow, byte-offset, one-past zero count, cast and classical libc cases.',
    'independent_qasm': {'total': 57, 'states': 51, 'channels': 6, 'new_memcmp_states': 4, 'importer_qubit_limit': 24, 'importer_result_bit_limit': 20},
    'supplementary_memcmp_grover': {'artifact': 'oracle-states.tar.gz', 'checks_per_configuration': 20, 'iterations': 12, 'predicted_marked_probability': oracle_reports['normal']['predicted_marked_probability'], 'maximum_amplitude_error': max(row['maximum_amplitude_error'] for report in oracle_reports.values() for row in report['checks']), 'scope': oracle_reports['normal']['scope']},
    'library_index': {'interfaces': 1224, 'pending': 784, 'partial': 72, 'metadata_only': 368},
    'locale_sanitizer_exception': {'frame': 'argz_add_sep', 'file': 'tests/glibc-locale.lsan', 'scope': 'Only validators using the glibc locale fixture; separate guard confirms project and locale-handle leaks remain detectable. GCC process leak attribution is disabled independently in its wrapper.'},
    'remaining': 'Complete quantum C11/library compatibility, mutable CPU byte snapshots, dynamic ranges, general layouts, indirect quantum ABI and physical QPU execution remain unproven or unimplemented. Finite simulator evidence is not an absence-of-bugs proof.',
    'artifact_sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(destination.iterdir()) if p.is_file()},
}
with (destination / 'comparison-summary.json').open('x') as stream:
    json.dump(report, stream, indent=2)
    stream.write('\n')
print(f'Preserved three verification segments, including the failed prefix, and both {len(final)}-input source states.')
