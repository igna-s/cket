"""Build a separate integration copy and close only the relevant main checks.

The independent 137-input broad campaign continues unchanged. This targeted
integration proof must not be described as that campaign's completion.
"""
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

ROOT = Path(__file__).resolve().parent.parent
CANDIDATE = ROOT / 'build/coherent-cpu-maintained'
OUT = ROOT / 'build/coherent-integration-v1'
PROJECT = OUT / 'project'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def utc():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


if '--worker' not in sys.argv:
    OUT.mkdir()
    shutil.copyfile(Path(__file__), OUT / 'runner.py')
    with (OUT / 'worker.log').open('x') as log:
        process = subprocess.Popen(
            [sys.executable, str(Path(__file__).resolve()), '--worker'],
            cwd=ROOT, stdin=subprocess.DEVNULL, stdout=log,
            stderr=subprocess.STDOUT, start_new_session=True)
    fields = (Path('/proc') / str(process.pid) / 'stat').read_text().rsplit(')', 1)[1].split()
    write_json(OUT / 'job.json', {
        'pid': process.pid, 'linux_start_ticks': int(fields[19]),
        'boot_id': Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
        'started_utc': utc(), 'runner_sha256': digest(Path(__file__))})
    print('Started separate targeted integration:', process.pid)
    raise SystemExit(0)


record = {'status': 'running', 'started_utc': utc(), 'commands': [],
          'broad_campaign_status': 'pending-separate-frozen-campaign',
          'scope': 'Fresh normal and ASan/UBSan core builds in a separate source-identical '
                   'integration copy. Native, memory-return, pointer-distance and coherent '
                   'CPU-condition regressions have explicit finite counts. This is not a '
                   'completed broad campaign, physical-hardware proof or complete C11 claim. '
                   'Main application is a separate guarded step. Cybersecurity is unchanged.'}
write_json(OUT / 'progress.json', record)
env = dict(os.environ, CC='gcc-11', CXX='g++-11', CKET_REAL_GCC='gcc-11',
           UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')


def step(name, argv, extra=None, expected=None):
    log = OUT / (f'{len(record["commands"]) + 1:02}-{name}.log')
    started = utc()
    with log.open('x') as handle:
        result = subprocess.run(argv, cwd=PROJECT, env=dict(env, **(extra or {})),
                                stdin=subprocess.DEVNULL, stdout=handle,
                                stderr=subprocess.STDOUT)
    row = {'name': name, 'command': argv, 'returncode': result.returncode,
           'started_utc': started, 'finished_utc': utc(),
           'log': log.name, 'log_sha256': digest(log)}
    record['commands'].append(row)
    write_json(OUT / 'progress.json', record)
    assert result.returncode == 0, row
    text = log.read_text()
    assert '\0' not in text
    if expected:
        assert text.count(expected) == 1, (name, text[-2000:])
    print('PASS integration:', name, flush=True)


try:
    maintained = json.loads((ROOT / 'docs/audit-evidence/coherent-cpu-conditions-maintained/maintained-summary.json').read_text())
    frozen = maintained['source_sha256']
    assert maintained['status'] == 'verified-scoped-maintained' and len(frozen) == 137
    terminal = json.loads((CANDIDATE / 'build/maintained-checks-v1/exit.json').read_text())
    assert terminal['exit_code'] == 0 and all(row['checks'] == 170 for row in terminal['configurations'])
    PROJECT.mkdir()
    for name, expected in frozen.items():
        assert digest(CANDIDATE / name) == expected, name
        target = PROJECT / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(CANDIDATE / name, target)
    shutil.copyfile(CANDIDATE / 'LICENSE', PROJECT / 'LICENSE')
    (PROJECT / '.deps').mkdir()
    (PROJECT / '.deps/gcc').symlink_to(ROOT / '.deps/gcc', target_is_directory=True)
    spec = importlib.util.spec_from_file_location('integration_sources', PROJECT / 'scripts/archive-audit.py')
    source_module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(source_module)
    assert source_module.source_hashes() == frozen
    native_owner = ROOT / 'build/coherent-cpu-branch-v3/build/native-v2'
    matrix = json.loads((native_owner / 'results.json').read_text())
    assert matrix['native_executions'] == 180 and matrix['diagnostic_checks'] == 30
    record.update(source_files=137, source_sha256=frozen,
                  original_native_matrix_sha256=digest(native_owner / 'results.json'))
    write_json(OUT / 'progress.json', record)
    configurations = []
    for configuration in ('normal', 'sanitized'):
        if configuration == 'normal':
            stage, options = PROJECT, {'CC': 'gcc-11'}
            step('fresh-normal-build', ['bash', 'scripts/build.sh'])
        else:
            stage = PROJECT / 'build/sanitized'
            script = (PROJECT / 'scripts/sanitize-all.sh').read_text()
            delimiter = '"$san_root/build/cket_tests" "$san_root"\n'
            assert script.count(delimiter) == 1
            prefix = script[:script.index(delimiter)]
            marker = 'cd "$(dirname "$0")/.."'
            assert prefix.count(marker) == 1
            prefix = prefix.replace(marker, 'cd "$CKET_AUDIT_INTEGRATION"')
            wrapper = OUT / 'fresh-sanitized-build-only.sh'
            wrapper.write_text(prefix)
            step('fresh-sanitized-build', ['bash', str(wrapper)],
                 {'CKET_AUDIT_INTEGRATION': str(PROJECT)})
            options = {'CC': str(stage / 'build/gcc-wrapper')}
        row = {'configuration': configuration,
               'fresh_core_sha256': {name: digest(stage / 'build' / name) for name in
                                    ('cket', 'cket_backend', 'cket_plugin.so', 'cket_runtime.o', 'libcket_quantum.a')}}
        for name, binary, expected in (
                ('native', 'cket_tests', '393 passed, 0 failed (native C tests, no external quantum SDK)'),
                ('memory-returns', 'validate_memory_returns', '290 memory return checks passed;'),
                ('pointer-differences', 'validate_pointer_differences', '170 pointer difference checks passed;')):
            argv = [str(stage / 'build' / binary)] + ([str(stage)] if name == 'native' else [])
            step(configuration + '-' + name, argv, options, expected)
        rows = []
        destination = OUT / (configuration + '-cpu-matrix')
        destination.mkdir()
        fixture_hashes = {}
        for item in matrix['rows']:
            source = native_owner / (item['name'] + '.c')
            fixture_hashes[source.name] = digest(source)
            diagnostic = item.get('diagnostic')
            binary = destination / (item['name'] + '-' + item['optimization'][1:] + '-' + item['builtins'][2:])
            argv = [str(stage / 'build/cket'), str(source), item['optimization'], item['builtins'], '-o', str(binary)]
            if diagnostic:
                argv.insert(2, '--emit-ir')
            result = subprocess.run(argv, cwd=PROJECT, env=dict(env, **options), capture_output=True, text=True)
            result_row = {'name': item['name'], 'optimization': item['optimization'],
                          'builtins': item['builtins'], 'compile_returncode': result.returncode,
                          'stderr': result.stderr, 'runs': []}
            rows.append(result_row)
            assert not any(marker in result.stderr for marker in
                           ('AddressSanitizer:', 'runtime error:', 'internal compiler error:')), result_row
            if diagnostic:
                assert result.returncode == 1 and diagnostic in result.stderr, result_row
            else:
                assert result.returncode == 0, result_row
                for before in item['runs']:
                    result = subprocess.run([str(binary), before['input']], env=dict(env, **options),
                                            capture_output=True, text=True)
                    result_row['runs'].append({'input': before['input'], 'returncode': result.returncode,
                                               'stderr': result.stderr})
                    assert result.returncode == 0, result_row
                    assert not any(marker in result.stderr for marker in
                                   ('AddressSanitizer:', 'runtime error:')), result_row
            write_json(destination / 'progress.json', rows)
        executions = sum(len(item['runs']) for item in rows)
        assert executions == 180 and sum(bool(item.get('diagnostic')) for item in matrix['rows']) == 30
        write_json(destination / 'results.json', {'status': 'passed', 'native_executions': 180,
                   'diagnostic_checks': 30, 'fixture_sha256': fixture_hashes, 'rows': rows})
        row.update(native_checks=393, memory_return_checks=290, pointer_difference_checks=170,
                   cpu_condition_native_executions=180, cpu_effect_diagnostics=30,
                   cpu_matrix_results_sha256=digest(destination / 'results.json'))
        configurations.append(row)
        record['configurations'] = configurations
        write_json(OUT / 'progress.json', record)
        assert source_module.source_hashes() == frozen
        print('PASS integration coherent matrix:', configuration, '180 native / 30 diagnostics', flush=True)
    for name, expected in frozen.items():
        assert digest(CANDIDATE / name) == expected, name
    record.update(status='passed-targeted-integration', exit_code=0, finished_utc=utc())
except BaseException as error:
    traceback.print_exc()
    (OUT / 'failure.txt').write_text(traceback.format_exc())
    record.update(status='failed-targeted-integration', exit_code=1, error=repr(error), finished_utc=utc())
write_json(OUT / 'progress.json', record)
write_json(OUT / 'exit.json', record)
raise SystemExit(record['exit_code'])
