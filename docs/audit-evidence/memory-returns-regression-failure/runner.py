"""Run one fresh broad normal/sanitizer campaign on the maintained pointer inputs."""
from datetime import datetime, timezone
from pathlib import Path
import hashlib
import importlib.util
import json
import os
import shutil
import subprocess
import sys
import traceback

root = Path(__file__).resolve().parent.parent
candidate = root / 'build/memory-return-maintained'
build = candidate / 'build'
manifest = build / 'return-full-sources-before.json'
metadata = build / 'return-full-job.json'
terminal = build / 'return-full-exit.json'
log = build / 'return-full-verify.log'
spec = importlib.util.spec_from_file_location('return_full_archiver', candidate / 'scripts/archive-audit.py')
archiver = importlib.util.module_from_spec(spec)
spec.loader.exec_module(archiver)


def utc():
    return datetime.now(timezone.utc).isoformat()


def check_sources():
    frozen = json.loads(manifest.read_text())
    assert len(frozen) == 136 and archiver.source_hashes() == frozen


if '--worker' in sys.argv:
    status, actual, error = 1, None, None
    try:
        check_sources()
        print('Fresh broad normal/ASan/UBSan verification on 136 frozen pointer-return inputs; 290 recurring return checks and 70 QuEST circuits per configuration expected.', flush=True)
        result = subprocess.run(['bash', 'scripts/verify.sh'], cwd=candidate)
        actual = result.returncode
        status = actual
        check_sources()
    except BaseException as failure:
        error = repr(failure)
        status = 1
        traceback.print_exc()
    terminal.write_text(json.dumps({'exit_code': status, 'verify_script_returncode': actual, 'error': error,
                                    'source_inputs': 136, 'finished_utc': utc()}, indent=2) + '\n')
    raise SystemExit(status)

assert not any(path.exists() for path in (manifest, metadata, terminal, log))
scoped_directory = root / 'docs/audit-evidence/memory-returns-maintained'
scoped = json.loads((scoped_directory / 'maintained-summary.json').read_text())
assert scoped['status'] == 'verified-isolated-scoped'
assert scoped['checks_per_configuration']['total'] == 290
for name, digest in scoped['artifact_sha256'].items():
    assert hashlib.sha256((scoped_directory / name).read_bytes()).hexdigest() == digest, name
assert archiver.source_hashes() == scoped['source_sha256']
with manifest.open('x') as handle:
    json.dump(scoped['source_sha256'], handle, indent=2)
    handle.write('\n')
quest = root / 'build/memory-sequence-prototype/build/validation-deps/quest-3.7.0.tar.gz'
assert hashlib.sha256(quest.read_bytes()).hexdigest() == '1a5939798a70b749dfa57cfaa116ece03a2d36aa59c1b712cb3dc2103ab1cded'
target = build / 'validation-deps/quest-3.7.0.tar.gz'
target.parent.mkdir(exist_ok=True)
shutil.copyfile(quest, target)
check_sources()
environment = dict(os.environ, CC='gcc-11', CXX='g++-11', CKET_FUZZ_CASES='128', CKET_FUZZ_SEED='20261012')
with log.open('x') as handle:
    worker = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), '--worker'], cwd=candidate,
                              env=environment, stdin=subprocess.DEVNULL, stdout=handle,
                              stderr=subprocess.STDOUT, start_new_session=True)
fields = (Path('/proc') / str(worker.pid) / 'stat').read_text().rsplit(')', 1)[1].split()
metadata.write_text(json.dumps({'pid': worker.pid, 'linux_start_ticks': int(fields[19]), 'started_utc': utc(),
    'runner': str(Path(__file__).resolve()), 'runner_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
    'source_manifest_sha256': hashlib.sha256(manifest.read_bytes()).hexdigest(),
    'scope': 'Fresh complete normal and ASan/UBSan compiler/core/tools and all broad suites on the maintained 136-input pointer worktree. Earlier 134/136-input scoped proofs remain distinct. No main promotion; no new cybersecurity behavior.'}, indent=2) + '\n')
print(f'Started fresh broad pointer campaign {worker.pid}: {log}')
