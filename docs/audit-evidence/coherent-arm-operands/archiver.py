"""Preserve eight closed exploratory probes and the exact two source identities."""
from pathlib import Path
import hashlib
import io
import json
import shutil
import tarfile

root = Path('/tmp/quantum-audit-isolated-20261017')
directory = root / 'build/coherent-arm-operand-probes-v1'
destination = root / 'docs/audit-evidence/coherent-arm-operands'
destination.mkdir()
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
results = json.loads((directory / 'results.json').read_text())
rows = results['rows']
assert len(rows) == 8
assert all(row['ordinary_c_returncode'] == 0 for row in rows)
assert all(row['main134']['compile_returncode'] == 0 and row['main134']['run_returncode'] == -8 for row in rows)
assert all(row['corrected137']['compile_returncode'] == 0 and
           row['corrected137']['run_returncode'] == (0 if row['name'] == 'condition-control' else -8)
           for row in rows)
sources = {}
for label, owner, metadata in (
        ('main134', root, 'docs/audit-evidence/memory-expressions/integration-summary.json'),
        ('corrected137', root / 'build/coherent-cpu-maintained',
         'docs/audit-evidence/coherent-cpu-conditions-maintained/maintained-summary.json')):
    frozen = json.loads((root / metadata).read_text())['source_sha256']
    for name, expected in frozen.items():
        assert digest(owner / name) == expected, (label, name)
    assert all(row[label]['compiler_sha256'] == digest(owner / 'build/cket_plugin.so') for row in rows)
    sources[label] = {'source_files': len(frozen), 'source_sha256': frozen,
                      'evidence_summary': '../' + metadata.split('docs/audit-evidence/', 1)[1]}
for row in rows:
    assert digest(directory / (row['name'] + '.c')) == row['source_sha256']
shutil.copyfile(directory / 'results.json', destination / 'results.json')
shutil.copyfile(root / 'build/probe-coherent-arm-operands.py', destination / 'runner.py')
shutil.copyfile(Path(__file__), destination / 'archiver.py')
with tarfile.open(destination / 'fixtures.tar.gz', 'x:gz') as archive:
    for path in sorted(directory.glob('*.c')):
        data = path.read_bytes()
        member = tarfile.TarInfo(path.name)
        member.size, member.mode = len(data), 0o644
        archive.addfile(member, io.BytesIO(data))
summary = {'status': 'accepted-program-bugs-reproduced', 'sources': sources,
           'ordinary_c_executions_separate': 8, 'quantum_native_executions_per_compiler': 8,
           'optimization': '-O2', 'builtins': '-fbuiltin',
           'remaining_failures_on_corrected_candidate': 7,
           'scope': 'The candidate fixes the nested-condition control case only. Seven accepted '
                    'programs still terminate with SIGFPE for inactive controlled-body operands '
                    '(xor, add, assignment, mixed add, memset, abs, phase). No fix or all-level '
                    'matrix is claimed for those seven profiles. No cybersecurity changes.',
           'artifact_sha256': {path.name: digest(path) for path in destination.iterdir() if path.is_file()}}
(destination / 'failure-summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print('Archived eight references and eight executions per compiler; seven body-operand failures remain in the corrected candidate.')
