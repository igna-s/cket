"""Resume the confirmed interrupted 134-input campaign without inventing its exit code."""
from datetime import datetime, timezone
from pathlib import Path
import ast
import hashlib
import importlib.util
import json
import os
import re
import subprocess
import sys
import traceback

root = Path(__file__).resolve().parent.parent
candidate = root / 'build/memory-sequence-prototype'
build = candidate / 'build'
manifest = build / 'sequence-full-sources-before.json'
metadata = build / 'sequence-resume-job.json'
terminal = build / 'sequence-resume-exit.json'
log = build / 'sequence-resume.log'
prefix = build / 'sequence-interrupted-prefix.log'
spec = importlib.util.spec_from_file_location('sequence_resume_sources', candidate / 'scripts/archive-audit.py')
archiver = importlib.util.module_from_spec(spec)
spec.loader.exec_module(archiver)


def utc():
    return datetime.now(timezone.utc).isoformat()


def check_sources():
    frozen = json.loads(manifest.read_text())
    assert len(frozen) == 134 and archiver.source_hashes() == frozen


if '--worker' in sys.argv:
    status, actual, error = 1, None, None
    try:
        check_sources()
        result = subprocess.run(['bash', str(root / 'build/resume-memory-sequence-tail.sh')], cwd=candidate)
        actual = result.returncode
        status = actual
        check_sources()
    except BaseException as failure:
        status, error = 1, repr(failure)
        traceback.print_exc()
    record = {'exit_code': status, 'resume_returncode': actual, 'error': error,
              'finished_utc': utc(), 'source_inputs': 134, 'mode': 'passed-in-segments' if status == 0 else 'failed-resume',
              'original_verify_script_returncode': None, 'verify_script_returncode': None}
    if status == 0:
        (build / 'sequence-full-verify.log').write_bytes(prefix.read_bytes() + log.read_bytes())
        with (build / 'sequence-full-exit.json').open('x') as handle:
            json.dump(record, handle, indent=2)
            handle.write('\n')
    with terminal.open('x') as handle:
        json.dump(record, handle, indent=2)
        handle.write('\n')
    raise SystemExit(status)

assert not any(path.exists() for path in (metadata, terminal, log, prefix, build / 'sequence-full-exit.json'))
job = json.loads((build / 'sequence-full-job.json').read_text())
process = Path('/proc') / str(job['pid'])
if process.exists():
    fields = (process / 'stat').read_text().rsplit(')', 1)[1].split()
    assert int(fields[19]) != job['linux_start_ticks'] or fields[0] == 'Z', 'Original campaign is still live.'
check_sources()
text = (build / 'sequence-full-verify.log').read_text()
tree = ast.parse((candidate / 'scripts/archive-audit.py').read_text())
patterns, = [ast.literal_eval(node.value) for node in ast.walk(tree) if isinstance(node, ast.Assign)
             and any(isinstance(target, ast.Name) and target.id == 'patterns' for target in node.targets)]
remaining = {'library', 'examples', 'classical', 'frontend', 'synthesis', 'qasm'}
for name, pattern in patterns.items():
    assert len(re.findall(pattern, text, re.M)) == (1 if name in remaining else 2), name
assert text.count('PASS locale sanitizer boundary:') == 1
assert text.rstrip().splitlines()[-1].startswith('PASS libc boundary ')
assert not re.search(r'^FAIL\b|ERROR: (?:AddressSanitizer|LeakSanitizer)|runtime error:|internal compiler error:', text, re.M)
tools = {name: hashlib.sha256((build / 'sanitized/build' / name).read_bytes()).hexdigest()
         for name in ('validate_library', 'test_examples', 'cket', 'cket_backend', 'cket_plugin.so',
                      'cket_runtime.o', 'libcket_quantum.a', 'gcc-wrapper', 'gcc-version.txt')}
with prefix.open('xb') as handle:
    handle.write(text.encode())
environment = dict(os.environ, CC='gcc-11', CXX='g++-11', CKET_FUZZ_CASES='128', CKET_FUZZ_SEED='20261012')
with log.open('x') as handle:
    worker = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), '--worker'], cwd=candidate,
                              env=environment, stdin=subprocess.DEVNULL, stdout=handle,
                              stderr=subprocess.STDOUT, start_new_session=True)
fields = (Path('/proc') / str(worker.pid) / 'stat').read_text().rsplit(')', 1)[1].split()
metadata.write_text(json.dumps({'pid': worker.pid, 'linux_start_ticks': int(fields[19]), 'started_utc': utc(),
    'runner': str(Path(__file__).resolve()), 'source_manifest_sha256': hashlib.sha256(manifest.read_bytes()).hexdigest(),
    'interrupted_prefix_sha256': hashlib.sha256(prefix.read_bytes()).hexdigest(), 'instrumented_tool_hashes_at_resume': tools,
    'scope': 'Original fresh normal and sanitizer core builds retained on identical frozen inputs. Original normal suites and sanitizer suites through numeric-locale passed. Resume reruns the entire incomplete library suite, examples and remaining classical/fuzz/growth/QuEST tail. Instrumented tool hashes captured at resume, not before the interrupted build. Original exit status is unknown; successful completion will be explicitly segmented.'}, indent=2) + '\n')
print(f'Resumed original 134-input campaign: {worker.pid}; original interrupted prefix preserved.')
