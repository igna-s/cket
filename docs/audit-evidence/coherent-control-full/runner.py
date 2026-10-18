"""Fresh broad normal/sanitizer proof, with separately closed suites and fuzz ranges.

The repository's validation commands are split for durable progress. Compiler
and test inputs stay frozen. The derived fuzz harness keeps the original case
generator, scalar oracle, generated programs, seed and five compiler levels.
"""
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
CANDIDATE = ROOT / 'build/coherent-cpu-maintained'
OUT = CANDIDATE / 'build/full-campaign-v1'
spec = importlib.util.spec_from_file_location('difference_full_original', ROOT / 'build/run-isolated-memory-audit.py')
original = importlib.util.module_from_spec(spec)
spec.loader.exec_module(original)
digest, utc, write_json = original.digest, original.utc, original.write_json
namespace = {'__file__': str(ROOT / 'build/resume-linux-memory-audit-v2.py')}
text = Path(namespace['__file__']).read_text()
exec(compile(text[:text.index("\n\nif '--worker'")], namespace['__file__'], 'exec'), namespace)

if '--worker' not in sys.argv:
    OUT.mkdir()
    shutil.copyfile(Path(__file__), OUT / 'runner.py')
    with (OUT / 'worker.log').open('x') as log:
        worker = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), '--worker'], cwd=CANDIDATE,
            stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
    fields = (Path('/proc') / str(worker.pid) / 'stat').read_text().rsplit(')', 1)[1].split()
    write_json(OUT / 'job.json', {'pid': worker.pid, 'linux_start_ticks': int(fields[19]),
        'boot_id': Path('/proc/sys/kernel/random/boot_id').read_text().strip(), 'started_utc': utc(),
        'runner_sha256': digest(Path(__file__))})
    print('Started fresh 137-input broad campaign:', worker.pid)
    raise SystemExit(0)

manifest = CANDIDATE / 'build/maintained-sources.json'
record = {'status': 'running', 'started_utc': utc(), 'source_files': 137,
          'source_manifest_sha256': digest(manifest), 'commands': [],
          'scope': 'Fresh broad normal and ASan/UBSan builds on all 137 frozen inputs. Repository validation commands run as individually recorded suites, rather than one scripts/verify.sh process. Fuzz cases are split into archived derived-driver ranges; original generator/program/oracle and all 133 case indices, five levels, 256 inputs and seed 20261012 are retained. Counts are explicit aggregates. Includes the coherent CPU-predicate and inactive-arithmetic correction, with the former rejection now checked positively in the 170-check suite. Invalid conditional CPU-memory accesses remain pending. No main promotion, complete C11, hardware or speedup claim. No security configuration changes.'}
write_json(OUT / 'progress.json', record)
combined = bytearray()
env = dict(os.environ, CC='gcc-11', CXX='g++-11', CKET_REAL_GCC='gcc-11',
           UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')


def step(name, argv, extra=None, expected=None):
    log = OUT / f'{len(record["commands"]) + 1:03}-{name}.log'
    started = utc()
    print(started, name, flush=True)
    with log.open('xb') as handle:
        result = subprocess.run(argv, cwd=CANDIDATE, env=dict(env, **(extra or {})),
            stdin=subprocess.DEVNULL, stdout=handle, stderr=subprocess.STDOUT)
    row = {'name': name, 'command': argv, 'started_utc': started, 'finished_utc': utc(),
           'returncode': result.returncode, 'log': log.name, 'log_sha256': digest(log)}
    record['commands'].append(row)
    write_json(OUT / 'progress.json', record)
    assert result.returncode == 0, row
    data = log.read_bytes()
    assert b'\0' not in data
    if expected:
        assert len(re.findall(expected, data.decode(), re.M)) == 1, name
    combined.extend(data)
    return log


try:
    frozen = original.source_check(CANDIDATE, manifest)
    assert len(frozen) == 137
    quest = ROOT / 'build/validation-deps/quest-3.7.0.tar.gz'
    assert digest(quest) == '1a5939798a70b749dfa57cfaa116ece03a2d36aa59c1b712cb3dc2103ab1cded'
    dependencies = CANDIDATE / 'build/validation-deps'
    dependencies.mkdir(exist_ok=True)
    shutil.copyfile(quest, dependencies / quest.name)
    driver = namespace['prepare_fuzz_driver'](CANDIDATE, OUT)
    patterns = original.patterns(CANDIDATE)
    for configuration in ('normal', 'sanitized'):
        if configuration == 'normal':
            stage = CANDIDATE
            step('fresh-normal-build', ['bash', 'scripts/build.sh'])
            options = {'CC': 'gcc-11'}
        else:
            stage = CANDIDATE / 'build/sanitized'
            assert not stage.exists()
            script = (CANDIDATE / 'scripts/sanitize-all.sh').read_text()
            delimiter = '"$san_root/build/cket_tests" "$san_root"\n'
            assert script.count(delimiter) == 1
            prefix = script[:script.index(delimiter)]
            marker = 'cd "$(dirname "$0")/.."'
            assert prefix.count(marker) == 1
            prefix = prefix.replace(marker, 'cd "$CKET_AUDIT_CANDIDATE"')
            wrapper = OUT / 'fresh-sanitizer-build-only.sh'
            wrapper.write_text(prefix)
            step('fresh-sanitizer-build', ['bash', str(wrapper)], {'CKET_AUDIT_CANDIDATE': str(CANDIDATE)})
            options = {'CC': str(stage / 'build/gcc-wrapper')}
        record[configuration + '_fresh_core_sha256'] = {name: digest(stage / 'build' / name)
            for name in ('cket', 'cket_backend', 'cket_plugin.so', 'cket_runtime.o', 'libcket_quantum.a')}
        write_json(OUT / 'progress.json', record)
        for name in ('native', 'harness', 'shifts', 'expressions', 'inplace', 'memcopy', 'memcmp', 'strncmp',
                     'memset', 'memory_sequences', 'memory_returns', 'pointer_differences', 'byte_snapshots',
                     'division', 'bindings', 'history', 'ctype', 'profiles', 'numeric_locale', 'library', 'examples'):
            binary = 'cket_tests' if name == 'native' else 'test_examples' if name == 'examples' else 'validate_' + name
            argv = [str(stage / 'build' / binary)] + ([str(stage)] if name == 'native' else [])
            extra = dict(options)
            if name in ('ctype', 'numeric_locale'):
                extra['LSAN_OPTIONS'] = f'suppressions={CANDIDATE}/tests/glibc-locale.lsan:print_suppressions=0'
            step(configuration + '-' + name, argv, extra, patterns[name])
        step(configuration + '-classical', ['bash', 'scripts/test-c11-classical.sh'],
             dict(options, CKET_DRIVER=str(stage / 'build/cket')), patterns['classical'])
        step(configuration + '-validation-build', ['bash', 'scripts/build-validation.sh'],
             {'VALIDATION_SANITIZERS': str(int(configuration == 'sanitized'))})
        suffix = '-sanitized' if configuration == 'sanitized' else ''
        if suffix:
            for name in ('fuzz_frontend', 'benchmark_synthesis', 'validate_qasm', 'qasm_samples'):
                shutil.copyfile(CANDIDATE / 'build' / (name + suffix), stage / 'build' / (name + suffix))
                (stage / 'build' / (name + suffix)).chmod(0o755)
        fuzz = stage / 'build/fuzz_frontend-resumable-full'
        flags = ['-O2'] if not suffix else ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
        step(configuration + '-build-resumable-fuzz', ['gcc-11', '-std=c11', '-Wall', '-Wextra', '-Werror',
            '-Itests', *flags, str(driver), 'tests/validation.c', '-o', str(fuzz)])
        indices, logs = [], []
        for first in range(0, 133, 16):
            limit = min(first + 16, 133)
            log = step(f'{configuration}-fuzz-{first:03}-{limit:03}', [str(fuzz), '--seed', '20261012',
                '--cases', '128', '--first', str(first), '--limit', str(limit)], options)
            interval = [int(value) for value in re.findall(r'^PASS frontend (\d+): all 256 byte inputs at all 5 optimization levels$', log.read_text(), re.M)]
            assert interval == list(range(first, limit))
            indices.extend(interval)
            logs.append({'log': log.name, 'sha256': digest(log), 'first': first, 'limit': limit})
            write_json(OUT / (configuration + '-fuzz-progress.json'), {'indices': indices, 'segments': logs})
        assert indices == list(range(133))
        combined.extend(b'SEGMENTED FRONTEND COVERAGE: 133 original indices, five levels, 256 byte inputs.\n')
        combined.extend(b'665 generated frontend compilations passed; seed=20261012 (C generator, SplitMix64)\n')
        write_json(OUT / (configuration + '-fuzz-coverage.json'), {'status': 'passed-in-segments',
            'case_indices': indices, 'compilations': 665, 'seed': 20261012, 'optimization_levels': 5,
            'byte_inputs_per_compilation': 256, 'segments': logs,
            'original_source_sha256': digest(CANDIDATE / 'tests/fuzz_frontend.c'),
            'derived_driver_sha256': digest(driver), 'derived_binary_sha256': digest(fuzz)})
        step(configuration + '-growth', [str(stage / 'build' / ('benchmark_synthesis' + suffix))], options, patterns['synthesis'])
        if suffix:
            step('growth-equality', ['cmp', str(CANDIDATE / 'build/synthesis-growth.csv'), str(stage / 'build/synthesis-growth.csv')])
        step(configuration + '-qasm', [str(stage / 'build' / ('validate_qasm' + suffix))],
             dict(options, LSAN_OPTIONS=f'suppressions={CANDIDATE}/tests/glibc-locale.lsan:print_suppressions=0'), patterns['qasm'])
        if not suffix:
            step('locale-sanitizer-boundary', ['bash', 'scripts/test-locale-sanitizer.sh'])
        original.source_check(CANDIDATE, manifest)
        record[configuration + '_status'] = 'passed-in-segments'
        write_json(OUT / 'progress.json', record)
    combined.extend((original.FINAL + '\n').encode())
    original.check_transcript(CANDIDATE, combined.decode())
    original.source_check(CANDIDATE, manifest)
    (OUT / 'combined-verify.log').write_bytes(combined)
    record.update(status='passed-in-segments', exit_code=0, finished_utc=utc(),
                  combined_log_sha256=digest(OUT / 'combined-verify.log'))
except BaseException as error:
    traceback.print_exc()
    (OUT / 'failure.txt').write_text(traceback.format_exc())
    record.update(status='failed-broad-campaign', exit_code=1, error=repr(error), finished_utc=utc())
write_json(OUT / 'progress.json', record)
write_json(OUT / 'exit.json', {key: value for key, value in record.items() if key != 'commands'})
raise SystemExit(record['exit_code'])
