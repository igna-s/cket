"""Run the complete isolated pipeline after six checked CI/test changes."""
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
candidate = root / 'build/memory-sequence-prototype'
build = candidate / 'build'
manifest = build / 'sequence-full-sources-before.json'
metadata = build / 'sequence-full-job.json'
terminal = build / 'sequence-full-exit.json'
log = build / 'sequence-full-verify.log'


def utc():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def module_at(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


archiver = module_at('sequence_full_archiver', candidate / 'scripts/archive-audit.py')


def check_sources():
    frozen = json.loads(manifest.read_text())
    assert len(frozen) == 134 and archiver.source_hashes() == frozen


if '--worker' in sys.argv:
    status, actual, error = 1, None, None
    try:
        check_sources()
        print('Fresh complete normal/ASan/UBSan campaign on 134 frozen inputs; 360 sequence checks wired into both paths; 70 QuEST circuits expected.', flush=True)
        result = subprocess.run(['bash', 'scripts/verify.sh'], cwd=candidate)
        actual = result.returncode
        status = actual
        check_sources()
    except BaseException as failure:
        status = 1
        error = repr(failure)
        traceback.print_exc()
    with terminal.open('x') as handle:
        json.dump({'exit_code': status, 'verify_script_returncode': actual, 'error': error,
                   'finished_utc': utc(), 'source_inputs': 134}, handle, indent=2)
        handle.write('\n')
    raise SystemExit(status)

assert not any(path.exists() for path in (manifest, metadata, terminal, log))
before = json.loads((build / 'sequence-validation-v1-sources.json').read_text())
current = archiver.source_hashes()
assert len(current) == len(before) == 134 and current.keys() == before.keys()
changed = sorted(name for name in current if current[name] != before[name])
assert changed == sorted(['scripts/build.sh', 'scripts/verify.sh', 'scripts/sanitize-all.sh',
                          'scripts/archive-audit.py', 'scripts/inventory_c11.py', 'tests/validate_qasm.c']), changed
for label, status in [('sequence-normal-evidence', 'passed-scoped-normal'),
                      ('sequence-sanitized-evidence', 'passed-scoped-ASan-UBSan')]:
    directory = build / label
    report = json.loads((directory / 'results.json').read_text())
    assert report['status'] == status
    for name, digest in report['artifact_sha256'].items():
        assert hashlib.sha256((directory / name).read_bytes()).hexdigest() == digest, (label, name)
formatter = module_at('sequence_full_formatter', candidate / 'scripts/format.py')
for name in current:
    path = candidate / name
    assert formatter.format_text(path, path.read_text()) == path.read_text(), name
index = json.loads((candidate / 'docs/c11-library.json').read_text())
old_index = json.loads((root / 'build/memset-prototype/docs/c11-library.json').read_text())
diff = [(a['header'], a['name']) for a, b in zip(old_index['interfaces'], index['interfaces']) if a != b]
assert diff == [('string.h', 'memcpy'), ('string.h', 'memmove'), ('string.h', 'memset')], diff
assert {key: sum(row['quantum'] == key for row in index['interfaces'])
        for key in ('pending', 'partial', 'metadata-only')} == {'pending': 782, 'partial': 74, 'metadata-only': 368}
quest = root / 'build/memset-prototype/build/validation-deps/quest-3.7.0.tar.gz'
assert hashlib.sha256(quest.read_bytes()).hexdigest() == '1a5939798a70b749dfa57cfaa116ece03a2d36aa59c1b712cb3dc2103ab1cded'
target = build / 'validation-deps/quest-3.7.0.tar.gz'
target.parent.mkdir(exist_ok=True)
if target.exists():
    assert target.read_bytes() == quest.read_bytes()
else:
    shutil.copyfile(quest, target)
subprocess.run([sys.executable, 'scripts/archive-audit.py', '--freeze', str(manifest)], cwd=candidate, check=True)
check_sources()
environment = dict(os.environ, CC='gcc-11', CXX='g++-11', CKET_FUZZ_CASES='128', CKET_FUZZ_SEED='20261012')
with log.open('x') as handle:
    process = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), '--worker'],
                               cwd=candidate, env=environment, stdin=subprocess.DEVNULL,
                               stdout=handle, stderr=subprocess.STDOUT, start_new_session=True)
stat = (Path('/proc') / str(process.pid) / 'stat').read_text().rsplit(')', 1)[1].split()
with metadata.open('x') as handle:
    json.dump({'pid': process.pid, 'linux_start_ticks': int(stat[19]), 'started_utc': utc(),
               'runner': str(Path(__file__).resolve()), 'runner_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
               'source_manifest_sha256': hashlib.sha256(manifest.read_bytes()).hexdigest(),
               'delta_from_scoped_campaign': changed,
               'scope': 'Fresh full normal and ASan/UBSan verification in the isolated 134-input checkout. Earlier scoped normal/sanitizer and independent export evidence retained unchanged. Compiler implementation and the 360-check validator are byte-identical to those earlier proofs; six pipeline/reference-test inputs changed. Endpoint source checks, not continuous monitoring.'}, handle, indent=2)
    handle.write('\n')
print(json.dumps({'state': 'started', 'pid': process.pid, 'log': str(log)}))
