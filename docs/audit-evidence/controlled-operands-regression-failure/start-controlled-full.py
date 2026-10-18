"""Fresh broad proof for the maintained controlled-operand candidate."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys

root = Path(__file__).resolve().parent.parent
candidate = root / 'build/coherent-body-maintained'
terminal = json.loads((candidate / 'build/maintained-checks-v1/exit.json').read_text())
assert terminal['exit_code'] == 0 and terminal['status'] == 'passed-scoped-maintained'
assert all(row['checks'] == 1270 for row in terminal['configurations'])
frozen = json.loads((candidate / 'build/maintained-sources.json').read_text())
assert len(frozen) == 138
for name, expected in frozen.items():
    assert hashlib.sha256((candidate / name).read_bytes()).hexdigest() == expected, name
for configuration in terminal['configurations']:
    stage = candidate if configuration['configuration'] == 'normal' else candidate / 'build/sanitized'
    for name, expected in configuration['reused_tools_sha256'].items():
        assert hashlib.sha256((stage / 'build' / name).read_bytes()).hexdigest() == expected, name
    assert hashlib.sha256((stage / 'build/validate_controlled_operands').read_bytes()).hexdigest() == configuration['fresh_validator_sha256']
source = (root / 'build/run-pointer-difference-full.py').read_text()
before = "CANDIDATE = ROOT / 'build/pointer-difference-maintained'"
assert source.count(before) == 1
source = source.replace(before, "CANDIDATE = ROOT / 'build/coherent-body-maintained'")
for before, after in (
        ("'source_files': 137", "'source_files': 138"),
        ('len(frozen) == 137', 'len(frozen) == 138'),
        ('Fresh broad normal and ASan/UBSan builds on all 137 frozen inputs.',
         'Fresh broad normal and ASan/UBSan builds on all 138 frozen inputs.'),
        ('Started fresh 137-input broad campaign:', 'Started fresh 138-input broad campaign:'),
        ("'memory_returns', 'pointer_differences', 'byte_snapshots'",
         "'memory_returns', 'pointer_differences', 'controlled_operands', 'byte_snapshots'"),
        ('assert not stage.exists()', 'assert stage.is_dir()  # Prior scoped tools exist; this step rebuilds all core components.'),
):
    assert source.count(before) == 1, before
    source = source.replace(before, after)
source = source.replace('No main promotion, complete C11, hardware or speedup claim.',
    'Adds 1270 controlled-operand checks/configuration after its separate scoped normal/sanitizer/phase evidence. Prior scoped core files exist but both core configurations are rebuilt in this fresh campaign. Phase-angle and inactive invalid CPU reads remain pending. No main promotion, complete C11, hardware or speedup claim.')
path = root / 'build/run-controlled-full.py'
assert not path.exists()
compile(source, str(path), 'exec')
path.write_text(source)
subprocess.run([sys.executable, str(path)], cwd=root, check=True)
