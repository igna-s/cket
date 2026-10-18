"""Add the verified controlled-operand matrix to the repository's recurrent checks."""
from pathlib import Path
import hashlib
import importlib.util
import json
import shutil
import sys

root = Path('/tmp/quantum-audit-isolated-20261017')
base = root / 'build/coherent-body-prototype-v4'
candidate = root / 'build/coherent-body-maintained'
candidate.mkdir()
(candidate / 'build').mkdir()
frozen = json.loads((base / 'build/sources.json').read_text())
for name, expected in frozen.items():
    assert hashlib.sha256((base / name).read_bytes()).hexdigest() == expected, name
    target = candidate / name
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(base / name, target)
shutil.copyfile(base / 'LICENSE', candidate / 'LICENSE')
(candidate / '.deps').mkdir()
(candidate / '.deps/gcc').symlink_to(root / '.deps/gcc', target_is_directory=True)
shutil.copyfile(root / 'build/validate_controlled_operands.c', candidate / 'tests/validate_controlled_operands.c')


def insert_after(name, anchor, addition):
    path = candidate / name
    source = path.read_text()
    assert source.count(anchor) == 1, (name, anchor)
    path.write_text(source.replace(anchor, anchor + addition))


insert_after('scripts/build.sh',
    '"$gcc_compiler" "${flags[@]}" tests/validate_pointer_differences.c tests/validation.c build/libcket_quantum.a -lm -o build/validate_pointer_differences\n',
    '"$gcc_compiler" "${flags[@]}" tests/validate_controlled_operands.c tests/validation.c -o build/validate_controlled_operands\n')
insert_after('scripts/sanitize-all.sh',
    '"$real_cc" "${flags[@]}" tests/validate_pointer_differences.c tests/validation.c "$san_root/build/libcket_quantum.a" -lm -o "$san_root/build/validate_pointer_differences"\n',
    '"$real_cc" "${flags[@]}" tests/validate_controlled_operands.c tests/validation.c -o "$san_root/build/validate_controlled_operands"\n')
insert_after('scripts/sanitize-all.sh',
    '"$san_root/build/validate_pointer_differences"\n',
    '"$san_root/build/validate_controlled_operands"\n')
insert_after('scripts/verify.sh',
    'build/validate_pointer_differences 2>&1 | tee build/verify-pointer-differences.log\n',
    'build/validate_controlled_operands 2>&1 | tee build/verify-controlled-operands.log\n')
insert_after('scripts/archive-audit.py',
    "        'pointer_differences': r'^170 pointer difference checks passed; native distances, finite coherent action and unsupported contexts have separate scopes\\.$',\n",
    "        'controlled_operands': r'^1270 controlled operand checks passed; native values, undefined-domain diagnostics, effect boundaries and independent archived states have separate scopes\\.$',\n")
insert_after('scripts/archive-audit.py',
    "            'pointer_difference_checks': 170,\n",
    "            'controlled_operand_checks': 1270,\n")
sys.path.insert(0, str(root / '.deps/format-venv/lib/python3.10/site-packages'))
spec = importlib.util.spec_from_file_location('controlled_maintained_format', candidate / 'scripts/format.py')
formatter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(formatter)
formatter.CLANG = root / '.deps/format-venv/bin/clang-format'
formatter.SHFMT = root / '.deps/format-tools/shfmt'
for name in ('tests/validate_controlled_operands.c', 'scripts/build.sh', 'scripts/sanitize-all.sh',
             'scripts/verify.sh', 'scripts/archive-audit.py'):
    path = candidate / name
    formatted = formatter.format_text(path, path.read_text())
    assert formatter.format_text(path, formatted) == formatted, name
    path.write_text(formatted)
spec = importlib.util.spec_from_file_location('controlled_maintained_sources', candidate / 'scripts/archive-audit.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
snapshot = module.source_hashes()
assert len(snapshot) == 138
(candidate / 'build/maintained-sources.json').write_text(json.dumps(snapshot, indent=2) + '\n')
print('Prepared 138-source maintained candidate with 1270 controlled-operand checks/configuration; ordinary C references remain separate.')
