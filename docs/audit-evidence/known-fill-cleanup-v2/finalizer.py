from pathlib import Path
import hashlib
import importlib.util
import json
import shutil
import tarfile

root = Path('/tmp/quantum-audit-isolated-20261017')
capsule = root / 'docs/audit-evidence/known-fill-cleanup-v2'
checks = root / 'build/known-fill-root-checks-v1'
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
record = json.loads((checks / 'exit.json').read_text())
summary = json.loads((capsule / 'integration-summary.json').read_text())
assert record['status'] == 'passed-fresh-applied-root-checks' and record['exit_code'] == 0
assert record['exact_program_executions'] == 20 and len(record['commands']) == 21
assert record['source_sha256'] == summary['source_sha256']
assert all(digest(root / name) == sha for name, sha in record['source_sha256'].items())
for row in record['commands']:
    assert row['returncode'] == 0 and digest(checks / row['log']) == row['log_sha256']
shutil.copy2(checks / 'exit.json', capsule / 'root-checks.json')
shutil.copy2(checks / 'fresh-normal-build.log', capsule / 'root-build.log')
for row in record['programs'].values():
    assert digest(checks / row['source']) == row['source_sha256']
    shutil.copy2(checks / row['source'], capsule / row['source'])
with tarfile.open(capsule / 'root-checks.tar.gz', 'x:gz', compresslevel=6) as archive:
    for p in sorted(checks.iterdir()):
        if p.is_file() and p.suffix in {'.py', '.json', '.log', '.c'}:
            archive.add(p, arcname=p.name, recursive=False)
path = capsule / 'README.es.md'
text = path.read_text()
replacements = {
    'Las146 entradas': 'Las 146 entradas',
    '`memset`,393': '`memset`, 393',
    'nativos,1270': 'nativos, 1270',
    'operandos,2332': 'operandos, 2332',
    'historia,271': 'historia, 271',
    'actualizaciones y360': 'actualizaciones y 360',
    'añadieron14': 'añadieron 14',
    'producen140': 'producen 140',
    'pasó3528': 'pasó 3528',
    'completos:588': 'completos: 588',
    'capturas y1764': 'capturas y 1764',
    'con294': 'con 294',
    'de600 estados': 'de 600 estados',
    'negativos de8,16 y32': 'negativos de 8, 16 y 32',
    'parcial de64': 'parcial de 64',
    'límite de128': 'límite de 128',
    'estas146 entradas': 'estas 146 entradas',
    'a144 entradas': 'a 144 entradas',
}
for before, after in replacements.items():
    text = text.replace(before, after)
text += '''
La aplicación en la raíz Linux se comprobó con otra reconstrucción normal y20 ejecuciones de los
dos programas exactos: `cleanup-interference.c` y `observation-order.c`, en los cinco niveles GCC y
ambos modos de builtins. `root-checks.json` y `root-checks.tar.gz` conservan los cierres y las fuentes.
Son controles de aplicación, separados de las suites normales e instrumentadas del candidato.
'''.replace(
    'y20 ejecuciones', 'y 20 ejecuciones'
)
path.write_text(text)
shutil.copy2(Path(__file__), capsule / 'finalizer.py')
spec = importlib.util.spec_from_file_location('format_known_fill_final', root / 'scripts/format.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
for name in ['README.es.md', 'finalizer.py']:
    path = capsule / name
    formatted = module.format_text(path, path.read_text())
    assert module.format_text(path, formatted) == formatted
    path.write_text(formatted)
summary['root_verification'].update(
    fresh_root_build='passed',
    exact_program_executions=20,
    exact_program_source_sha256={
        name: row['source_sha256'] for name, row in record['programs'].items()
    },
)
summary['artifact_sha256'] = {
    p.name: digest(p)
    for p in capsule.iterdir()
    if p.is_file() and p.name != 'integration-summary.json'
}
(capsule / 'integration-summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print(
    'Finalized146 integration: fresh normal root build and20 exact executions verified; whole regression pending.'
)
