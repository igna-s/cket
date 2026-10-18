"""Retain the new recurrent suite separately from its earlier finite experiment."""
from pathlib import Path
import hashlib
import io
import json
import shutil
import tarfile

root = Path('/tmp/quantum-audit-isolated-20261017')
candidate = root / 'build/pointer-difference-maintained'
build = candidate / 'build'
checks = build / 'maintained-checks-v1'
destination = root / 'docs/audit-evidence/pointer-differences-maintained'
destination.mkdir()
terminal = json.loads((checks / 'exit.json').read_text())
frozen = json.loads((build / 'maintained-sources.json').read_text())
assert terminal['status'] == 'passed-scoped-maintained' and terminal['exit_code'] == 0
assert terminal['source_sha256'] == frozen and len(frozen) == 137
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
for name, expected in frozen.items():
    assert digest(candidate / name) == expected, name
for row in terminal['configurations']:
    assert row['returncode'] == 0 and row['checks'] == 150
    assert digest(checks / (row['configuration'] + '.log')) == row['log_sha256']
    assert row['native_checks'] == 80 and row['diagnostic_checks'] == 50
    assert row['action_and_inverse_circuits'] == 20 and row['ordinary_c_executions_separate'] == 40
for path in sorted(checks.iterdir()):
    if path.is_file():
        shutil.copyfile(path, destination / path.name)
shutil.copyfile(build / 'maintained-sources.json', destination / 'verified-sources.json')
for name in ('prepare-maintained-pointer-differences.py', 'finish-maintained-pointer-preparation.py',
             'run-maintained-pointer-differences.py', 'archive-maintained-pointer-differences.py'):
    shutil.copyfile(root / 'build' / name, destination / name)
with tarfile.open(destination / 'sources.tar.gz', 'x:gz') as archive:
    for name in [*frozen, 'LICENSE', 'verified-sources.json']:
        data = (build / 'maintained-sources.json').read_bytes() if name == 'verified-sources.json' else (candidate / name).read_bytes()
        member = tarfile.TarInfo(name)
        member.size, member.mode = len(data), 0o755 if name == 'cket' or name.endswith('.sh') else 0o644
        archive.addfile(member, io.BytesIO(data))
summary = {'status': 'verified-scoped-maintained', 'source_files': 137, 'source_sha256': frozen,
           'base_experiment': '../pointer-differences/experimental-summary.json',
           'checks_per_configuration': {'native': 80, 'diagnostic': 50, 'action_and_inverse_circuits': 20,
                                        'state_comparisons_inside_circuits': 40, 'ordinary_c_executions_separate': 40},
           'recurrent_paths': ['tests/validate_pointer_differences.c', 'scripts/build.sh',
                               'scripts/sanitize-all.sh', 'scripts/verify.sh', 'scripts/archive-audit.py'],
           'scope': terminal['scope'],
           'remaining': 'Full broad regression and main application remain open. Ten diagnostic checks explicitly retain the real unsupported CPU-scalar coherent-control context; rejecting it does not implement it. General quantum C11 remains incomplete and cybersecurity remains classical.',
           'artifact_sha256': {path.name: digest(path) for path in destination.iterdir() if path.is_file()}}
(destination / 'maintained-summary.json').write_text(json.dumps(summary, indent=2) + '\n')
with tarfile.open(destination / 'sources.tar.gz') as archive:
    assert set(archive.getnames()) == set(frozen) | {'LICENSE', 'verified-sources.json'}
    for name, expected in frozen.items():
        assert hashlib.sha256(archive.extractfile(name).read()).hexdigest() == expected, name
print('Archived 150 normal and 150 sanitizer recurrent checks on 137 frozen inputs; main remains the verified 134-input memory batch.')
