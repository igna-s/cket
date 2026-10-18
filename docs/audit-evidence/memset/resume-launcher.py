"""Resume only the interrupted sanitizer tail, keeping the original record."""
from pathlib import Path
import datetime
import hashlib
import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys
import traceback

root = Path(__file__).resolve().parent.parent
candidate = root / 'build/memset-prototype'
build = candidate / 'build'
manifest = build / 'memset-full-sources-before.json'
metadata = build / 'memset-resume-job.json'
terminal = build / 'memset-resume-exit.json'
log = build / 'memset-resume.log'
prefix = build / 'memset-interrupted-prefix.log'


def utc():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def check_sources():
    spec = importlib.util.spec_from_file_location('memset_resume_archiver', candidate / 'scripts/archive-audit.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    frozen = json.loads(manifest.read_text())
    assert len(frozen) == 132 and module.source_hashes() == frozen
    return frozen


if '--worker' in sys.argv:
    status, actual, error = 1, None, None
    try:
        check_sources()
        result = subprocess.run(['bash', str(root / 'build/resume-memset-tail.sh')], cwd=candidate)
        actual = result.returncode
        status = actual
        check_sources()
    except BaseException as failure:
        status = 1
        error = repr(failure)
        traceback.print_exc()
    with terminal.open('x') as handle:
        json.dump({'exit_code': status, 'runner_returncode': actual, 'error': error,
                   'finished_utc': utc(), 'source_inputs': 132}, handle, indent=2)
        handle.write('\n')
    raise SystemExit(status)

assert not any(path.exists() for path in (metadata, terminal, log, prefix))
assert not (build / 'memset-full-exit.json').exists()
job = json.loads((build / 'memset-full-job.json').read_text())
process = Path('/proc') / str(job['pid'])
if process.exists():
    stat = (process / 'stat').read_text().rsplit(')', 1)[1].split()
    assert int(stat[19]) != job['linux_start_ticks'] or stat[0] == 'Z', 'Original job still alive.'
for process in Path('/proc').iterdir():
    if not process.name.isdigit() or int(process.name) == os.getpid():
        continue
    try:
        command = (process / 'cmdline').read_bytes().split(bytes([0]))
    except (FileNotFoundError, ProcessLookupError, PermissionError):
        continue
    assert not any(argument.decode(errors='replace').startswith(str(build / 'sanitized/build/'))
                   for argument in command), 'Original sanitizer validator is still running.'
check_sources()
text = (build / 'memset-full-verify.log').read_text()
spec = importlib.util.spec_from_file_location('memset_resume_patterns', candidate / 'scripts/archive-audit.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
import ast
tree = ast.parse((candidate / 'scripts/archive-audit.py').read_text())
patterns, = [ast.literal_eval(node.value) for node in ast.walk(tree)
             if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'patterns' for t in node.targets)]
remaining = {'examples', 'classical', 'frontend', 'synthesis', 'qasm'}
for name, pattern in patterns.items():
    assert len(re.findall(pattern, text, re.M)) == (1 if name in remaining else 2), name
assert text.count('PASS locale sanitizer boundary:') == 1
assert not re.search(r'^FAIL\b|ERROR: (?:AddressSanitizer|LeakSanitizer)|runtime error:|internal compiler error:|validation command timed out', text, re.M)
assert text.rstrip().endswith('PASS example dynamic_vector    : five GCC levels, runtime contract and exports')
for name in ('test_examples', 'cket', 'cket_backend', 'cket_plugin.so', 'cket_runtime.o', 'gcc-wrapper'):
    assert (build / 'sanitized/build' / name).is_file(), name
with prefix.open('xb') as handle:
    handle.write(text.encode())
environment = dict(os.environ, CC='gcc-11', CXX='g++-11', CKET_FUZZ_CASES='128', CKET_FUZZ_SEED='20261012')
with log.open('x') as handle:
    process = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), '--worker'],
                               cwd=candidate, env=environment, stdin=subprocess.DEVNULL,
                               stdout=handle, stderr=subprocess.STDOUT, start_new_session=True)
stat = (Path('/proc') / str(process.pid) / 'stat').read_text().rsplit(')', 1)[1].split()
with metadata.open('x') as handle:
    json.dump({'pid': process.pid, 'linux_start_ticks': int(stat[19]), 'started_utc': utc(),
               'runner': str(Path(__file__).resolve()),
               'source_manifest_sha256': hashlib.sha256(manifest.read_bytes()).hexdigest(),
               'interrupted_prefix_sha256': hashlib.sha256(prefix.read_bytes()).hexdigest(),
               'scope': 'Original complete normal build and fresh sanitizer build retained. Normal suites and all sanitizer suites through library-boundary checks passed before interruption. Resume restarts the incomplete full sanitizer examples, then classical integration, fresh instrumented validation-tool build, frontend fuzzing, synthesis and QuEST. Source identity checked at endpoints; not one uninterrupted invocation.',
               'interruption': 'Original process identity absent, no terminal record and no original sanitizer validators alive. WSL uptime at observation was about 102 seconds. No exit status is invented for the interrupted invocation.'}, handle, indent=2)
    handle.write('\n')
print(json.dumps({'state': 'started', 'pid': process.pid, 'log': str(log)}))
