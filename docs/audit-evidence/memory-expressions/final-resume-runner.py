"""Finish only unclosed suites; preserve every interrupted segment verbatim."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys
import traceback

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / 'build'
spec = importlib.util.spec_from_file_location('audit_original', BUILD / 'run-isolated-memory-audit.py')
original = importlib.util.module_from_spec(spec)
spec.loader.exec_module(original)
digest, utc, write_json = original.digest, original.utc, original.write_json
namespace = {'__file__': str(BUILD / 'resume-linux-memory-audit-v2.py')}
text = (BUILD / 'resume-linux-memory-audit-v2.py').read_text()
exec(compile(text[:text.index("\n\nif '--worker'")], namespace['__file__'], 'exec'), namespace)
JOB = BUILD / 'linux-resume-v3-job.json'
TERMINAL = BUILD / 'linux-resume-v3-exit.json'


def run(label):
    candidate = BUILD / ('memory-sequence-prototype' if label == 'sequence' else 'memory-return-maintained')
    build = candidate / 'build'
    old = build / 'linux-isolated-resume-v2'
    before = json.loads((old / 'progress.json').read_text())
    assert before['status'] == 'running'
    assert len(before['commands']) == (4 if label == 'sequence' else 9)
    manifest = build / ('sequence-full-sources-before.json' if label == 'sequence' else 'return-full-v3-sources-before.json')
    original.source_check(candidate, manifest)
    assert before['source_manifest_sha256'] == digest(manifest)
    prefix = bytearray((old / 'completed-prefix.log').read_bytes())
    assert digest(old / 'completed-prefix.log') == before['completed_prefix_sha256']
    for row in before['commands']:
        assert row['returncode'] == 0 and digest(old / row['log']) == row['log_sha256'], row
        if row['name'] == 'frontend-sanitized-remaining':
            prefix.extend((old / 'fuzz-completed-prefix.log').read_bytes())
        prefix.extend((old / row['log']).read_bytes())
        if row['name'] == 'frontend-sanitized-remaining':
            coverage = json.loads((old / 'fuzz-coverage.json').read_text())
            assert coverage['case_indices'] == list(range(133)) and coverage['compilations'] == 665
            assert coverage['seed'] == 20261012 and coverage['optimization_levels'] == 5
            assert coverage['byte_inputs_per_compilation'] == 256
            assert coverage['original_source_sha256'] == digest(candidate / 'tests/fuzz_frontend.c')
            assert coverage['derived_driver_sha256'] == digest(old / 'resumable-fuzz-driver.c')
            assert coverage['derived_binary_sha256'] == digest(build / 'sanitized/build/fuzz_frontend-resumable-v2')
            indices = [int(x) for x in re.findall(rb'^PASS frontend (\d+): all 256 byte inputs at all 5 optimization levels$',
                       (old / 'fuzz-completed-prefix.log').read_bytes() + (old / row['log']).read_bytes(), re.M)]
            assert indices == list(range(133))
            prefix.extend(b'SEGMENTED FRONTEND COVERAGE: all 133 original indices, five levels and 256 inputs verified.\n')
            prefix.extend(b'665 generated frontend compilations passed; seed=20261012 (C generator, SplitMix64)\n')
    assert b'\0' not in prefix
    output = build / 'linux-isolated-resume-v3'
    output.mkdir()
    (output / 'completed-prefix.log').write_bytes(prefix)
    record = {'status': 'running', 'label': label, 'started_utc': utc(),
              'source_manifest_sha256': digest(manifest), 'source_inputs': before['source_inputs'],
              'prior_exit_codes': None, 'prior_closed_commands': len(before['commands']),
              'completed_prefix_sha256': digest(output / 'completed-prefix.log'), 'commands': [],
              'scope': before['scope'] + ' Another confirmed WSL restart interrupted the final QASM suite (134 inputs) and the memset suite (136 inputs). This segment retains all previously closed steps and reruns only unfinished suites. Earlier invocations have no terminal status; their raw logs remain unchanged.'}
    write_json(output / 'progress.json', record)
    combined = bytearray(prefix)
    env = dict(os.environ, CC='gcc-11', CXX='g++-11', CKET_REAL_GCC='gcc-11',
               UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    san = build / 'sanitized'
    wrapper = str(san / 'build/gcc-wrapper')

    def step(name, argv, extra=None, expected=None):
        log = output / f'{len(record["commands"]) + 1:02}-{name}.log'
        started = utc()
        print(started, label, name, flush=True)
        with log.open('xb') as handle:
            result = subprocess.run(argv, cwd=candidate, env=dict(env, **(extra or {})),
                                    stdin=subprocess.DEVNULL, stdout=handle, stderr=subprocess.STDOUT)
        row = {'name': name, 'command': argv, 'returncode': result.returncode, 'started_utc': started,
               'finished_utc': utc(), 'log': log.name, 'log_sha256': digest(log)}
        record['commands'].append(row)
        write_json(output / 'progress.json', record)
        assert result.returncode == 0, row
        data = log.read_bytes()
        assert b'\0' not in data
        if expected:
            assert len(re.findall(expected, data.decode(), re.M)) == 1, name
        combined.extend(data)
        return log

    try:
        if label == 'returns':
            for name, expected in before['fresh_instrumented_core_sha256'].items():
                assert digest(san / 'build' / name) == expected, name
            record['reused_fresh_instrumented_core_sha256'] = before['fresh_instrumented_core_sha256']
            write_json(output / 'progress.json', record)
            for name in ('memset', 'memory_sequences', 'memory_returns', 'byte_snapshots', 'division',
                         'bindings', 'history', 'ctype', 'profiles', 'numeric_locale', 'library', 'examples'):
                binary = 'test_examples' if name == 'examples' else 'validate_' + name
                options = {'CC': wrapper}
                if name in ('ctype', 'numeric_locale'):
                    options['LSAN_OPTIONS'] = f'suppressions={candidate}/tests/glibc-locale.lsan:print_suppressions=0'
                step(name + '-sanitized', [str(san / 'build' / binary)], options, original.patterns(candidate)[name])
            step('classical-sanitized', ['bash', 'scripts/test-c11-classical.sh'],
                 {'CC': wrapper, 'CKET_DRIVER': str(san / 'build/cket')})
            step('validation-sanitized-build', ['bash', 'scripts/build-validation.sh'], {'VALIDATION_SANITIZERS': '1'})
            for name in ('fuzz_frontend', 'benchmark_synthesis', 'validate_qasm', 'qasm_samples'):
                shutil.copyfile(build / f'{name}-sanitized', san / 'build' / f'{name}-sanitized')
                (san / 'build' / f'{name}-sanitized').chmod(0o755)
            driver = namespace['prepare_fuzz_driver'](candidate, output)
            binary = san / 'build/fuzz_frontend-resumable-v3'
            step('build-resumable-fuzzer', ['gcc-11', '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                 '-Itests', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie',
                 str(driver), 'tests/validation.c', '-o', str(binary)])
            log = step('frontend-sanitized-remaining', [str(binary), '--seed', '20261012', '--cases', '128',
                        '--first', '0', '--limit', '133'], {'CC': wrapper})
            indices = [int(x) for x in re.findall(r'^PASS frontend (\d+): all 256 byte inputs at all 5 optimization levels$', log.read_text(), re.M)]
            assert indices == list(range(133))
            combined.extend(b'SEGMENTED DRIVER COVERAGE: original 133 case indices, five levels and 256 byte inputs.\n')
            combined.extend(b'665 generated frontend compilations passed; seed=20261012 (C generator, SplitMix64)\n')
            write_json(output / 'fuzz-coverage.json', {'status': 'passed-derived-driver', 'seed': 20261012,
                'case_indices': indices, 'compilations': 665, 'optimization_levels': 5, 'byte_inputs_per_compilation': 256,
                'original_source_sha256': digest(candidate / 'tests/fuzz_frontend.c'),
                'derived_driver_sha256': digest(driver), 'derived_binary_sha256': digest(binary), 'scope': record['scope']})
            step('growth-sanitized', [str(san / 'build/benchmark_synthesis-sanitized')], {'CC': wrapper})
            step('growth-comparison', ['cmp', str(build / 'synthesis-growth.csv'), str(san / 'build/synthesis-growth.csv')])
        step('qasm-sanitized', [str(san / 'build/validate_qasm-sanitized')],
             {'CC': wrapper, 'LSAN_OPTIONS': f'suppressions={candidate}/tests/glibc-locale.lsan:print_suppressions=0'})
        combined.extend((original.FINAL + '\n').encode())
        original.check_transcript(candidate, combined.decode())
        original.source_check(candidate, manifest)
        (output / 'combined-verify.log').write_bytes(combined)
        record.update(status='passed-in-segments', exit_code=0, finished_utc=utc(),
                      combined_log_sha256=digest(output / 'combined-verify.log'))
    except BaseException as error:
        (output / 'failure.txt').write_text(traceback.format_exc())
        record.update(status='failed-resume', exit_code=1, error=repr(error), finished_utc=utc())
        traceback.print_exc()
    write_json(output / 'progress.json', record)
    return record['exit_code']


if __name__ == '__main__':
    if '--worker' in sys.argv:
        with ThreadPoolExecutor(max_workers=2) as pool:
            futures = [pool.submit(run, label) for label in ('sequence', 'returns')]
            codes = [future.result() for future in futures]
        write_json(TERMINAL, {'exit_code': max(codes), 'campaign_exit_codes': codes, 'finished_utc': utc()})
        raise SystemExit(max(codes))
    assert sys.argv[1:] == [] and not JOB.exists() and not TERMINAL.exists()
    with (BUILD / 'linux-resume-v3.log').open('x') as log:
        process = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), '--worker'], cwd=ROOT,
            stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
    fields = (Path('/proc') / str(process.pid) / 'stat').read_text().rsplit(')', 1)[1].split()
    write_json(JOB, {'pid': process.pid, 'linux_start_ticks': int(fields[19]), 'started_utc': utc(),
        'boot_id': Path('/proc/sys/kernel/random/boot_id').read_text().strip(), 'runner_sha256': digest(Path(__file__))})
    print(JOB.read_text())
