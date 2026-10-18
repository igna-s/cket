"""Build/run the maintained C suite on exact sources, with explicit tool-reuse provenance."""
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
import traceback

root = Path(__file__).resolve().parent.parent
candidate = root / 'build/memory-return-maintained'
label = sys.argv[1] if len(sys.argv) > 1 else 'maintained-v1'
assert re.fullmatch('[a-z0-9-]+', label)
destination = candidate / 'build' / label


def utc():
    return datetime.now(timezone.utc).isoformat()


if '--worker' not in sys.argv:
    destination.mkdir(exist_ok=False)
    with (destination / 'runner.log').open('x') as handle:
        worker = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), label, '--worker'],
                                  cwd=candidate, stdin=subprocess.DEVNULL, stdout=handle,
                                  stderr=subprocess.STDOUT, start_new_session=True)
    fields = (Path('/proc') / str(worker.pid) / 'stat').read_text().rsplit(')', 1)[1].split()
    (destination / 'job.json').write_text(json.dumps({'pid': worker.pid, 'linux_start_ticks': int(fields[19]),
        'started_utc': utc(), 'scope': 'Maintained C suite only, normal and ASan/UBSan. Source-identical previously built compiler/core tools reused; no broad full-regression or main integration claim.'}, indent=2) + '\n')
    print(f'Started maintained pointer validator {worker.pid}: {destination}')
    raise SystemExit(0)

commands = []
status, error = 1, None
try:
    spec = importlib.util.spec_from_file_location('maintained_return_sources', candidate / 'scripts/archive-audit.py')
    archiver = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(archiver)
    sources = archiver.source_hashes()
    assert len(sources) == 136
    with (destination / 'sources-before.json').open('x') as handle:
        json.dump(sources, handle, indent=2)
        handle.write('\n')
    base = json.loads((candidate / 'build/maintained-base.json').read_text())
    for name, digest in base['base_sources'].items():
        if name.startswith(('src/', 'include/')) or name == 'cket':
            assert sources[name] == digest, name
    provenance = {'source_files': 136, 'base_source_files': 134,
                  'changed_inputs': sorted(name for name in sources if sources[name] != base['base_sources'].get(name)),
                  'normal_tool_hashes': base['copied_normal_artifacts'], 'sanitizer_tool_hashes': {},
                  'scope': 'New C validator builds against source-identical previously built normal/instrumented compiler, driver, backend, runtime and engine. Only tests/pipeline/inventory inputs changed; no inherited full test success.'}
    for name, digest in base['copied_normal_artifacts'].items():
        assert hashlib.sha256((candidate / 'build' / name).read_bytes()).hexdigest() == digest, name
    (destination / 'runner.py').write_bytes(Path(__file__).read_bytes())

    def run(name, command, environment):
        print(utc(), name, ' '.join(map(str, command)), flush=True)
        with (destination / (name + '.log')).open('x') as handle:
            result = subprocess.run(list(map(str, command)), cwd=candidate, env=environment,
                                    stdout=handle, stderr=subprocess.STDOUT)
        commands.append({'name': name, 'command': list(map(str, command)), 'returncode': result.returncode})
        assert result.returncode == 0, commands[-1]

    environment = dict(os.environ, CC='gcc-11', CXX='g++-11')
    common = ['-std=c11', '-g', '-Wall', '-Wextra', '-Werror', '-Isrc/engine', '-Itests']
    run('normal-build', ['gcc-11', *common, '-O2', 'tests/validate_memory_returns.c',
                        'tests/validation.c', 'build/libcket_quantum.a', '-lm', '-o', 'build/validate_memory_returns'], environment)
    run('normal', [candidate / 'build/validate_memory_returns'], environment)
    stage = destination / 'sanitized'
    (stage / 'build').mkdir(parents=True)
    for name in ('include', 'src', 'tests'):
        (stage / name).symlink_to(candidate / name, target_is_directory=True)
    shutil.copy2(candidate / 'cket', stage / 'cket')
    prior = root / 'build/memory-return-prototype/build/return-sanitized-v1/build'
    for name in ('cket', 'cket_backend', 'cket_plugin.so', 'cket_runtime.o',
                 'libcket_quantum.a', 'gcc-version.txt', 'gcc-wrapper'):
        digest = hashlib.sha256((prior / name).read_bytes()).hexdigest()
        shutil.copy2(prior / name, stage / 'build' / name)
        assert hashlib.sha256((stage / 'build' / name).read_bytes()).hexdigest() == digest
        assert hashlib.sha256((prior / name).read_bytes()).hexdigest() == digest
        provenance['sanitizer_tool_hashes'][name] = digest
    (destination / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n')
    flags = ['-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
    run('sanitized-build', ['gcc-11', *common, *flags, 'tests/validate_memory_returns.c',
                           'tests/validation.c', stage / 'build/libcket_quantum.a', '-lm', '-o', stage / 'build/validate_memory_returns'], environment)
    environment.update(CC=str(stage / 'build/gcc-wrapper'), CKET_REAL_GCC='gcc-11',
                       UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    run('sanitized', [stage / 'build/validate_memory_returns'], environment)
    marker = '290 memory return checks passed; complete labeled action, native integration and expected rejections have distinct proof scopes.'
    for name in ('normal', 'sanitized'):
        text = (destination / (name + '.log')).read_text()
        assert text.splitlines().count(marker) == 1
        assert '26 ordinary C reference executions' in text
        assert not re.search(r'^FAIL\b|ERROR: (?:AddressSanitizer|LeakSanitizer)|runtime error:|internal compiler error:', text, re.M)
    assert archiver.source_hashes() == sources
    status = 0
except BaseException as failure:
    error = repr(failure)
    traceback.print_exc()
(destination / 'exit.json').write_text(json.dumps({'exit_code': status, 'error': error,
    'finished_utc': utc(), 'commands': commands, 'scope': 'Maintained pointer C suite only; complete C11 and broad integration remain pending.'}, indent=2) + '\n')
raise SystemExit(status)
