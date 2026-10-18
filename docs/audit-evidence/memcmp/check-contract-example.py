from pathlib import Path
import json
import os
import re
import subprocess

root = Path(__file__).resolve().parent.parent
source = root / 'build/memcmp-contract-example.c'
program, = re.findall(r'```c\n(.*?)```', (root / 'docs/c11-memcmp.md').read_text(), re.S)
source.write_text(program)
checks = []
for configuration in ('normal', 'sanitized'):
    tool_root = root if configuration == 'normal' else root / 'build/sanitized'
    environment = dict(os.environ, CC='gcc-11')
    if configuration == 'sanitized':
        environment.update(CC=str(tool_root / 'build/gcc-wrapper'), CKET_REAL_GCC='gcc-11', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    command = [str(tool_root / 'build/cket'), str(source), '--run']
    result = subprocess.run(command, env=environment, cwd=root, capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stderr
    assert not result.stderr.strip(), result.stderr
    checks.append({'configuration': configuration, 'returncode': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr})
(root / 'build/memcmp-contract-example-results.json').write_text(json.dumps({'status': 'passed', 'scope': 'Exact documented C example compiled and executed normally and with ASan/UBSan; smoke test, not an independent semantic proof.', 'checks': checks}, indent=2) + '\n')
print('Exact memcmp contract example compiled and executed in both configurations.')
