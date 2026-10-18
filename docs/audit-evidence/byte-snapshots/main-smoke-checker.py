"""Check source identity and archive the separate main-tree smoke checks."""
from datetime import datetime, timezone
from pathlib import Path
import hashlib
import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parent.parent
destination = root / 'docs/audit-evidence/byte-snapshots'
report_path = destination / 'integration-summary.json'
report = json.loads(report_path.read_text())
assert report['status'] == 'verified-isolated-and-applied'
assert 'root_verification' not in report, 'Root completion already recorded.'
spec = importlib.util.spec_from_file_location('main_archiver', root / 'scripts/archive-audit.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
assert module.source_hashes() == report['source_sha256']
assert len(report['source_sha256']) == 129
program, = re.findall(r'```c\n(.*?)```', (root / 'docs/c11-byte-snapshots.md').read_text(), re.S)
example_path = root / 'build/byte-snapshot-contract-example.c'
if sys.argv[1:] == ['--prepare']:
    assert not example_path.exists()
    example_path.write_text(program)
    print('Main sources match all 129 frozen candidate hashes; exact contract example prepared.')
    raise SystemExit(0)
assert sys.argv[1:] == ['--finish']
assert example_path.read_text() == program
marker = '650 CPU byte snapshot checks passed; dynamic addresses, volatile/atomic access and complete quantum C11 remain pending.'
logs = {}
for configuration in ('normal', 'sanitized'):
    path = root / f'build/byte-snapshot-root-{configuration}.log'
    text = path.read_text()
    assert text.splitlines().count(marker) == 1, configuration
    assert not re.search(r'^FAIL\b|ERROR: (?:AddressSanitizer|LeakSanitizer)|runtime error:|internal compiler error:|validation infrastructure failure|validation command timed out', text, re.M)
    logs[f'root-{configuration}.log'] = path
examples = []
for configuration in ('normal', 'sanitized'):
    stage = root if configuration == 'normal' else root / 'build/byte-snapshot-root-sanitized'
    environment = dict(os.environ, CC='gcc-11')
    if configuration == 'sanitized':
        environment['CC'] = str(stage / 'build/gcc-wrapper')
        environment['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
    result = subprocess.run([
        str(stage / 'build/cket'), str(example_path), '--run',
        '-o', str(root / f'build/byte-snapshot-example-{configuration}.out'),
    ], cwd=root, env=environment, text=True, capture_output=True, timeout=240)
    examples.append({'configuration': configuration, 'returncode': result.returncode,
                     'stdout': result.stdout, 'stderr': result.stderr})
    assert result.returncode == 0, examples[-1]
assert module.source_hashes() == report['source_sha256']
for name, path in logs.items():
    shutil.copyfile(path, destination / name)
for source, name in (
    (example_path, 'contract-example.c'),
    (root / 'build/byte-snapshot-root-build.log', 'root-build.log'),
    (root / 'build/run-byte-snapshot-main-smoke.sh', 'main-smoke-checker.sh'),
    (Path(__file__).resolve(), 'main-smoke-checker.py'),
):
    shutil.copyfile(source, destination / name)
(destination / 'contract-example-results.json').write_text(json.dumps(examples, indent=2) + '\n')
report['root_verification'] = {
    'status': 'passed', 'completed_utc': datetime.now(timezone.utc).isoformat(),
    'source_files': 129, 'checks_per_configuration': 650,
    'configurations': ['normal', 'ASan/UBSan'],
    'exact_contract_example_per_configuration': True,
    'scope': 'Separate main-tree fresh normal build and fresh instrumented plugin/driver/backend/engine/runtime/validator build. The 650-check suite and exact current contract example pass in both configurations. The complete broad regression belongs to the source-identical isolated checkout and is not relabeled as a main-tree execution.',
}
report['remaining'] = report['remaining'].removeprefix('Main rebuild and smoke checks pending. ')
report['artifact_sha256'] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                           for p in sorted(destination.iterdir())
                           if p.is_file() and p.name != 'integration-summary.json'}
report_path.write_text(json.dumps(report, indent=2) + '\n')
print('Fresh main normal/ASan/UBSan builds each pass 650 checks and the exact contract example; all 129 source hashes match.')
