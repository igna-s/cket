"""Launch the fresh, frozen normal/sanitized memset integration campaign."""
from pathlib import Path
import datetime
import hashlib
import importlib.util
import json
import os
import shutil
import subprocess
import sys
import traceback

root = Path(__file__).resolve().parent.parent
candidate = root / 'build/memset-prototype'
base = root / 'build/byte-snapshot-prototype'
output = candidate / 'build'
manifest = output / 'memset-full-sources-before.json'
terminal = output / 'memset-full-exit.json'
metadata = output / 'memset-full-job.json'
log = output / 'memset-full-verify.log'

def utc():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()

def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

archiver = load_module('memset_full_archiver', candidate / 'scripts/archive-audit.py')

def check_sources():
    frozen = json.loads(manifest.read_text())
    assert len(frozen) == 132 and archiver.source_hashes() == frozen

if '--worker' in sys.argv:
    status, actual, detail = 1, None, None
    try:
        check_sources()
        print('Fresh memset integration: 132 frozen inputs; complete normal and ASan/UBSan pipeline.', flush=True)
        result = subprocess.run(['bash', 'scripts/verify.sh'], cwd=candidate)
        actual = result.returncode
        status = actual
        check_sources()
    except BaseException as error:
        detail = repr(error)
        traceback.print_exc()
        status = 1
    with terminal.open('x') as handle:
        json.dump({'finished_utc': utc(), 'exit_code': status,
                   'verify_script_returncode': actual, 'error': detail,
                   'source_identity': '132 hashes checked before the fresh normal build and after the campaign; endpoint checks, not continuous monitoring.'}, handle, indent=2)
        handle.write('\n')
    raise SystemExit(status)

assert not any(path.exists() for path in (manifest, terminal, metadata, log))
evidence = output / 'validator-v2-evidence'
assert json.loads((evidence / 'results.json').read_text())['status'] == 'passed-exploratory'
old = json.loads((evidence / 'verified-sources.json').read_text())
current = archiver.source_hashes()
assert len(current) == len(old) == 132 and current.keys() == old.keys()
changes = sorted(name for name in current if current[name] != old[name])
assert changes == sorted([
    'include/string.h', 'src/quantum_memory.inc', 'scripts/build.sh',
    'scripts/sanitize-all.sh', 'scripts/verify.sh', 'tests/validate_library.c',
    'tests/validate_qasm.c', 'scripts/archive-audit.py', 'scripts/inventory_c11.py',
]), changes
base_sources = json.loads((base / 'build/byte-snapshot-verified-sources.json').read_text())
for name, digest in base_sources.items():
    assert hashlib.sha256((base / name).read_bytes()).hexdigest() == digest, name
archiver.ROOT = root
main = json.loads((root / 'docs/audit-evidence/strncmp/integration-summary.json').read_text())
assert archiver.source_hashes() == main['source_sha256'], 'Preserve changed main inputs.'
archiver.ROOT = candidate
formatter = load_module('memset_full_formatter', candidate / 'scripts/format.py')
for name in current:
    path = candidate / name
    text = path.read_text()
    assert formatter.format_text(path, text) == text, name
print('Checked formatting of all 132 isolated source inputs.', flush=True)
archive_source = base / 'build/validation-deps/quest-3.7.0.tar.gz'
assert hashlib.sha256(archive_source.read_bytes()).hexdigest() == '1a5939798a70b749dfa57cfaa116ece03a2d36aa59c1b712cb3dc2103ab1cded'
archive_target = output / 'validation-deps/quest-3.7.0.tar.gz'
archive_target.parent.mkdir(exist_ok=True)
assert archive_target.resolve().is_relative_to(candidate.resolve())
if archive_target.exists():
    assert archive_target.read_bytes() == archive_source.read_bytes()
else:
    shutil.copyfile(archive_source, archive_target)
subprocess.run([sys.executable, 'scripts/archive-audit.py', '--freeze', str(manifest)],
               cwd=candidate, check=True)
check_sources()
environment = os.environ.copy()
environment.update(CC='gcc-11', CXX='g++-11', CKET_FUZZ_CASES='128', CKET_FUZZ_SEED='20261012')
with log.open('x') as handle:
    process = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), '--worker'],
                               cwd=candidate, env=environment, stdin=subprocess.DEVNULL,
                               stdout=handle, stderr=subprocess.STDOUT, start_new_session=True)
fields = (Path('/proc') / str(process.pid) / 'stat').read_text().rsplit(')', 1)[1].split()
with metadata.open('x') as handle:
    json.dump({'pid': process.pid, 'linux_start_ticks': int(fields[19]),
               'runner': str(Path(__file__).resolve()), 'started_utc': utc(),
               'runner_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
               'source_manifest_sha256': hashlib.sha256(manifest.read_bytes()).hexdigest(),
               'command': 'CC=gcc-11 CXX=g++-11 CKET_FUZZ_CASES=128 CKET_FUZZ_SEED=20261012 bash scripts/verify.sh',
               'proof_scope': 'Fresh complete normal/sanitized isolated campaign. Source identity checked at start and terminal endpoint. Main compiler and the separate byte-snapshot candidate were unchanged at launch.'}, handle, indent=2)
    handle.write('\n')
print(json.dumps({'state': 'started', 'pid': process.pid, 'log': str(log)}))
