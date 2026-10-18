"""Finalize the maintained isolated pointer suite after its actual two-config success."""
from datetime import datetime, timezone
from pathlib import Path
import hashlib
import importlib.util
import json
import os
import re
import shutil
import subprocess
import tarfile

root = Path(__file__).resolve().parent.parent
candidate = root / 'build/memory-return-maintained'
proof = candidate / 'build/maintained-v2'
destination = root / 'docs/audit-evidence/memory-returns-maintained'
assert not destination.exists()
terminal = json.loads((proof / 'exit.json').read_text())
assert terminal['exit_code'] == 0 and terminal['error'] is None
manifest = json.loads((proof / 'sources-before.json').read_text())
assert len(manifest) == 136
for name, digest in manifest.items():
    assert hashlib.sha256((candidate / name).read_bytes()).hexdigest() == digest, name
for name in ('normal', 'sanitized'):
    text = (proof / (name + '.log')).read_text()
    assert text.splitlines().count('290 memory return checks passed; complete labeled action, native integration and expected rejections have distinct proof scopes.') == 1
    assert not re.search(r'^FAIL\b|ERROR: (?:AddressSanitizer|LeakSanitizer)|runtime error:|internal compiler error:', text, re.M)
contract = candidate / 'docs/c11-memory-returns.md'
text = contract.read_text()
before = ('Its current 136-input campaign has passed 290 normal checks;\n'
          'the instrumented configuration is still in progress.')
if before in text:
    assert text.count(before) == 1
    text = text.replace(before, 'Its 136-input scoped campaign passed 290 checks in each\nnormal and instrumented configuration.')
else:
    assert 'Its 136-input scoped campaign passed 290 checks in each normal and instrumented configuration.' in re.sub(r'\s+', ' ', text)
spec = importlib.util.spec_from_file_location('maintained_contract_format', candidate / 'scripts/format.py')
formatter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(formatter)
contract.write_text(formatter.format_text(contract, text))
for name in manifest:
    path = candidate / name
    if name.endswith(('.c', '.cpp', '.h', '.inc', '.py', '.sh')) or name == 'cket':
        assert formatter.format_text(path, path.read_text()) == path.read_text(), name
for name in ('build', 'verify', 'sanitize-all'):
    subprocess.run(['bash', '-n', str(candidate / 'scripts' / (name + '.sh'))], check=True)
baseline_path = root / 'build/memory-sequence-prototype/docs/c11-library.json'
data = json.loads(baseline_path.read_text())
spec = importlib.util.spec_from_file_location('maintained_inventory', candidate / 'scripts/inventory_c11.py')
indexer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(indexer)
for row in data['interfaces']:
    values = indexer.semantics(row['header'], row['name'], row['kind'], row['profile'] == 'annex-k-optional')
    for field, value in zip(('quantum', 'restriction_classes', 'milestone', 'tests', 'contract'), values):
        row[field] = value
(candidate / 'docs/c11-library.json').write_text(json.dumps(data, indent=2) + '\n')
inventory = subprocess.run(['python3', 'scripts/inventory_c11.py'], cwd=candidate,
                           text=True, capture_output=True, check=True)
(proof / 'inventory.log').write_text(inventory.stdout + inventory.stderr)
rows = json.loads((candidate / 'docs/c11-library.json').read_text())['interfaces']
assert {status: sum(row['quantum'] == status for row in rows) for status in ('pending', 'partial', 'metadata-only')} == {'pending': 782, 'partial': 74, 'metadata-only': 368}
baseline = json.loads((root / 'build/memory-sequence-prototype/docs/c11-library.json').read_text())['interfaces']
assert len(rows) == len(baseline) == 1224
assert [(a['header'], a['name']) for a, b in zip(rows, baseline) if a != b] == [('string.h', 'memcpy'), ('string.h', 'memmove'), ('string.h', 'memset')]
incomplete = subprocess.run(['python3', 'scripts/inventory_c11.py', '--require-complete'], cwd=candidate,
                            text=True, capture_output=True)
assert incomplete.returncode == 1
(proof / 'completeness.log').write_text(incomplete.stdout + incomplete.stderr)
program, = re.findall(r'```c\n(.*?)```', contract.read_text(), re.S)
source = proof / 'contract-example.c'
source.write_text(program)
examples = []
for configuration, stage in [('normal', candidate), ('sanitized', proof / 'sanitized')]:
    environment = dict(os.environ, CC='gcc-11')
    if configuration == 'sanitized':
        environment.update(CC=str(stage / 'build/gcc-wrapper'), CKET_REAL_GCC='gcc-11',
                           UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    result = subprocess.run([str(stage / 'build/cket'), str(source), '--run', '-O2', '-fbuiltin',
                             '-o', str(proof / ('example-' + configuration))], cwd=candidate,
                            env=environment, text=True, capture_output=True, timeout=240)
    examples.append({'configuration': configuration, 'returncode': result.returncode,
                     'stdout': result.stdout, 'stderr': result.stderr})
    assert result.returncode == 0, examples[-1]
(proof / 'contract-example-results.json').write_text(json.dumps(examples, indent=2) + '\n')
for name, digest in manifest.items():
    assert hashlib.sha256((candidate / name).read_bytes()).hexdigest() == digest, name
destination.mkdir()
for source, name in [
    (proof / 'sources-before.json', 'verified-sources.json'), (proof / 'exit.json', 'exit.json'),
    (proof / 'job.json', 'job.json'), (proof / 'runner.py', 'runner.py'),
    (proof / 'provenance.json', 'provenance.json'), (contract, 'contract-draft.md'),
    (candidate / 'docs/c11-library.json', 'inventory.json'),
    *[(proof / (name + '.log'), name + '.log') for name in ('normal-build', 'normal', 'sanitized-build', 'sanitized', 'inventory', 'completeness')],
    (proof / 'contract-example.c', 'contract-example.c'),
    (proof / 'contract-example-results.json', 'contract-example-results.json'),
    (Path(__file__).resolve(), 'archive-checker.py')]:
    shutil.copyfile(source, destination / name)
with tarfile.open(destination / 'sources.tar.gz', 'x:gz') as archive:
    for name in list(manifest) + ['LICENSE']:
        archive.add(candidate / name, arcname=name)
    archive.add(proof / 'sources-before.json', arcname='verified-sources.json')
failure = candidate / 'build/maintained-v1'
with tarfile.open(destination / 'initial-warning.tar.gz', 'x:gz') as archive:
    for name in ('job.json', 'exit.json', 'sources-before.json', 'sources.tar.gz', 'normal-build.log', 'runner.log'):
        archive.add(failure / name, arcname=name)
summary = {'status': 'verified-isolated-scoped', 'archived_utc': datetime.now(timezone.utc).isoformat(),
           'source_files': 136, 'source_sha256': manifest,
           'checks_per_configuration': {'labeled_state_action': 100, 'native': 130, 'specific_rejections': 60,
                                        'total': 290, 'ordinary_C_reference_executions': 26, 'ordinary_C_syntax_checks': 6},
           'configurations': ['normal', 'ASan/UBSan'], 'exact_contract_examples_per_configuration': 1,
           'scope': 'Maintained C validator built separately in both configurations against source-identical previously built compiler/core tools; copied binary hashes and original 134-input base retained. No fresh full compiler rebuild or broad-regression success claimed. Five profiles at five GCC levels, both builtin modes and both synthesis policies. Complete prepared word labels, not dense 49-qubit simulation.',
           'remaining': 'Fresh full broad regression and main promotion remain pending. General pointer/ABI/count/layout and complete C11 work remain open. Cybersecurity stays classical.',
           'artifact_sha256': {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in destination.iterdir() if path.is_file()}}
(destination / 'maintained-summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print('Archived 136-input maintained C pointer suite: 290 normal and 290 ASan/UBSan checks, exact examples, recurring pipeline links and unchanged source hashes. Broad verification/main promotion still pending.')
