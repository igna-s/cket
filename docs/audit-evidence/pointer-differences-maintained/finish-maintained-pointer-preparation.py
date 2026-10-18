from pathlib import Path
import importlib.util
import json
import sys

root = Path('/tmp/quantum-audit-isolated-20261017')
candidate = root / 'build/pointer-difference-maintained'
assert not (candidate / 'build/maintained-sources.json').exists()
sys.path.insert(0, str(root / '.deps/format-venv/lib/python3.10/site-packages'))
spec = importlib.util.spec_from_file_location('maintained_difference_format', candidate / 'scripts/format.py')
formatter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(formatter)
formatter.CLANG = root / '.deps/format-venv/bin/clang-format'
formatter.SHFMT = root / '.deps/format-tools/shfmt'
for name in ('tests/validate_pointer_differences.c', 'scripts/build.sh', 'scripts/sanitize-all.sh',
             'scripts/verify.sh', 'scripts/archive-audit.py'):
    path = candidate / name
    formatted = formatter.format_text(path, path.read_text())
    assert formatter.format_text(path, formatted) == formatted, name
    path.write_text(formatted)
spec = importlib.util.spec_from_file_location('maintained_difference_sources', candidate / 'scripts/archive-audit.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
snapshot = module.source_hashes()
assert len(snapshot) == 137
(candidate / 'build/maintained-sources.json').write_text(json.dumps(snapshot, indent=2) + '\n')
print('Prepared recurrent 150-check validator on 137 frozen inputs; main compiler unchanged.')
