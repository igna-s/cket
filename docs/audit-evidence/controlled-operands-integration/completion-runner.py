"""Correct the prose-format artifact capture; preserve the original evidence."""
from pathlib import Path
import hashlib
import importlib.util
import io
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile

root = Path(__file__).resolve().parent.parent
out = root / 'build/controlled-artifact-recovery-v1'
destination = root / 'docs/audit-evidence/controlled-operands-integration'
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
write = lambda path, value: path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')
summary_path = destination / 'integration-summary.json'
summary = json.loads(summary_path.read_text())
frozen = summary['source_sha256']
for name, expected in frozen.items():
    assert digest(root / name) == expected
original_draft = root / 'build/controlled-integration-v1/contract-draft.md'
assert summary['artifact_sha256']['contract-draft.md'] == digest(original_draft)
assert digest(destination / 'contract-draft.md') != digest(original_draft)
code = lambda path: re.findall(r'^```c\n(.*?)^```', path.read_text(), re.M | re.S)
assert code(original_draft) == code(destination / 'contract-draft.md') == code(root / 'docs/c11-controlled-operands.md')
out.mkdir()
names = ['docs/audit-evidence/controlled-operands-integration/integration-summary.json',
         'docs/audit-evidence/controlled-operands-integration/README.es.md',
         'docs/paper/audit_paper.py', 'docs/paper/evidence/paper-audit.json',
         'docs/paper/evidence/paper-audit-en.json', 'docs/paper/evidence/sha256-manifest.json']
before = {name: digest(root / name) for name in names}
write(out / 'before.json', before)
with tarfile.open(destination / 'artifact-capture-before-fix.tar.gz', 'x:gz') as archive:
    for name, path in (('integration-summary.json', summary_path), ('contract-draft.md', original_draft)):
        data = path.read_bytes()
        member = tarfile.TarInfo(name)
        member.size, member.mode = len(data), 0o644
        archive.addfile(member, io.BytesIO(data))
recovery = {'status': 'corrected-artifact-capture-after-prose-format',
    'old_summary_sha256': before[names[0]], 'old_expected_draft_sha256': digest(original_draft),
    'formatted_draft_sha256': digest(destination / 'contract-draft.md'),
    'exact_compiled_programs_unchanged': len(code(original_draft)),
    'reason': 'The actual application runner captured artifact hashes before mdformat wrapped prose in its copied contract draft. All four C programs remained byte-identical to the compiled fixtures. Only artifact metadata and paper checks are corrected; compiler sources and closed tests remain identical. The original summary and draft are retained together in the before-fix archive.'}
write(destination / 'postcheck-artifact-recovery.json', recovery)
shutil.copyfile(Path(__file__), destination / 'completion-runner.py')
readme = destination / 'README.es.md'
readme.write_text(readme.read_text() + '\nEl [registro de recuperación](postcheck-artifact-recovery.json) conserva una captura de hash\nanterior al formato de la prosa del borrador. Los cuatro programas C probados conservaron sus\nbytes exactos. El resumen y borrador anteriores se guardan juntos; los hashes internos actuales\nse verifican también dentro de la auditoría de papers.\n')
path = root / 'docs/paper/audit_paper.py'
text = path.read_text()
marker = '    targets = [\n'
assert text.count(marker) == 1
text = text.replace(marker, '''    current_capsules = (
        ('controlled-operands-integration', current_integration),
        ('controlled-operands-full', current_full),
        ('coherent-control-full', previous_full),
        ('controlled-operands-v5', operand_v5),
        ('integrated137-remote-ci', integrated_ci),
    )
    check(
        'Current evidence: internal capsule artifacts match their recorded bytes after formatting',
        all(hashlib.sha256((ROOT / 'docs/audit-evidence' / folder / name).read_bytes()).hexdigest() == expected
            for folder, capsule in current_capsules for name, expected in capsule['artifact_sha256'].items()),
    )
''' + marker)
path.write_text(text)
sys.path.insert(0, str(root / '.deps/format-venv/lib/python3.10/site-packages'))
spec = importlib.util.spec_from_file_location('controlled_recovery_format', root / 'scripts/format.py')
formatter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(formatter)
for path in (readme, root / 'docs/paper/audit_paper.py'):
    formatted = formatter.format_text(path, path.read_text())
    assert formatter.format_text(path, formatted) == formatted
    path.write_text(formatted)
summary['artifact_capture_recovery'] = recovery['reason']
summary['artifact_sha256'] = {path.name: digest(path) for path in destination.iterdir() if path.is_file() and path != summary_path}
write(summary_path, summary)
commands = []
for argv in ([str(root / '.deps/format-venv/bin/python'), 'scripts/format.py', '--check'],
             [str(root / '.deps/paper-venv/bin/python'), 'docs/paper/audit_paper.py', '--language', 'all']):
    log = out / (str(len(commands) + 1) + '.log')
    with log.open('x') as handle:
        result = subprocess.run(argv, cwd=root, env=dict(os.environ, CC='gcc-11', CXX='g++-11'), stdout=handle, stderr=subprocess.STDOUT)
    commands.append({'command': argv, 'returncode': result.returncode, 'log': log.name, 'log_sha256': digest(log)})
    write(out / 'commands.json', commands)
    assert result.returncode == 0, log.read_text()[-7000:]
for name in ('paper-audit.json', 'paper-audit-en.json'):
    path = root / 'docs/paper/evidence' / name
    report = json.loads(path.read_text())
    assert report['status'] == 'passed'
    report['audit_checkout']['execution_scope'] = 'Current138 source hashes and closed broad/targeted compiler tests are unchanged. Artifact capture was corrected after prose formatting; the four compiled C programs remain byte-identical. Both paper audits also verify internal hashes of the five current capsules. Historical metadata/draft are preserved in the recovery archive. Complete C11, phase angles and CPU-load bugs remain pending; cybersecurity unchanged.'
    write(path, report)
for name, expected in frozen.items():
    assert digest(root / name) == expected
files = sorted(set(names) | {path.relative_to(root).as_posix() for path in destination.iterdir() if path.is_file() and path.name in
    ('artifact-capture-before-fix.tar.gz', 'postcheck-artifact-recovery.json', 'completion-runner.py')})
manifest = {'before_sha256': before, 'file_sha256': {name: digest(root / name) for name in files}, 'source_sha256': frozen}
write(out / 'manifest.json', manifest)
with tarfile.open(out / 'documents.tar', 'x') as archive:
    for name in files:
        data = (root / name).read_bytes()
        member = tarfile.TarInfo(name)
        member.size, member.mode = len(data), 0o644
        archive.addfile(member, io.BytesIO(data))
write(out / 'exit.json', {'exit_code': 0, 'archive_sha256': digest(out / 'documents.tar'), 'manifest_sha256': digest(out / 'manifest.json'), 'files': len(files)})
print('Corrected prose artifact capture and verified all current capsule hashes; compiler138 unchanged.')
