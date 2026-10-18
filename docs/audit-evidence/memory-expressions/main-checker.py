"""Record separate fresh main checks for the single verified memory batch."""
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

root = Path(__file__).resolve().parent.parent
destination = root / 'docs/audit-evidence/memory-expressions'
report_path = destination / 'integration-summary.json'
report = json.loads(report_path.read_text())
assert report['status'] == 'verified-isolated-and-applied' and 'root_verification' not in report
spec = importlib.util.spec_from_file_location('memory_batch_main_archiver', root / 'scripts/archive-audit.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
assert len(report['source_sha256']) == 134 and module.source_hashes() == report['source_sha256']
programs = {}
for name in ('memset', 'memory-sequences'):
    source = (root / f'docs/c11-{name}.md').read_text()
    programs[name], = [program for program in re.findall(r'```c\n(.*?)```', source, re.S)
                       if '#include <quantum.h>' in program and 'int main(void)' in program]
if sys.argv[1:] == ['--prepare']:
    for name, program in programs.items():
        path = root / f'build/memory-batch-{name}-example.c'
        with path.open('x') as handle:
            handle.write(program)
    print('All 134 main inputs match the full isolated proof; both exact contract examples prepared.')
    raise SystemExit(0)
assert sys.argv[1:] == ['--finish']
logs = {}
results = []
for configuration in ('normal', 'sanitized'):
    stage = root if configuration == 'normal' else root / 'build/memory-batch-root-sanitized'
    environment = dict(os.environ, CC='gcc-11', CKET_REAL_GCC='gcc-11')
    if configuration == 'sanitized':
        environment['CC'] = str(stage / 'build/gcc-wrapper')
        environment['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
    log_path = root / f'build/memory-batch-root-{configuration}.log'
    with log_path.open('x') as handle:
        for name, program in programs.items():
            path = root / f'build/memory-batch-{name}-example.c'
            assert path.read_text() == program
            for level in ('-O0', '-O1', '-O2', '-O3', '-Os'):
                for builtins in ('-fbuiltin', '-fno-builtin'):
                    command = [str(stage / 'build/cket'), str(path), '--run', level, builtins,
                               '-o', str(root / f'build/memory-batch-{name}-{configuration}-{level[1:]}-{builtins[2:]}.out')]
                    result = subprocess.run(command, cwd=root, env=environment, text=True,
                                            capture_output=True, timeout=240)
                    row = {'configuration': configuration, 'contract': name,
                           'optimization': level, 'builtins': builtins, 'command': command,
                           'source_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                           'returncode': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr}
                    results.append(row)
                    handle.write(json.dumps(row) + '\n')
                    handle.flush()
                    assert result.returncode == 0, row
                    assert not re.search(r'ERROR: (?:AddressSanitizer|LeakSanitizer)|runtime error:|internal compiler error:', result.stderr), row
        handle.write('PASS 20 exact contract example executions: two programs, five GCC levels and both builtin modes.\n')
    logs[f'root-{configuration}.log'] = log_path
assert module.source_hashes() == report['source_sha256']
for name, path in logs.items():
    shutil.copyfile(path, destination / name)
for source, name in [
    (root / 'build/memory-batch-root-build.log', 'root-build.log'),
    (root / 'build/run-memory-batch-main-smoke.sh', 'main-checker.sh'),
    (Path(__file__).resolve(), 'main-checker.py'),
    *[(root / f'build/memory-batch-{name}-example.c', f'{name}-example.c') for name in programs],
]:
    shutil.copyfile(source, destination / name)
(destination / 'contract-example-results.json').write_text(json.dumps(results, indent=2) + '\n')
report['root_verification'] = {
    'status': 'passed', 'completed_utc': datetime.now(timezone.utc).isoformat(),
    'source_files': 134, 'checks_per_configuration': 20,
    'configurations': ['normal', 'ASan/UBSan'], 'exact_contract_examples_per_configuration': 2,
    'execution_worktree': str(root),
    'execution_scope': os.environ.get('CKET_AUDIT_WORKTREE_SCOPE', 'Repository working tree.'),
    'scope': 'Fresh main normal build and fresh instrumented plugin/runtime/driver/backend/engine build. Both exact contract programs run at all five optimization levels and both builtin modes: 20 executions/configuration. The full broad normal/sanitizer campaign, including 800 memset, 360 sequence and 70 QuEST checks, belongs to the source-identical isolated checkout. No intermediate 132-input main promotion or repeated full main regression is claimed.',
}
report['remaining'] = 'Current documentation and paper-manifest refresh pending. General C11, pointer/count/layout/observation-ordering and indirect ABI work remain incomplete. Cybersecurity stays classical.'
report['artifact_sha256'] = {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
                           for path in destination.iterdir()
                           if path.is_file() and path.name != 'integration-summary.json'}
report_path.write_text(json.dumps(report, indent=2) + '\n')
print('Fresh main normal/sanitizer checks passed: 20 exact contract executions each, all five GCC levels/both builtin modes, all 134 hashes match. Broad regression retained as isolated evidence.')
