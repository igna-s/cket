"""Archive closed v5 proofs, retained runner failure and still-open load bugs."""
from pathlib import Path
import hashlib
import io
import json
import shutil
import tarfile

root = Path(__file__).resolve().parent.parent
candidate = root / 'build/coherent-body-maintained-v5'
destination = root / 'docs/audit-evidence/controlled-operands-v5'
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
frozen = json.loads((candidate / 'build/maintained-sources.json').read_text())
assert len(frozen) == 138
for name, expected in frozen.items():
    assert digest(candidate / name) == expected
states = {}
for configuration, folder in (('normal', 'states-v1'), ('sanitized', 'states-sanitized-v1')):
    directory = candidate / 'build' / folder
    record = json.loads((directory / 'exit.json').read_text())
    assert record['exit_code'] == 0 and record['complete_state_checks'] == 4000
    assert record['source_sha256'] == frozen
    for row in record['checks']:
        assert digest(directory / row['qasm']) == row['qasm_sha256']
    states[configuration] = {'complete_state_checks': 4000, 'captures': 1000, 'maximum_physical_wires': record['maximum_physical_wires']}
reversible = {}
for configuration, folder in (('normal', 'reversible-state-proof-v1'), ('sanitized', 'reversible-state-proof-sanitized-v2')):
    directory = candidate / 'build' / folder
    record = json.loads((directory / 'exit.json').read_text())
    assert record['source_sha256'] == frozen and record['native_executions'] == 300 and len(record['checks']) == 600
    assert record['exit_code'] == (1 if configuration == 'normal' else 0)
    if configuration == 'normal':
        assert 'FileNotFoundError' in record['error']
    for row in record['checks']:
        assert row['configuration'] == configuration
        for name in ('qasm', 'source', 'ir'):
            assert digest(directory / row[name]) == row[name + '_sha256']
    reversible[configuration] = {'native_executions': 300, 'complete_state_checks': 600,
        'maximum_physical_wires': max(row['physical_wires'] for row in record['checks']),
        'original_runner_exit_code': record['exit_code']}
sanitized = json.loads((candidate / 'build/scoped-sanitized-v1/exit.json').read_text())
assert sanitized['exit_code'] == 0 and sanitized['native_checks'] == 393 and sanitized['controlled_checks'] == 1270
assert sanitized['source_sha256'] == frozen
broad = candidate / 'build/full-campaign-v1'
progress = json.loads((broad / 'progress.json').read_text())
normal = {}
for row in progress['commands']:
    if row['name'] in ('fresh-normal-build', 'normal-native', 'normal-controlled_operands'):
        assert row['returncode'] == 0 and digest(broad / row['log']) == row['log_sha256']
        normal[row['name']] = row
assert len(normal) == 3
assert '393 passed, 0 failed' in (broad / normal['normal-native']['log']).read_text()
assert '1270 controlled operand checks passed;' in (broad / normal['normal-controlled_operands']['log']).read_text()
pending = json.loads((candidate / 'build/pending-cpu-loads-v1/results.json').read_text())
assert pending['accepted_signals'] == 140 and pending['ordinary_c_executions'] == 70
assert pending['source_sha256'] == frozen
assert all(row['compile_returncode'] == 0 and row['run_returncode'] < 0 for row in pending['rows'])
destination.mkdir()
with tarfile.open(destination / 'sources.tar.gz', 'x:gz') as archive:
    for name in [*frozen, 'LICENSE', 'verified-sources.json']:
        data = (json.dumps(frozen, indent=2) + '\n').encode() if name == 'verified-sources.json' else (candidate / name).read_bytes()
        member = tarfile.TarInfo(name)
        member.size, member.mode = len(data), 0o644
        archive.addfile(member, io.BytesIO(data))
with tarfile.open(destination / 'sources.tar.gz') as archive:
    for name, expected in frozen.items():
        assert hashlib.sha256(archive.extractfile(name).read()).hexdigest() == expected
folders = ('states-v1', 'states-sanitized-v1', 'reversible-state-proof-v1',
           'reversible-state-proof-sanitized-v2', 'scoped-sanitized-v1', 'pending-cpu-loads-v1')
suffixes = {'.py', '.json', '.c', '.inc', '.qasm', '.log', '.txt', '.sh'}
with tarfile.open(destination / 'scoped-evidence.tar.gz', 'x:gz') as archive:
    for folder in folders:
        directory = candidate / 'build' / folder
        for path in sorted(directory.rglob('*')):
            if path.is_file() and not path.is_symlink() and path.suffix in suffixes and 'stage' not in path.relative_to(directory).parts:
                archive.add(path, arcname=folder + '/' + path.relative_to(directory).as_posix())
    for name in ('prepare-controlled-v5.py', 'start-controlled-v5-full.py', 'run-controlled-v5-full.py',
                 'run-controlled-v5-sanitized.py', 'start-controlled-v5-states.py', 'run-controlled-v5-states.py',
                 'run-controlled-v5-reversible-states.py', 'start-controlled-v5-reversible-sanitized.py',
                 'run-controlled-v5-reversible-sanitized-v2.py', 'probe-inactive-cpu-loads.py',
                 'capture-controlled-regressions.py'):
        archive.add(root / 'build' / name, arcname='runners/' + name)
for name, row in normal.items():
    shutil.copyfile(broad / row['log'], destination / (name + '.log'))
shutil.copyfile(candidate / 'build/scoped-sanitized-v1/exit.json', destination / 'scoped-sanitized.json')
shutil.copyfile(candidate / 'build/pending-cpu-loads-v1/results.json', destination / 'pending-cpu-loads.json')
shutil.copyfile(Path(__file__), destination / 'archiver.py')
summary = {'status': 'verified-scoped-v5-not-integrated', 'source_files': 138, 'source_sha256': frozen,
    'prior_broad_failure': '../controlled-operands-regression-failure/failure-summary.json',
    'normal_fresh_core_sha256': progress['normal_fresh_core_sha256'],
    'native_checks_per_configuration': 393, 'controlled_operand_checks_per_configuration': 1270,
    'operand_states': states, 'reversible_regressions': reversible,
    'pending_cpu_loads': {'ordinary_c_executions': 70, 'accepted_signals': 140, 'profiles': 7,
        'scope': 'Normal integrated137 and v5 compilers, five GCC levels and both builtin modes. All references pass; all accepted quantum executables fault. This maps the already-open CPU-load family, not successful support.'},
    'broad_campaign_at_archive': {'status': progress['status'], 'closed_commands': len(progress['commands']), 'source_manifest_sha256': progress['source_manifest_sha256']},
    'scope': 'V5 limits selected-domain history to deferred CPU calculations and restores reversible in-place paths for literals and scalar snapshots. Fresh whole normal build and separate fresh sanitizer plugin/validators with source-identical core reuse pass native393 and controlled1270 each. Operand phase proofs total8000 and separate bool/loop source-level inverse proofs total1200 states with600 native executions. Normal bool/loop proof ended exit1 only because the sanitizer terminal file was not yet available; all300 normal executions/600 states are retained and hash-verified. Only the sanitizer continuation was run, ending zero. No overall zero is invented for that original runner.',
    'remaining': 'Full broad regression, guarded main application, phase angles and inactive CPU-load fixes, general ABI and complete C11 remain pending. The integrated compiler still has137 inputs and the historical body bugs; no hardware or speedup claim. Cybersecurity stays classical and unchanged.',
    'artifact_sha256': {path.name: digest(path) for path in destination.iterdir() if path.is_file()}}
(destination / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print('Archived v5:393+1270 per configuration;8000 operand states;1200 bool/loop states;600 native regressions;140 pending CPU-load faults.')
