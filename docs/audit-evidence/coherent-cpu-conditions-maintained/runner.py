"""Run newly compiled recurrent validators with the already verified frontend."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import hashlib
import importlib.util
import json
import os
import shutil
import subprocess
import sys
import traceback

ROOT = Path(__file__).resolve().parent.parent
CANDIDATE = ROOT / 'build/coherent-cpu-maintained'
OUT = CANDIDATE / 'build/maintained-checks-v1'
BASE = ROOT / 'build/coherent-cpu-branch-v3'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


if '--worker' not in sys.argv:
    OUT.mkdir()
    shutil.copyfile(Path(__file__), OUT / 'runner.py')
    with (OUT / 'worker.log').open('x') as log:
        worker = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), '--worker'],
            cwd=CANDIDATE, stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT,
            start_new_session=True)
    fields = (Path('/proc') / str(worker.pid) / 'stat').read_text().rsplit(')', 1)[1].split()
    (OUT / 'job.json').write_text(json.dumps({'pid': worker.pid, 'linux_start_ticks': int(fields[19]),
        'boot_id': Path('/proc/sys/kernel/random/boot_id').read_text().strip()}, indent=2) + '\n')
    print('Started recurrent pointer-distance validator:', worker.pid)
    raise SystemExit(0)


def run(configuration):
    stage = CANDIDATE if configuration == 'normal' else OUT / 'sanitized'
    if configuration != 'normal':
        (stage / 'build').mkdir(parents=True)
        for name in ('include', 'src', 'tests', 'examples'):
            (stage / name).symlink_to(CANDIDATE / name, target_is_directory=True)
        shutil.copyfile(CANDIDATE / 'cket', stage / 'cket')
    source = BASE / ('build' if configuration == 'normal' else 'build/sanitized-v1/stage/build')
    tools = {}
    for name in ('cket', 'cket_backend', 'cket_runtime.o', 'cket_plugin.so', 'libcket_quantum.a', 'gcc-version.txt'):
        tools[name] = digest(source / name)
        shutil.copyfile(source / name, stage / 'build' / name)
        (stage / 'build' / name).chmod(0o755)
    if configuration != 'normal':
        shutil.copyfile(source / 'gcc-wrapper', stage / 'build/gcc-wrapper')
        (stage / 'build/gcc-wrapper').chmod(0o755)
    flags = ['gcc-11', '-std=c11', '-Wall', '-Wextra', '-Werror', '-Isrc/engine', '-Itests']
    flags += ['-O2'] if configuration == 'normal' else ['-O1', '-g', '-fsanitize=address,undefined',
        '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
    argv = flags + ['tests/validate_pointer_differences.c', 'tests/validation.c',
        str(stage / 'build/libcket_quantum.a'), '-lm', '-o', str(stage / 'build/validate_pointer_differences')]
    with (OUT / f'{configuration}-build.log').open('x') as log:
        result = subprocess.run(argv, cwd=CANDIDATE, stdout=log, stderr=subprocess.STDOUT)
    assert result.returncode == 0, (configuration, result.returncode)
    env = dict(os.environ, CC='gcc-11' if configuration == 'normal' else str(stage / 'build/gcc-wrapper'),
               CKET_REAL_GCC='gcc-11', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    log_path = OUT / (configuration + '.log')
    with log_path.open('x') as log:
        result = subprocess.run([str(stage / 'build/validate_pointer_differences')], cwd=CANDIDATE,
                                env=env, stdout=log, stderr=subprocess.STDOUT)
    assert result.returncode == 0, (configuration, result.returncode)
    text = log_path.read_text()
    expected = '170 pointer difference checks passed; native distances, finite coherent action and unsupported contexts have separate scopes.\n'
    assert text.endswith(expected) and 'PASS 90 native pointer distances and 40 rejection contracts; 45 ordinary C executions separate;' in text
    assert 'PASS 40 pointer-distance action/inverse circuits:' in text
    assert not any(marker in text for marker in ('AddressSanitizer:', 'runtime error:', 'internal compiler error:', 'FAIL '))
    row = {'status': 'passed-scoped-maintained', 'returncode': 0, 'configuration': configuration,
           'checks': 170, 'native_checks': 90, 'diagnostic_checks': 40, 'action_and_inverse_circuits': 40,
           'state_comparisons_inside_circuits': 80, 'ordinary_c_executions_separate': 45,
           'reused_tools_sha256': tools, 'fresh_validator_sha256': digest(stage / 'build/validate_pointer_differences'),
           'log_sha256': digest(log_path)}
    (OUT / (configuration + '-results.json')).write_text(json.dumps(row, indent=2) + '\n')
    print('PASS recurrent pointer differences:', configuration, flush=True)
    return row


try:
    frozen = json.loads((CANDIDATE / 'build/maintained-sources.json').read_text())
    for name, expected in frozen.items():
        assert digest(CANDIDATE / name) == expected, name
    assert len(frozen) == 137
    with ThreadPoolExecutor(max_workers=2) as pool:
        results = list(pool.map(run, ('normal', 'sanitized')))
    for name, expected in frozen.items():
        assert digest(CANDIDATE / name) == expected, name
    terminal = {'status': 'passed-scoped-maintained', 'exit_code': 0, 'configurations': results,
        'source_files': 137, 'source_sha256': frozen,
        'scope': 'Newly compiled recurrent C validator on 137 frozen inputs with the scoped v3 normal/sanitizer compiler tools reused. Ninety native checks, forty specific diagnostics, forty complete action/inverse circuits and forty-five separate ordinary C references per configuration. The former CPU-scalar coherent-control rejection is now positive and a second phased fixture binds its explicit source-level distance-one precondition. This finite proof is separate from a full broad campaign and does not certify general memory lifetimes or all C11. No main application or cybersecurity change.'}
except BaseException as error:
    traceback.print_exc()
    terminal = {'status': 'failed-maintained-checks', 'exit_code': 1, 'error': repr(error)}
(OUT / 'exit.json').write_text(json.dumps(terminal, indent=2) + '\n')
print(json.dumps({key: value for key, value in terminal.items() if key not in ('source_sha256', 'configurations')}), flush=True)
raise SystemExit(terminal['exit_code'])
