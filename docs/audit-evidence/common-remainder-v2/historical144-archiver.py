from pathlib import Path
import hashlib
import importlib.util
import json
import shutil
import tarfile

root = Path('/tmp/quantum-audit-isolated-20261017')
project = root / 'build/string-integration-v6/build/full-project-v1'
old = project / 'build/full-campaign-v1'
out = project / 'build/full-campaign-resume-v1'
capsule = root / 'docs/audit-evidence/common-remainder-v2'
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
load = lambda p: json.loads(p.read_text())
write = lambda p, r: p.write_text(json.dumps(r, indent=2) + '\n')
record = load(out / 'progress.json')
terminal = load(out / 'exit.json')
frozen = load(project / 'build/sources.json')
assert record['status'] == terminal['status'] == 'passed-in-segments'
assert record['exit_code'] == terminal['exit_code'] == 0
assert len(record['commands']) == 80 and len(frozen) == 144
assert frozen == load(root / 'docs/audit-evidence/strings-integration-v6/integration-summary.json')['source_sha256']
assert all(digest(project / name) == sha for name, sha in frozen.items())
assert len(load(old / 'progress.json')['commands']) == 59
assert not (old / 'exit.json').exists()
assert record['continuation']['previous_boot_id'] != record['continuation']['current_boot_id']
assert sum(row.get('reused_closed_command_from_previous_segment', False) for row in record['commands']) == 59
for row in record['commands']:
    assert row['returncode'] == 0 and digest(out / row['log']) == row['log_sha256']
assert digest(out / 'combined-verify.log') == terminal['combined_log_sha256']
for c in ['normal', 'sanitized']:
    fuzz = load(out / (c + '-fuzz-coverage.json'))
    assert fuzz['status'] == 'passed-in-segments' and fuzz['compilations'] == 665
    assert fuzz['seed'] == 20261012 and fuzz['case_indices'] == list(range(133))
    assert fuzz['optimization_levels'] == 5 and fuzz['byte_inputs_per_compilation'] == 256
    assert fuzz['original_source_sha256'] == frozen['tests/fuzz_frontend.c']
with tarfile.open(capsule / 'historical144-broad.tar.gz', 'x:gz', compresslevel=6) as archive:
    for segment, directory in [('original', old), ('continuation', out)]:
        for path in sorted(directory.iterdir()):
            if path.is_file() and path.suffix in {'.json', '.log', '.py', '.sh', '.c'}:
                archive.add(path, arcname=segment + '/' + path.name, recursive=False)
    archive.add(project / 'build/sources.json', arcname='sources.json', recursive=False)
shutil.copy2(Path(__file__), capsule / 'historical144-archiver.py')
write(capsule / 'historical144-broad-progress.json', record)
write(capsule / 'historical144-broad-exit.json', terminal)
summary = load(capsule / 'integration-summary.json')
summary['historical144_completed_broad'] = {
    'status': terminal['status'], 'exit_code': 0, 'source_sha256': frozen,
    'closed_commands': 80, 'reused_original_closed_commands': 59,
    'new_continuation_closed_commands': 21,
    'fuzz_compilations_per_configuration': 665, 'fuzz_seed': 20261012,
    'scope': 'Historical144 only; fresh builds in the interrupted original segment, identical tool hashes reused by the continuation. Original process has no successful terminal code. This does not certify146/147/148.',
}
path = capsule / 'README.es.md'
path.write_text(path.read_text() + '''

## Cierre posterior de la campaña histórica de 144 entradas

`historical144-broad.tar.gz` conserva también el cierre posterior de la campaña anterior: 80
comandos correctos, 59 reutilizados de su prefijo cerrado y 21 ejecutados en la continuación,
con 665 compilaciones de fuzzing por configuración y semilla 20261012. Los hashes de las 144
entradas coinciden con su archivo histórico. Las reconstrucciones originales y la reutilización
de sus herramientas están diferenciadas; el proceso interrumpido no recibe un código de éxito
inventado. Este cierre de 144 entradas no verifica las revisiones de 146, 147 o 148.
''')
spec = importlib.util.spec_from_file_location('formatter', root / 'scripts/format.py')
formatter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(formatter)
path.write_text(formatter.format_text(path, path.read_text()))
summary['artifact_sha256'] = {p.name: digest(p) for p in capsule.iterdir() if p.is_file() and p.name != 'integration-summary.json'}
write(capsule / 'integration-summary.json', summary)
print('Archived actual historical144 full completion:80 commands,59+21; current148 broad remains pending.')
