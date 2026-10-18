"""Preserve the first broad pointer regression before correcting obsolete fixtures."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import shutil

root = Path(__file__).resolve().parent.parent
candidate = root / 'build/memory-return-maintained'
build = candidate / 'build'
destination = root / 'docs/audit-evidence/memory-returns-regression-failure'
assert not destination.exists()
terminal = json.loads((build / 'return-full-exit.json').read_text())
assert terminal['exit_code'] == terminal['verify_script_returncode'] == 1
manifest = json.loads((build / 'return-full-sources-before.json').read_text())
scoped_directory = root / 'docs/audit-evidence/memory-returns-maintained'
scoped = json.loads((scoped_directory / 'maintained-summary.json').read_text())
assert manifest == scoped['source_sha256'] and len(manifest) == 136
for name, digest in manifest.items():
    assert hashlib.sha256((candidate / name).read_bytes()).hexdigest() == digest, name
log = (build / 'return-full-verify.log').read_text()
assert '389 passed, 2 failed' in log
assert [line for line in log.splitlines() if line.startswith('FAIL ')] == [
    'FAIL reject pointer alias', 'FAIL reject quantum product pointer escape']
destination.mkdir()
for source, name in [
    (build / 'return-full-sources-before.json', 'verified-sources.json'),
    (build / 'return-full-job.json', 'job.json'), (build / 'return-full-exit.json', 'exit.json'),
    (build / 'return-full-verify.log', 'verify.log'), (build / 'verify-native.log', 'native.log'),
    (build / 'verify-build.log', 'build.log'), (root / 'build/launch-memory-return-full.py', 'runner.py'),
    (scoped_directory / 'sources.tar.gz', 'sources.tar.gz'),
    (Path(__file__).resolve(), 'archive-checker.py')]:
    shutil.copyfile(source, destination / name)
summary = {'status': 'failed-preserved', 'archived_utc': datetime.now(timezone.utc).isoformat(),
           'source_files': 136, 'source_sha256': manifest,
           'native_passed': 389, 'native_failed': 2,
           'scope': 'First fresh broad normal build stopped in native tests. Two negative pointer-declaration fixtures compiled successfully. Stronger positive and explicit CPU-escape regressions are pending; no broad success or main promotion claimed.',
           'artifact_sha256': {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in destination.iterdir() if path.is_file()}}
(destination / 'failure-summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print('Preserved the failed 136-input broad campaign: 389 native passes/two obsolete-boundary candidates; no result relabeled as passing.')
