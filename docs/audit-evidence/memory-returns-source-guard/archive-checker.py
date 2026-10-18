"""Preserve a source-identity stop that happened before compiler/test execution."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import shutil

root = Path(__file__).resolve().parent.parent
build = root / 'build/memory-return-maintained/build'
destination = root / 'docs/audit-evidence/memory-returns-source-guard'
assert not destination.exists()
terminal = json.loads((build / 'return-full-v2-exit.json').read_text())
assert terminal['exit_code'] == 1 and terminal['verify_script_returncode'] is None
assert terminal['error'] == 'AssertionError()'
log = (build / 'return-full-v2-verify.log').read_text()
assert 'check_sources()' in log and 'Fresh broad normal/' not in log
job = json.loads((build / 'return-full-v2-job.json').read_text())
runner = root / 'build/launch-memory-return-full.py'
assert hashlib.sha256(runner.read_bytes()).hexdigest() == job['runner_sha256']
destination.mkdir()
for source, name in [
    (build / 'return-full-v2-sources-before.json', 'frozen-sources.json'),
    (build / 'return-full-v2-job.json', 'job.json'), (build / 'return-full-v2-exit.json', 'exit.json'),
    (build / 'return-full-v2-verify.log', 'verify.log'), (runner, 'runner.py'),
    (Path(__file__).resolve(), 'archive-checker.py')]:
    shutil.copyfile(source, destination / name)
summary = {'status': 'failed-source-guard-preserved', 'archived_utc': datetime.now(timezone.utc).isoformat(),
           'source_files_in_snapshot': 136, 'source_capsule_available': False,
           'scope': 'Initial source-identity check stopped the worker before verify.sh, compilation or tests. Frozen hashes do not describe the final formatted tree. Only the original hashes/metadata/error are preserved; no exact source capsule or quantum semantic failure is claimed. A later freeze starts after formatting has finished.',
           'artifact_sha256': {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in destination.iterdir() if path.is_file()}}
(destination / 'guard-summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print('Preserved the initial source-guard stop separately from compiler/test failures; no source capsule invented.')
