from pathlib import Path
import hashlib
import io
import json
import shutil
import tarfile

root = Path('/tmp/quantum-audit-isolated-20261017')
candidate = root / 'build/coherent-body-maintained'
checks = candidate / 'build/maintained-checks-v1'
destination = root / 'docs/audit-evidence/controlled-operands-maintained'
destination.mkdir()
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
terminal = json.loads((checks / 'exit.json').read_text())
frozen = json.loads((candidate / 'build/maintained-sources.json').read_text())
assert terminal['exit_code'] == 0 and terminal['source_sha256'] == frozen and len(frozen) == 138
for name, expected in frozen.items():
    assert digest(candidate / name) == expected, name
for row in terminal['configurations']:
    assert row['checks'] == 1270 and row['defined_native_checks'] == 1000
    assert row['undefined_domain_diagnostics'] == 200 and row['effect_diagnostics'] == 60
    assert row['native_homonyms'] == 10 and row['ordinary_c_executions_separate'] == 100
    assert digest(checks / (row['configuration'] + '.log')) == row['log_sha256']
for path in checks.iterdir():
    if path.is_file():
        shutil.copyfile(path, destination / path.name)
for name in ('prepare-maintained-controlled-operands.py', 'finish-maintained-controlled-preparation.py',
             'run-maintained-controlled-operands.py', 'start-controlled-full.py', 'run-controlled-full.py',
             'run-pointer-difference-full.py', 'run-isolated-memory-audit.py',
             'resume-linux-memory-audit-v2.py', 'archive-maintained-controlled-operands.py'):
    shutil.copyfile(root / 'build' / name, destination / name)
for name in ('preparation-original.py', 'preparation-failure.json', 'maintained-sources.json'):
    shutil.copyfile(candidate / 'build' / name, destination / name)
with tarfile.open(destination / 'sources.tar.gz', 'x:gz') as archive:
    for name in [*frozen, 'LICENSE', 'verified-sources.json']:
        data = (json.dumps(frozen, indent=2) + '\n').encode() if name == 'verified-sources.json' else (candidate / name).read_bytes()
        member = tarfile.TarInfo(name)
        member.size, member.mode = len(data), 0o644
        archive.addfile(member, io.BytesIO(data))
with tarfile.open(destination / 'sources.tar.gz') as archive:
    for name, expected in frozen.items():
        assert hashlib.sha256(archive.extractfile(name).read()).hexdigest() == expected
summary = {'status': 'verified-scoped-maintained', 'source_files': 138, 'source_sha256': frozen,
    'base_experiment': '../controlled-operands/experimental-summary.json',
    'checks_per_configuration': {'defined_native': 1000, 'undefined_domain_diagnostics': 200,
        'cpu_effect_diagnostics': 60, 'native_homonyms': 10, 'ordinary_c_executions_separate': 100},
    'scope': terminal['scope'],
    'broad_campaign': 'Fresh normal/sanitizer campaign started separately on the same 138 frozen inputs; no completion claim.',
    'remaining': 'Main application, full regression, phase-angle and inactive invalid CPU-memory fixes, general ABI and complete C11 remain pending. Cybersecurity stays classical and unchanged.',
    'artifact_sha256': {path.name: digest(path) for path in destination.iterdir() if path.is_file()}}
(destination / 'maintained-summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print('Archived1270 normal +1270 sanitizer recurrent checks on138 inputs; original preparation-anchor failure remains preserved.')
