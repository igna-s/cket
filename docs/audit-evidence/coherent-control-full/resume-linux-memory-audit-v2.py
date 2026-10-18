"""Resume interrupted proofs without repeating completed suites or fuzz cases.

The temporary fuzz driver retains the exact original expression generator,
generated program, oracle and five compiler invocations. Its only additions are
case-range selection and durable progress. Original repository inputs remain
unchanged; the derived harness and its scope are archived separately.
"""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import hashlib
import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys
import time
import traceback

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / 'build'
spec = importlib.util.spec_from_file_location('original_linux_runner', BUILD / 'run-isolated-memory-audit.py')
original = importlib.util.module_from_spec(spec)
spec.loader.exec_module(original)
digest, utc, write_json = original.digest, original.utc, original.write_json
JOB = BUILD / 'linux-resume-v2-job.json'
TERMINAL = BUILD / 'linux-resume-v2-exit.json'


def prepare_fuzz_driver(candidate, output):
    source = (candidate / 'tests/fuzz_frontend.c').read_text()
    derived = source.replace('#include "validation.h"\n', '#include "validation.h"\n#include <unistd.h>\n', 1)
    edits = [
        ('uint64_t seed = 20261011, cases = 24;',
         'uint64_t seed = 20261011, cases = 24, first = 0, limit = UINT64_MAX;'),
        ('        else\n            validation_fail("unknown option: %s", argv[i]);',
         '        else if (!strcmp(argv[i], "--first"))\n            first = number(argv[++i]);\n'
         '        else if (!strcmp(argv[i], "--limit"))\n            limit = number(argv[++i]);\n'
         '        else\n            validation_fail("unknown option: %s", argv[i]);'),
        ('    validation v;\n', '    if (limit == UINT64_MAX) limit = cases + 5;\n'
         '    if (first > limit || limit > cases + 5) validation_fail("invalid segment");\n'
         '    validation v;\n'),
        ('for (uint64_t i = 0; i < cases + 5; ++i)', 'for (uint64_t i = 0; i < limit; ++i)'),
        ('        program(source, expr);', '        if (i < first) continue;\n        program(source, expr);'),
        ('        fflush(stdout);', '        fflush(stdout);\n        if (fsync(STDOUT_FILENO)) validation_fail("cannot sync fuzz progress");'),
        (' generated frontend compilations passed; seed=', ' segment frontend compilations passed; seed='),
        ('           (cases + 5) * 5,', '           (limit - first) * 5,'),
    ]
    for before, after in edits:
        assert derived.count(before) == 1, before
        derived = derived.replace(before, after)
    # The complete generator, source-emitter and scalar oracle are untouched.
    assert source[:source.index('int main(')].replace('#include "validation.h"\n', '') == \
        derived[:derived.index('int main(')].replace('#include "validation.h"\n#include <unistd.h>\n', '')
    path = output / 'resumable-fuzz-driver.c'
    path.write_text(derived)
    return path


def completed_fuzz_prefix(raw):
    data = raw.read_bytes()
    first_zero = data.find(b'\0')
    valid = data if first_zero < 0 else data[:first_zero]
    if first_zero >= 0:
        assert not data[first_zero:].strip(b'\0'), 'Corruption is not confined to the trailing zero-filled extent.'
    valid = valid[:valid.rfind(b'\n') + 1]
    text = valid.decode()
    assert not re.search(r'^FAIL|runtime error:|ERROR: .*Sanitizer|validation .*fail', text, re.M)
    indices = [int(value) for value in re.findall(r'^PASS frontend (\d+): all 256 byte inputs at all 5 optimization levels$', text, re.M)]
    assert indices == list(range(len(indices))) and len(indices) <= 133
    return valid, len(indices), len(data) - len(valid)


def run(label):
    candidate = BUILD / ('memory-sequence-prototype' if label == 'sequence' else 'memory-return-maintained')
    build = candidate / 'build'
    old = build / 'linux-isolated-resume-v1'
    old_record = json.loads((old / 'progress.json').read_text())
    assert old_record['status'] == 'running'
    expected_closed = 8 if label == 'sequence' else 17
    assert len(old_record['commands']) == expected_closed, old_record['commands']
    for row in old_record['commands']:
        assert row['returncode'] == 0 and digest(old / row['log']) == row['log_sha256'], row
    manifest = build / ('sequence-full-sources-before.json' if label == 'sequence' else 'return-full-v3-sources-before.json')
    original.source_check(candidate, manifest)
    output = build / 'linux-isolated-resume-v2'
    output.mkdir()
    prefix = (old / 'original-prefix.log').read_bytes() + b''.join((old / row['log']).read_bytes() for row in old_record['commands'])
    assert b'\0' not in prefix
    (output / 'completed-prefix.log').write_bytes(prefix)
    record = {'status': 'running', 'label': label, 'started_utc': utc(),
              'source_inputs': old_record['source_inputs'], 'source_manifest_sha256': digest(manifest),
              'original_and_prior_resume_exit_codes': None, 'prior_completed_commands': expected_closed,
              'completed_prefix_sha256': digest(output / 'completed-prefix.log'), 'commands': [],
              'scope': ('Resumption after a confirmed additional WSL restart. Every closed earlier suite/log '
                        'is retained and hash-checked; no exit status is invented for stopped invocations. '
                        'Repository source inputs remain frozen. Fuzz resumption uses an archived temporary '
                        'driver with unchanged generator/program/oracle/compiler-loop bodies, original seed '
                        'and all five GCC levels/all 256 byte inputs. It skips only durable, already completed '
                        'case indices. Aggregate fuzz counts are coverage-checked segmented results, not an '
                        'uninterrupted invocation of the original fuzzer. No security configuration changes.')}
    write_json(output / 'progress.json', record)
    env = dict(os.environ, CC='gcc-11', CXX='g++-11', CKET_REAL_GCC='gcc-11',
               CKET_FUZZ_CASES='128', CKET_FUZZ_SEED='20261012',
               UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    san = build / 'sanitized'
    wrapper = str(san / 'build/gcc-wrapper')
    combined = bytearray(prefix)

    def step(name, command, extra=None, expected=None):
        log = output / f'{len(record["commands"]) + 1:02}-{name}.log'
        started = utc()
        print(started, label, name, flush=True)
        with log.open('xb') as handle:
            result = subprocess.run(command, cwd=candidate, env=dict(env, **(extra or {})),
                                    stdin=subprocess.DEVNULL, stdout=handle, stderr=subprocess.STDOUT)
        row = {'name': name, 'command': command, 'returncode': result.returncode,
               'started_utc': started, 'finished_utc': utc(), 'log': log.name, 'log_sha256': digest(log)}
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
            script = (candidate / 'scripts/sanitize-all.sh').read_text()
            delimiter = '"$san_root/build/cket_tests" "$san_root"\n'
            assert script.count(delimiter) == 1
            prefix_script = script[:script.index(delimiter)]
            assert prefix_script.count('cd "$(dirname "$0")/.."') == 1
            prefix_script = prefix_script.replace('cd "$(dirname "$0")/.."', 'cd "$CKET_AUDIT_CANDIDATE"')
            build_script = output / 'fresh-sanitizer-build-only.sh'
            build_script.write_text(prefix_script)
            step('fresh-sanitizer-build', ['bash', str(build_script)], {'CKET_AUDIT_CANDIDATE': str(candidate)})
            record['fresh_instrumented_core_sha256'] = {
                name: digest(san / 'build' / name) for name in
                ('cket', 'cket_backend', 'cket_plugin.so', 'cket_runtime.o', 'libcket_quantum.a')}
            write_json(output / 'progress.json', record)
            suite_names = ['native', 'harness', 'shifts', 'expressions', 'inplace', 'memcopy',
                           'memcmp', 'strncmp', 'memset', 'memory_sequences', 'memory_returns',
                           'byte_snapshots', 'division', 'bindings', 'history', 'ctype',
                           'profiles', 'numeric_locale', 'library', 'examples']
            for name in suite_names:
                binary = 'cket_tests' if name == 'native' else 'test_examples' if name == 'examples' else 'validate_' + name
                command = [str(san / 'build' / binary)] + ([str(san)] if name == 'native' else [])
                options = {'CC': wrapper}
                if name in ('ctype', 'numeric_locale'):
                    options['LSAN_OPTIONS'] = f'suppressions={candidate}/tests/glibc-locale.lsan:print_suppressions=0'
                step(name + '-sanitized', command, options, original.patterns(candidate)[name])
            step('classical-sanitized', ['bash', 'scripts/test-c11-classical.sh'],
                 {'CC': wrapper, 'CKET_DRIVER': str(san / 'build/cket')})
            step('validation-sanitized-build', ['bash', 'scripts/build-validation.sh'], {'VALIDATION_SANITIZERS': '1'})
            for name in ('fuzz_frontend', 'benchmark_synthesis', 'validate_qasm', 'qasm_samples'):
                shutil.copyfile(build / f'{name}-sanitized', san / 'build' / f'{name}-sanitized')
                (san / 'build' / f'{name}-sanitized').chmod(0o755)
            fuzz_prefix, first, discarded = b'', 0, 0
        else:
            fuzz_prefix, first, discarded = completed_fuzz_prefix(old / '09-frontend-sanitized.log')
        (output / 'fuzz-completed-prefix.log').write_bytes(fuzz_prefix)
        record['fuzz_prior_cases'] = first
        record['unconfirmed_trailing_fuzz_bytes'] = discarded
        write_json(output / 'progress.json', record)
        driver = prepare_fuzz_driver(candidate, output)
        binary = san / 'build/fuzz_frontend-resumable-v2'
        step('build-resumable-fuzzer', ['gcc-11', '-std=c11', '-O1', '-g', '-Wall', '-Wextra',
             '-Werror', '-Itests', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
             '-fno-pie', '-no-pie', str(driver), 'tests/validation.c', '-o', str(binary)])
        combined.extend(fuzz_prefix)
        log = step('frontend-sanitized-remaining', [str(binary), '--seed', '20261012', '--cases', '128',
                                                  '--first', str(first), '--limit', '133'], {'CC': wrapper})
        text = (fuzz_prefix + log.read_bytes()).decode()
        indices = [int(value) for value in re.findall(r'^PASS frontend (\d+): all 256 byte inputs at all 5 optimization levels$', text, re.M)]
        assert indices == list(range(133)), indices
        aggregate = '665 generated frontend compilations passed; seed=20261012 (C generator, SplitMix64)\n'
        # step() appended only the successful resumed interval; add its durable
        # old prefix and verified aggregate once. The original partial raw log
        # including any zero-filled tail remains preserved in the v1 capsule.
        combined.extend(b'SEGMENTED FRONTEND COVERAGE: all 133 original case indices, five levels and 256 byte inputs verified.\n')
        combined.extend(aggregate.encode())
        write_json(output / 'fuzz-coverage.json', {'status': 'passed-in-segments', 'seed': 20261012,
                   'case_indices': indices, 'compilations': 665, 'optimization_levels': 5,
                   'byte_inputs_per_compilation': 256, 'prior_cases': first,
                   'resumed_cases': 133 - first, 'original_source_sha256': digest(candidate / 'tests/fuzz_frontend.c'),
                   'derived_driver_sha256': digest(driver), 'derived_binary_sha256': digest(binary),
                   'scope': record['scope']})
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


if '--worker' in sys.argv:
    with ThreadPoolExecutor(max_workers=2) as pool:
        futures = [pool.submit(run, label) for label in ('sequence', 'returns')]
        codes = [future.result() for future in futures]
    write_json(TERMINAL, {'exit_code': max(codes), 'campaign_exit_codes': codes, 'finished_utc': utc()})
    raise SystemExit(max(codes))
if '--poll' in sys.argv:
    job = json.loads(JOB.read_text())
    for tick in range(23):
        if TERMINAL.exists():
            print(TERMINAL.read_text())
            raise SystemExit(0)
        assert Path('/proc/sys/kernel/random/boot_id').read_text().strip() == job['boot_id'], 'WSL boot changed.'
        fields = (Path('/proc') / str(job['pid']) / 'stat').read_text().rsplit(')', 1)[1].split()
        assert int(fields[19]) == job['linux_start_ticks'] and fields[0] != 'Z', 'Worker stopped without terminal record.'
        time.sleep(2)
    for name in ('memory-sequence-prototype', 'memory-return-maintained'):
        path = BUILD / name / 'build/linux-isolated-resume-v2/progress.json'
        if path.exists():
            value = json.loads(path.read_text())
            print(name, value['status'], len(value['commands']), 'commands closed')
    raise SystemExit(0)
assert sys.argv[1:] == []
assert not JOB.exists() and not TERMINAL.exists()
with (BUILD / 'linux-resume-v2.log').open('x') as log:
    process = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), '--worker'], cwd=ROOT,
                               stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT,
                               start_new_session=True)
fields = (Path('/proc') / str(process.pid) / 'stat').read_text().rsplit(')', 1)[1].split()
write_json(JOB, {'pid': process.pid, 'linux_start_ticks': int(fields[19]), 'started_utc': utc(),
                'boot_id': Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
                'runner_sha256': digest(Path(__file__)), 'scope': 'Background Linux worker, bounded Windows poll invocations.'})
print('Started verified segmented recovery:', JOB.read_text())
