"""Close application metadata after all actual checks passed; never rebuild."""
from pathlib import Path
import hashlib
import importlib.util
import json
import os
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / 'build/coherent-integration-v1'
DEST = ROOT / 'docs/audit-evidence/coherent-memory-integration'
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')


commands = json.loads((OUT / 'application-commands.json').read_text())
assert len(commands) == 3
for row in commands:
    assert row['returncode'] == 0 and digest(OUT / row['log']) == row['log_sha256']
for name in ('paper-audit.json', 'paper-audit-en.json'):
    row = json.loads((ROOT / 'docs/paper/evidence' / name).read_text())
    assert row['status'] == 'passed' and row['checks'] == 352
failure = {'status': 'postcheck-summary-assertion-corrected',
           'original_application_returncode': 1,
           'reason': 'Finalizer expected 349 paper checks; both actual passing reports contain 352 because new document links also add checks. All three underlying commands had actual exit zero. Compiler builds and tests were not repeated.',
           'original_runner_sha256': digest(DEST / 'application-runner.py'),
           'original_paper_reports': {name: digest(ROOT / 'docs/paper/evidence' / name)
                                     for name in ('paper-audit.json', 'paper-audit-en.json')}}
shutil.copyfile(DEST / 'application-runner.py', DEST / 'application-runner-before-summary-fix.py')
shutil.copyfile(ROOT / 'build/apply-coherent-integration.py', DEST / 'application-runner.py')
shutil.copyfile(Path(__file__), DEST / 'completion-runner.py')
write_json(DEST / 'postcheck-summary-recovery.json', failure)
summary_path = DEST / 'integration-summary.json'
summary = json.loads(summary_path.read_text())
summary['postcheck_recovery'] = failure['reason']
summary['artifact_sha256'] = {path.name: digest(path) for path in DEST.iterdir()
                             if path.is_file() and path != summary_path}
write_json(summary_path, summary)
log = OUT / 'application-4.log'
argv = [str(ROOT / '.deps/paper-venv/bin/python'), 'docs/paper/audit_paper.py', '--language', 'all']
with log.open('x') as handle:
    result = subprocess.run(argv, cwd=ROOT, env=dict(os.environ, CC='gcc-11', CXX='g++-11'),
                            stdout=handle, stderr=subprocess.STDOUT)
assert result.returncode == 0, log.read_text()[-5000:]
commands.append({'command': argv, 'returncode': result.returncode,
                 'log': log.name, 'log_sha256': digest(log),
                 'scope': 'Paper manifest refreshed only because public metadata recovery artifacts were added.'})
write_json(OUT / 'application-commands.json', commands)
source = (ROOT / 'build/apply-coherent-integration.py').read_text()
marker = "for name in ('paper-audit.json', 'paper-audit-en.json'):\n"
assert source.count(marker) == 1
tail = source[source.index(marker):]
before = json.loads((OUT / 'application-before.json').read_text())
frozen = summary['source_sha256']
spec = importlib.util.spec_from_file_location('completion_source_identity', ROOT / 'scripts/archive-audit.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
assert module.source_hashes() == frozen
namespace = dict(globals())
import io
import tarfile
namespace.update(io=io, tarfile=tarfile)
exec(compile(tail, 'application-completion-tail', 'exec'), namespace)
