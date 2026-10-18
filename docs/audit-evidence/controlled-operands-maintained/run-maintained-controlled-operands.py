"""Compile the recurrent C validator; reuse the scoped source-identical tools."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import sys
import traceback

ROOT = Path(__file__).resolve().parent.parent
CANDIDATE = ROOT / 'build/coherent-body-maintained'
BASE = ROOT / 'build/coherent-body-prototype-v4'
OUT = CANDIDATE / 'build/maintained-checks-v1'
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


if '--worker' not in sys.argv:
    OUT.mkdir()
    shutil.copyfile(Path(__file__), OUT / 'runner.py')
    with (OUT / 'worker.log').open('x') as handle:
        process = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), '--worker'],
            cwd=CANDIDATE, stdin=subprocess.DEVNULL, stdout=handle, stderr=subprocess.STDOUT,
            start_new_session=True)
    fields = (Path('/proc') / str(process.pid) / 'stat').read_text().rsplit(')', 1)[1].split()
    write_json(OUT / 'job.json', {'pid': process.pid, 'linux_start_ticks': int(fields[19]),
        'boot_id': Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
        'runner_sha256': digest(Path(__file__))})
    print('Started 138-input recurrent controlled operands:', process.pid)
    raise SystemExit(0)


record = {'status': 'running', 'configurations': [],
    'scope': 'Newly compiled recurrent C validator on 138 frozen inputs. Source-identical v4 '
             'normal and sanitizer plugin/driver/engine tools are reused with hashes; this is '
             'not a fresh whole compiler build or broad campaign. Per configuration: 1000 '
             'defined native values, 200 explicit undefined-division diagnostics, 60 CPU-effect '
             'rejection contracts and 10 internal-homonym native checks. One hundred ordinary '
             'C executions are separate. Independent 8000 state/inverse checks belong to the '
             '137-input v4 compiler prototype, whose frontend sources match this candidate. '
             'Main application, broader regression, phase-angle and invalid CPU-read fixes '
             'and complete C11 remain pending. Cybersecurity unchanged.'}
write_json(OUT / 'progress.json', record)
try:
    frozen = json.loads((CANDIDATE / 'build/maintained-sources.json').read_text())
    before = json.loads((BASE / 'build/sources.json').read_text())
    assert len(frozen) == 138 and len(before) == 137
    assert all(frozen[name] == before[name] for name in ('src/quantum_frontend.inc', 'src/quantum_semantics.inc'))
    for name, expected in frozen.items():
        assert digest(CANDIDATE / name) == expected, name
    matrix = json.loads((BASE / 'build/native-matrix-v2/exit.json').read_text())
    assert matrix['exit_code'] == 0
    for directory in ('states-v1', 'states-sanitized-v1'):
        proof = json.loads((BASE / 'build' / directory / 'exit.json').read_text())
        assert proof['exit_code'] == 0 and proof['complete_state_checks'] == 4000
    record.update(source_files=138, source_sha256=frozen)
    for configuration in ('normal', 'sanitized'):
        stage = CANDIDATE if configuration == 'normal' else CANDIDATE / 'build/sanitized'
        owner = BASE if configuration == 'normal' else BASE / 'build/native-matrix-v2/sanitized-stage'
        (stage / 'build').mkdir(parents=True, exist_ok=True)
        if configuration == 'sanitized':
            for name in ('src', 'include', 'tests', 'examples'):
                (stage / name).symlink_to(CANDIDATE / name, target_is_directory=True)
            shutil.copyfile(CANDIDATE / 'cket', stage / 'cket')
        tools = ('cket', 'cket_backend', 'cket_plugin.so', 'cket_runtime.o', 'libcket_quantum.a', 'gcc-version.txt')
        if configuration == 'sanitized':
            tools += ('gcc-wrapper',)
        provenance = {}
        for name in tools:
            source = owner / 'build' / name
            provenance[name] = digest(source)
            shutil.copyfile(source, stage / 'build' / name)
            (stage / 'build' / name).chmod(0o755)
        flags = ['-O2'] if configuration == 'normal' else ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
        argv = ['gcc-11', '-std=c11', '-Wall', '-Wextra', '-Werror', '-Itests', *flags,
            'tests/validate_controlled_operands.c', 'tests/validation.c', '-o', str(stage / 'build/validate_controlled_operands')]
        log = OUT / (configuration + '-build.log')
        with log.open('x') as handle:
            result = subprocess.run(argv, cwd=CANDIDATE, stdout=handle, stderr=subprocess.STDOUT)
        assert result.returncode == 0, log.read_text()
        env = dict(os.environ, CC='gcc-11' if configuration == 'normal' else str(stage / 'build/gcc-wrapper'),
                   CKET_REAL_GCC='gcc-11', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        log = OUT / (configuration + '.log')
        with log.open('x') as handle:
            result = subprocess.run([str(stage / 'build/validate_controlled_operands')], cwd=CANDIDATE,
                env=env, stdout=handle, stderr=subprocess.STDOUT)
        assert result.returncode == 0, log.read_text()[-5000:]
        assert log.read_text().count('1270 controlled operand checks passed;') == 1
        row = {'configuration': configuration, 'returncode': result.returncode, 'checks': 1270,
               'defined_native_checks': 1000, 'undefined_domain_diagnostics': 200,
               'effect_diagnostics': 60, 'native_homonyms': 10, 'ordinary_c_executions_separate': 100,
               'reused_tools_sha256': provenance, 'fresh_validator_sha256': digest(stage / 'build/validate_controlled_operands'),
               'log_sha256': digest(log)}
        record['configurations'].append(row)
        write_json(OUT / 'progress.json', record)
        print('PASS recurrent controlled operands:', configuration, 1270, flush=True)
    for name, expected in frozen.items():
        assert digest(CANDIDATE / name) == expected, name
    record.update(status='passed-scoped-maintained', exit_code=0)
except BaseException as error:
    traceback.print_exc()
    (OUT / 'failure.txt').write_text(traceback.format_exc())
    record.update(status='failed-maintained-checks', exit_code=1, error=repr(error))
write_json(OUT / 'exit.json', record)
write_json(OUT / 'progress.json', record)
raise SystemExit(record['exit_code'])
