"""Fresh installation-path checks after the full v5 campaign passed."""
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
candidate = root / 'build/coherent-body-maintained-v5'
out = root / 'build/controlled-integration-v1'
project = out / 'project'
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
utc = lambda: datetime.datetime.now(datetime.timezone.utc).isoformat()
write = lambda path, value: path.write_text(json.dumps(value, indent=2) + '\n')
if '--worker' not in sys.argv:
    out.mkdir()
    shutil.copyfile(Path(__file__), out / 'runner.py')
    with (out / 'worker.log').open('x') as handle:
        worker = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), '--worker'], cwd=root,
            stdin=subprocess.DEVNULL, stdout=handle, stderr=subprocess.STDOUT, start_new_session=True)
    fields = (Path('/proc') / str(worker.pid) / 'stat').read_text().rsplit(')', 1)[1].split()
    write(out / 'job.json', {'pid': worker.pid, 'linux_start_ticks': int(fields[19]),
        'boot_id': Path('/proc/sys/kernel/random/boot_id').read_text().strip(), 'runner_sha256': digest(Path(__file__))})
    print('Started targeted138-input integration:', worker.pid)
    raise SystemExit(0)
record = {'status': 'running', 'commands': [], 'configurations': [], 'started_utc': utc(),
          'scope': 'Separate fresh normal and ASan/UBSan installation-path builds on the same138 inputs as the completed broad v5 campaign. Native393, controlled1270 and four exact documentation programs at five levels/both builtin modes are checked. Windows source application remains a separate hash-guarded step. C11, phase angles and inactive CPU-load bugs remain pending. Cybersecurity unchanged.'}
write(out / 'progress.json', record)
env = dict(os.environ, CC='gcc-11', CXX='g++-11', CKET_REAL_GCC='gcc-11', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
def step(name, argv, extra=None, expected=None):
    log = out / (f'{len(record["commands"]) + 1:02}-' + name + '.log')
    with log.open('x') as handle:
        result = subprocess.run(argv, cwd=project, env=dict(env, **(extra or {})), stdout=handle, stderr=subprocess.STDOUT)
    row = {'name': name, 'command': argv, 'returncode': result.returncode, 'log': log.name, 'log_sha256': digest(log)}
    record['commands'].append(row)
    write(out / 'progress.json', record)
    assert result.returncode == 0, log.read_text()[-6000:]
    if expected:
        assert log.read_text().count(expected) == 1
    print('Closed integration:', name, flush=True)
try:
    frozen = json.loads((candidate / 'build/maintained-sources.json').read_text())
    terminal = json.loads((candidate / 'build/full-campaign-v1/exit.json').read_text())
    assert len(frozen) == 138 and terminal['status'] == 'passed-in-segments' and terminal['exit_code'] == 0
    assert digest(candidate / 'build/full-campaign-v1/combined-verify.log') == terminal['combined_log_sha256']
    project.mkdir()
    for name, expected in frozen.items():
        assert digest(candidate / name) == expected
        target = project / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(candidate / name, target)
    shutil.copyfile(candidate / 'LICENSE', project / 'LICENSE')
    (project / '.deps').mkdir()
    (project / '.deps/gcc').symlink_to(root / '.deps/gcc', target_is_directory=True)
    spec = importlib.util.spec_from_file_location('controlled_integration_sources', project / 'scripts/archive-audit.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    assert module.source_hashes() == frozen
    record.update(source_files=138, source_sha256=frozen, broad_campaign_exit_sha256=digest(candidate / 'build/full-campaign-v1/exit.json'))
    draft = root / 'build/controlled-operands-contract-draft.md'
    shutil.copyfile(draft, out / 'contract-draft.md')
    examples = re.findall(r'^```c\n(.*?)^```', draft.read_text(), re.M | re.S)
    assert len(examples) == 4
    for configuration in ('normal', 'sanitized'):
        if configuration == 'normal':
            stage, options = project, {'CC': 'gcc-11'}
            step('fresh-normal-build', ['bash', 'scripts/build.sh'])
        else:
            stage = project / 'build/sanitized'
            script = (project / 'scripts/sanitize-all.sh').read_text()
            delimiter = '"$san_root/build/cket_tests" "$san_root"\n'
            assert script.count(delimiter) == 1
            prefix = script[:script.index(delimiter)]
            marker = 'cd "$(dirname "$0")/.."'
            assert prefix.count(marker) == 1
            wrapper = out / 'fresh-sanitizer-build-only.sh'
            wrapper.write_text(prefix.replace(marker, 'cd "$CKET_AUDIT_INTEGRATION"'))
            step('fresh-sanitizer-build', ['bash', str(wrapper)], {'CKET_AUDIT_INTEGRATION': str(project)})
            options = {'CC': str(stage / 'build/gcc-wrapper')}
        step(configuration + '-native', [str(stage / 'build/cket_tests'), str(stage)], options, '393 passed, 0 failed')
        step(configuration + '-controlled', [str(stage / 'build/validate_controlled_operands')], options, '1270 controlled operand checks passed;')
        rows = []
        for index, program in enumerate(examples):
            source = out / (f'contract-{index}.c')
            if source.exists():
                assert source.read_text() == program
            else:
                source.write_text(program)
            for level in ('-O0', '-O1', '-O2', '-O3', '-Os'):
                for builtin in ('-fbuiltin', '-fno-builtin'):
                    binary = out / (f'contract-{index}-' + configuration + '-' + level[1:] + '-' + builtin[2:])
                    result = subprocess.run([str(stage / 'build/cket'), str(source), level, builtin, '-o', str(binary)],
                        cwd=project, env=dict(env, **options), capture_output=True, text=True)
                    row = {'example': index, 'optimization': level, 'builtins': builtin, 'source_sha256': digest(source),
                           'compile_returncode': result.returncode, 'compile_stderr': result.stderr}
                    assert result.returncode == 0, row
                    result = subprocess.run([str(binary)], env=dict(env, **options), capture_output=True, text=True)
                    row.update(run_returncode=result.returncode, run_stderr=result.stderr)
                    assert result.returncode == 0 and not any(marker in result.stderr for marker in ('AddressSanitizer:', 'runtime error:')), row
                    rows.append(row)
                    write(out / (configuration + '-examples.json'), rows)
        assert len(rows) == 40 and module.source_hashes() == frozen
        record['configurations'].append({'configuration': configuration, 'native_checks': 393, 'controlled_operand_checks': 1270,
            'exact_contract_executions': 40, 'fresh_core_sha256': {name: digest(stage / 'build' / name)
                for name in ('cket', 'cket_backend', 'cket_plugin.so', 'cket_runtime.o', 'libcket_quantum.a')},
            'examples_sha256': digest(out / (configuration + '-examples.json'))})
        write(out / 'progress.json', record)
    record.update(status='passed-targeted-integration', exit_code=0, finished_utc=utc())
except BaseException as error:
    traceback.print_exc()
    (out / 'failure.txt').write_text(traceback.format_exc())
    record.update(status='failed-targeted-integration', exit_code=1, error=repr(error), finished_utc=utc())
write(out / 'exit.json', record)
write(out / 'progress.json', record)
raise SystemExit(record['exit_code'])
