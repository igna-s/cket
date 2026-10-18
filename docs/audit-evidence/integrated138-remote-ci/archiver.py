"""Verify remote workflow scope against the exact committed source bytes."""

from pathlib import Path, PurePosixPath
import ast
import hashlib
import importlib.util
import io
import json
import re
import shutil
import sys
import tarfile

root = Path('/tmp/quantum-audit-isolated-20261017')
destination = root / 'docs/audit-evidence/integrated138-remote-ci'
source = root / 'build/integrated138-source.tar'
metadata = json.loads((root / 'build/integrated138-ci-metadata.json').read_text())
head = '08b9f3c9d4b27a3ec7ad334a50b3d03ee898a1c0'
assert (
    metadata['headSha'] == head
    and metadata['status'] == 'completed'
    and metadata['conclusion'] == 'success'
)
assert {job['name'] for job in metadata['jobs']} == {'formatting', 'verify'}
assert all(
    job['status'] == 'completed' and job['conclusion'] == 'success' for job in metadata['jobs']
)
integration = json.loads(
    (
        root / 'docs/audit-evidence/controlled-operands-integration/integration-summary.json'
    ).read_text()
)
frozen = integration['source_sha256']
assert len(frozen) == 138
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
with tarfile.open(source) as archive:
    assert archive.pax_headers['comment'] == head
    names = set()
    committed = {}
    for member in archive:
        name = PurePosixPath(member.name)
        assert not name.is_absolute() and '..' not in name.parts and member.name not in names
        names.add(member.name)
        if member.isdir():
            continue
        assert member.isfile(), member.name
        if member.name in frozen or member.name == 'LICENSE':
            committed[member.name] = archive.extractfile(member).read()
assert set(frozen) <= set(committed)
for name, expected in frozen.items():
    assert hashlib.sha256(committed[name]).hexdigest() == expected, name
    assert digest(root / name) == expected, name
tree = ast.parse(committed['scripts/archive-audit.py'])
patterns = None
for node in ast.walk(tree):
    if isinstance(node, ast.Assign) and any(
        isinstance(target, ast.Name) and target.id == 'patterns' for target in node.targets
    ):
        assert patterns is None
        patterns = ast.literal_eval(node.value)
assert patterns
raw = (root / 'build/integrated138-ci.log').read_text()
assert raw.count(head) >= 4
payload = []
for line in raw.splitlines():
    parts = line.split('\t', 2)
    if len(parts) == 3 and parts[0] == 'verify':
        payload.append(re.sub(r'^\d{4}-\d\d-\d\dT\S+Z ', '', parts[2]))
text = '\n'.join(payload) + '\n'
assert (
    text.count('All compiler, runtime, example, synthesis and independent QASM checks passed.') == 1
)
assert not re.search(
    r'^FAIL\b|ERROR: (?:AddressSanitizer|LeakSanitizer)|^.*runtime error:|internal compiler error:|validation infrastructure failure|validation command timed out',
    text,
    re.M,
)
counts = {}
for name, pattern in patterns.items():
    if name == 'frontend':
        pattern = pattern.replace('665', '145').replace('20261012', '20261011')
    matches = re.findall(pattern, text, re.M)
    assert len(matches) == 2, (name, len(matches))
    counts[name] = {'normal_and_sanitized_matches': 2, 'result_lines': matches}
destination.mkdir()
shutil.copyfile(root / 'build/integrated138-ci-metadata.json', destination / 'ci-metadata.json')
shutil.copyfile(root / 'build/integrated138-ci.log', destination / 'ci.log')
(destination / 'verified-sources.json').write_text(json.dumps(frozen, indent=2) + '\n')
(destination / 'result-counts.json').write_text(json.dumps(counts, indent=2) + '\n')
with tarfile.open(destination / 'sources.tar.gz', 'x:gz') as archive:
    for name in [*frozen, 'LICENSE', 'verified-sources.json']:
        data = (
            (json.dumps(frozen, indent=2) + '\n').encode()
            if name == 'verified-sources.json'
            else committed[name]
        )
        member = tarfile.TarInfo(name)
        member.size = len(data)
        member.mode = 0o644
        archive.addfile(member, io.BytesIO(data))
shutil.copyfile(Path(__file__), destination / 'archiver.py')
(destination / 'README.es.md').write_text(
    '''# CI remoto de la integración de 138 entradas

El workflow del commit `08b9f3c9d4b27a3ec7ad334a50b3d03ee898a1c0` terminó correctamente el 18 de octubre de 2026. Se comprobaron los jobs de formato y verificación, el SHA registrado por checkout, la transcripción completa y los bytes de las 138 entradas del commit frente al manifiesto de la integración.

En normal y ASan/UBSan aparecen 393 casos nativos, 1270 controles de operandos y todas las suites del workflow. El fuzzing remoto usa 145 compilaciones por configuración, semilla 20261011; la campaña local ampliada de 665 compilaciones por configuración usa otra semilla y se conserva por separado. Los circuitos independientes son 70 por configuración: 64 estados y 6 canales.

Esto verifica el compilador ya integrado. No incorpora los prototipos posteriores de `phase` ni corrige las cargas CPU inválidas en ramas inactivas. C11 completo, ejecución en hardware y aceleración no quedan demostrados. La ciberseguridad permanece clásica.
'''
)
sys.path.insert(0, str(root / '.deps/format-venv/lib/python3.10/site-packages'))
spec = importlib.util.spec_from_file_location('ci138_format', root / 'scripts/format.py')
formatter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(formatter)
for name in ['archiver.py', 'README.es.md']:
    path = destination / name
    formatted = formatter.format_text(path, path.read_text())
    assert formatter.format_text(path, formatted) == formatted
    path.write_text(formatted)
summary = {
    'status': 'verified-remote-ci-passed',
    'head_sha': head,
    'run_url': metadata['url'],
    'source_files': 138,
    'source_sha256': frozen,
    'normal_and_sanitized': {
        'native_each': 393,
        'controlled_operand_checks_each': 1270,
        'fuzz_compilations_each': 145,
        'fuzz_seed': 20261011,
        'independent_circuits_each': 70,
        'independent_states_each': 64,
        'independent_channels_each': 6,
    },
    'jobs': [
        {
            key: job[key]
            for key in ['name', 'status', 'conclusion', 'startedAt', 'completedAt', 'url']
        }
        for job in metadata['jobs']
    ],
    'verified_suite_patterns': len(patterns),
    'committed_tar_sha256': digest(source),
    'scope': 'The actual completed repository workflow on exact committed138 inputs, independently matched to the current integration. All normal/sanitizer result lines are present. Remote default145 fuzz compilations/configuration and seed20261011 stay distinct from the larger local665 campaign. Later phase candidates, full C11, physical hardware and speedup are not certified. Cybersecurity stays classical.',
    'artifact_sha256': {
        path.name: digest(path) for path in destination.iterdir() if path.is_file()
    },
}
(destination / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print(
    'Verified completed remote CI,138 committed inputs,',
    len(patterns),
    'normal/SAN suites;145 fuzz/configuration.',
)
