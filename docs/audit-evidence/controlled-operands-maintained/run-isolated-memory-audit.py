"""Continue frozen memory campaigns on Linux storage, without Windows mounts.

This runner preserves interrupted logs and records each resumed command's real
exit code. Reused compiler binaries are identified separately from fresh builds.
It does not change WSL configuration or certify complete C11 support.
"""

from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
from pathlib import Path
import ast
import hashlib
import importlib.util
import json
import os
import re
import shutil
import subprocess
import threading
import time
import traceback


ROOT = Path(__file__).resolve().parent.parent
LOCK = threading.Lock()
FINAL = 'All compiler, runtime, example, synthesis and independent QASM checks passed.'


def utc():
    return datetime.now(timezone.utc).isoformat()


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, value):
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(value, indent=2) + '\n')
    temporary.replace(path)


def report(message):
    with LOCK:
        print(utc(), message, flush=True)


def source_check(candidate, manifest):
    spec = importlib.util.spec_from_file_location(
        candidate.name.replace('-', '_'), candidate / 'scripts/archive-audit.py'
    )
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    expected = json.loads(manifest.read_text())
    assert module.source_hashes() == expected, 'Frozen inputs changed.'
    return expected


def patterns(candidate):
    tree = ast.parse((candidate / 'scripts/archive-audit.py').read_text())
    values = [ast.literal_eval(node.value) for node in ast.walk(tree)
              if isinstance(node, ast.Assign)
              and any(isinstance(target, ast.Name) and target.id == 'patterns'
                      for target in node.targets)]
    assert len(values) == 1
    return values[0]


def check_transcript(candidate, content):
    for name, pattern in patterns(candidate).items():
        assert len(re.findall(pattern, content, re.M)) == 2, name
    assert content.count('PASS locale sanitizer boundary:') == 1
    assert not re.search(
        r'^FAIL\b|ERROR: (?:AddressSanitizer|LeakSanitizer)|runtime error:|'
        r'internal compiler error:|validation infrastructure failure|validation command timed out',
        content, re.M
    )
    assert content.rstrip().endswith(FINAL)


def run_campaign(label):
    candidate_name = 'memory-sequence-prototype' if label == 'sequence' else 'memory-return-maintained'
    candidate = ROOT / 'build' / candidate_name
    build = candidate / 'build'
    output = build / 'linux-isolated-resume-v1'
    output.mkdir()
    manifest = build / ('sequence-full-sources-before.json' if label == 'sequence'
                        else 'return-full-v3-sources-before.json')
    frozen = source_check(candidate, manifest)
    assert len(frozen) == (134 if label == 'sequence' else 136)
    # The sanitized directory's links are recreated inside this Linux copy only.
    san_root = build / 'sanitized'
    san_root.mkdir(exist_ok=True)
    for name in ('src', 'include', 'tests', 'examples'):
        target = san_root / name
        assert not target.exists() and not target.is_symlink()
        target.symlink_to(candidate / name, target_is_directory=True)
    shutil.copyfile(candidate / 'cket', san_root / 'cket')
    for folder in (build, san_root / 'build'):
        if folder.exists():
            for path in folder.iterdir():
                if path.is_file() and (path.read_bytes()[:4] == b'\x7fELF' or path.name == 'gcc-wrapper'):
                    path.chmod(path.stat().st_mode | 0o111)
    (candidate / 'cket').chmod(0o755)
    deps = candidate / '.deps'
    deps.mkdir(exist_ok=True)
    (deps / 'gcc').symlink_to(ROOT / '.deps/gcc', target_is_directory=True)
    env = dict(os.environ, CC='gcc-11', CXX='g++-11', CKET_REAL_GCC='gcc-11',
               CKET_FUZZ_CASES='128', CKET_FUZZ_SEED='20261012',
               UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    steps = []

    def step(name, command, extra=None):
        steps.append((name, command, dict(env, **(extra or {}))))

    if label == 'sequence':
        prefix = (build / 'sequence-full-verify.log').read_bytes()
        # Both interrupted sanitizer library attempts remain evidence, but only
        # the original prefix contributes completed-suite counts.
        assert (build / 'sequence-interrupted-prefix.log').read_bytes() == prefix
        assert '2100 libc boundary checks passed' not in (build / 'sequence-resume.log').read_text()
        wrapper = str(san_root / 'build/gcc-wrapper')
        step('library-sanitized', [str(san_root / 'build/validate_library')], {'CC': wrapper})
        step('examples-sanitized', [str(san_root / 'build/test_examples')], {'CC': wrapper})
    else:
        prefix = (build / 'return-full-v3-verify.log').read_bytes()
        for name in ('memset', 'memory_sequences', 'memory_returns', 'byte_snapshots',
                     'division', 'bindings', 'history', 'ctype', 'profiles',
                     'numeric_locale', 'library'):
            extra = {}
            if name in ('ctype', 'numeric_locale'):
                extra['LSAN_OPTIONS'] = f'suppressions={candidate}/tests/glibc-locale.lsan:print_suppressions=0'
            step(name, [str(build / f'validate_{name}')], extra)
        step('classical', ['bash', 'scripts/test-c11-classical.sh'])
        step('validation-build', ['bash', 'scripts/build-validation.sh'])
        step('frontend', [str(build / 'fuzz_frontend'), '--seed', '20261012', '--cases', '128'])
        step('growth', [str(build / 'benchmark_synthesis')])
        step('qasm', [str(build / 'validate_qasm')])
        step('locale-sanitizer', ['bash', 'scripts/test-locale-sanitizer.sh'])
        step('fresh-sanitizer-build-and-suites', ['bash', 'scripts/sanitize-all.sh'])
    wrapper = str(san_root / 'build/gcc-wrapper')
    step('classical-sanitized', ['bash', 'scripts/test-c11-classical.sh'],
         {'CC': wrapper, 'CKET_DRIVER': str(san_root / 'build/cket')})
    step('validation-sanitized-build', ['bash', 'scripts/build-validation.sh'], {'VALIDATION_SANITIZERS': '1'})
    # Copies are commands so their actual exit statuses are recorded too.
    for name in ('fuzz_frontend', 'benchmark_synthesis', 'validate_qasm', 'qasm_samples'):
        step(f'stage-{name}', ['cp', str(build / f'{name}-sanitized'), str(san_root / 'build')])
    step('frontend-sanitized', [str(san_root / 'build/fuzz_frontend-sanitized'),
                                '--seed', '20261012', '--cases', '128'], {'CC': wrapper})
    step('growth-sanitized', [str(san_root / 'build/benchmark_synthesis-sanitized')], {'CC': wrapper})
    step('growth-comparison', ['cmp', str(build / 'synthesis-growth.csv'),
                              str(san_root / 'build/synthesis-growth.csv')])
    step('qasm-sanitized', [str(san_root / 'build/validate_qasm-sanitized')],
         {'CC': wrapper, 'LSAN_OPTIONS': f'suppressions={candidate}/tests/glibc-locale.lsan:print_suppressions=0'})
    record = {
        'status': 'running', 'label': label, 'started_utc': utc(),
        'source_manifest_sha256': digest(manifest), 'source_inputs': len(frozen),
        'linux_worktree': str(candidate), 'original_verify_script_returncode': None,
        'original_interrupted_prefix_sha256': hashlib.sha256(prefix).hexdigest(),
        'environment': {name: subprocess.check_output(command, text=True).strip()
                        for name, command in {
                            'kernel': ['uname', '-srmo'], 'gcc': ['gcc-11', '-dumpfullversion'],
                            'libc': ['getconf', 'GNU_LIBC_VERSION']}.items()},
        'scope': ('Segmented completion after confirmed WSL restarts. Sources and existing binaries '
                  'were copied with verified hashes into Linux /tmp. Closed original suites are '
                  'retained; only incomplete and remaining suites run here. Existing build artifacts '
                  'are reused until a recorded fresh build. No Windows drive mounts, no WSL '
                  'configuration edits, no cybersecurity changes, no full C11 completion claim.'),
        'commands': []
    }
    write_json(output / 'progress.json', record)
    (output / 'original-prefix.log').write_bytes(prefix)
    combined = bytearray(prefix)
    try:
        for index, (name, command, environment) in enumerate(steps, 1):
            logfile = output / f'{index:02}-{name}.log'
            report(f'{label}: {name} ({index}/{len(steps)})')
            started = utc()
            with logfile.open('xb') as handle:
                process = subprocess.Popen(command, cwd=candidate, env=environment,
                                           stdin=subprocess.DEVNULL, stdout=handle,
                                           stderr=subprocess.STDOUT)
                while process.poll() is None:
                    time.sleep(2)
                code = process.returncode
            combined.extend(logfile.read_bytes())
            record['commands'].append({'name': name, 'command': command, 'returncode': code,
                                       'started_utc': started, 'finished_utc': utc(),
                                       'log': logfile.name, 'log_sha256': digest(logfile)})
            write_json(output / 'progress.json', record)
            assert code == 0, f'{name}: {code}; see {logfile}'
            report(f'{label}: {name} passed')
        combined.extend((FINAL + '\n').encode())
        check_transcript(candidate, combined.decode())
        source_check(candidate, manifest)
        (output / 'combined-verify.log').write_bytes(combined)
        record.update(status='passed-in-segments', exit_code=0, finished_utc=utc(),
                      combined_log_sha256=digest(output / 'combined-verify.log'))
    except BaseException as error:
        record.update(status='failed-resume', exit_code=1, error=repr(error), finished_utc=utc())
        (output / 'failure.txt').write_text(traceback.format_exc())
        report(f'{label}: {error!r}')
    write_json(output / 'progress.json', record)
    return record['exit_code']


def main():
    package = json.loads((ROOT / 'build/wsl-isolated-package.json').read_text())
    for name, expected in package['file_sha256'].items():
        assert digest(ROOT / name) == expected, name
    # The parent remains a foreground Windows -> Linux invocation for the whole
    # campaign; independent campaigns use subprocesses, not detached WSL jobs.
    with ThreadPoolExecutor(max_workers=2) as pool:
        pending = [pool.submit(run_campaign, label) for label in ('sequence', 'returns')]
        while not all(job.done() for job in pending):
            report('Linux-only audit is active; no Windows drive is required.')
            time.sleep(20)
        results = [job.result() for job in pending]
    report(f'Actual campaign exit statuses: {results}')
    return max(results)


if __name__ == '__main__':
    raise SystemExit(main())
