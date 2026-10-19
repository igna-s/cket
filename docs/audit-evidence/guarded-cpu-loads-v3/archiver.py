"""Archive completed CPU-load probes, source identity and retained failures."""
from pathlib import Path
import datetime
import hashlib
import json
import shutil
import tarfile

root = Path('/tmp/quantum-audit-isolated-20261017')
candidate = root / 'build/guarded-cpu-loads-v3'
out = root / 'docs/audit-evidence/guarded-cpu-loads-v3'
out.mkdir()
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
write = lambda path, value: path.write_text(json.dumps(value, indent=2) + '\n')
frozen = json.loads((candidate / 'build/sources.json').read_text())
assert len(frozen) == 143
assert all(digest(candidate / name) == expected for name, expected in frozen.items())
targeted = json.loads((candidate / 'build/targeted-v1/exit.json').read_text())
assert targeted['status'] == 'passed-guarded-cpu-load-targeted' and targeted['exit_code'] == 0
assert len(targeted['commands']) == 12 and all(row.get('returncode') == 0 for row in targeted['commands'])
assert targeted['source_sha256'] == frozen
for row in targeted['commands']:
    path = candidate / 'build/targeted-v1' / row['log']
    assert digest(path) == row['log_sha256']
for configuration in ['normal', 'sanitized']:
    path = candidate / 'build/targeted-v1' / (configuration + '-query-proof.log')
    assert 'PASS 76 symbolic control-support results' in path.read_text()
    assert '393 passed, 0 failed' in (path.parent / (configuration + '-native.log')).read_text()
    assert '1270 controlled operand checks passed;' in (path.parent / (configuration + '-controlled_operands.log')).read_text()
    assert '3380 phase checks passed;' in (path.parent / (configuration + '-phase.log')).read_text()
matrices = {}
for configuration, folder in [('normal', 'load-probes-v1'), ('sanitized', 'load-probes-sanitized-v1')]:
    data = json.loads((candidate / 'build' / folder / 'exit.json').read_text())
    assert data['exit_code'] == 0 and data['rows_passed'] == 140 and data['rows_failed'] == 0
    assert len(data['rows']) == 140 and all(row['passed'] and row['compile_returncode'] == 0 and
        all(run['returncode'] == 0 for run in row['runs']) for row in data['rows'])
    assert data['native_executions'] == data['ordinary_c_executions'] == 420
    assert data['source_sha256'] == frozen
    for row in data['rows']:
        for run in row['runs']:
            assert not any(marker in run['stderr'] for marker in ['AddressSanitizer:', 'runtime error:'])
    write(out / ('matrix-' + configuration + '.json'), data)
    matrices[configuration] = dict(rows=140, native_executions=420, ordinary_c_executions=420)
write(out / 'targeted.json', targeted)

with tarfile.open(out / 'sources.tar.gz', 'x:gz') as archive:
    for name in sorted(frozen):
        archive.add(candidate / name, arcname=name, recursive=False)
history = {}
with tarfile.open(out / 'historical-sources.tar.gz', 'x:gz') as archive:
    for version in ['v1', 'v2']:
        project = root / ('build/guarded-cpu-loads-' + version)
        hashes = json.loads((project / 'build/sources.json').read_text())
        assert all(digest(project / name) == expected for name, expected in hashes.items())
        for name in sorted(hashes):
            archive.add(project / name, arcname=version + '/' + name, recursive=False)
        history[version] = {'source_sha256': hashes}
        exploration = json.loads((project / 'build/load-probes-v1/exit.json').read_text())
        history[version]['normal_matrix'] = {key: exploration[key] for key in
            ['status', 'exit_code', 'rows_passed', 'rows_failed', 'native_executions', 'ordinary_c_executions']}
        if version == 'v2':
            previous = json.loads((project / 'build/targeted-v1/exit.json').read_text())
            assert previous['status'] == 'failed-guarded-cpu-load-targeted' and previous['exit_code'] == 1
            history[version]['targeted_status'] = previous['status']
            assert all(frozen[name] == expected for name, expected in hashes.items()
                       if name.startswith(('src/', 'include/')))
assert history['v1']['normal_matrix']['rows_passed'] == 120
assert history['v1']['normal_matrix']['rows_failed'] == 20
assert history['v2']['normal_matrix']['rows_passed'] == 140
write(out / 'history.json', history)

selected = set()
for version in ['v1', 'v2', 'v3']:
    project = root / ('build/guarded-cpu-loads-' + version)
    for folder in ['load-probes-v1', 'load-probes-sanitized-v1', 'targeted-v1']:
        directory = project / 'build' / folder
        if not directory.is_dir(): continue
        for path in directory.iterdir():
            if path.is_file() and path.suffix in ['.json', '.c', '.py', '.log', '.sh']:
                selected.add(path)
for pattern in ['prepare-guarded-cpu-loads-v*.py', 'qualify-guarded-cpu-loads-v*.py',
                'probe-guarded-cpu-loads-v*.py', 'start-guarded-loads-v*.py', 'prove-control-support-query.c']:
    selected.update((root / 'build').glob(pattern))
with tarfile.open(out / 'scoped-evidence.tar.gz', 'x:gz') as archive:
    for path in sorted(selected):
        archive.add(path, arcname=path.relative_to(root).as_posix(), recursive=False)
for path in out.glob('*.tar.gz'):
    with tarfile.open(path) as archive:
        names = []
        for member in archive:
            assert member.isfile() and not member.name.startswith('/') and '..' not in Path(member.name).parts
            names.append(member.name)
        assert len(names) == len(set(names))
shutil.copyfile(Path(__file__), out / 'archiver.py')
(out / 'README.es.md').write_text('''# Lecturas CPU bajo controles cuánticos: pruebas dirigidas cerradas

Este candidato conserva 143 entradas de fuentes. Todavía no está integrado: el
árbol principal conserva la integración anterior de 138 entradas.

Las matrices normal y ASan/UBSan pasan 140 filas cada una: asignación, XOR,
suma, memset, fase, condición y acceso indexado; cinco optimizaciones, ambos
modos de builtins, controles de base y coherentes. Cada matriz ejecuta 420
programas nativos y 420 referencias C ordinarias, sin instrumentar estas últimas.

La calificación fresca cierra doce comandos con éxito. En cada configuración
pasan 393 pruebas nativas, 1270 de operandos, 3380 de fase y 76 comprobaciones
de la consulta simbólica. Estas últimas comparan amplitudes exactas, registros,
versiones, contadores y RNG antes y después: la consulta no observa el estado.

V1 conserva 20 fallos reales entre sus 140 filas. V2 pasa esa matriz, pero
su prueba de fases falla porque el capturador no interceptaba la consulta de
controles. V3 conserva el mismo compilador que V2 y corrige ese capturador para
consultar el contexto real capturado. Los fallos originales quedan archivados.

Faltan la prueba independiente de estados exportados específica de estas
lecturas, su regresión amplia y la integración. Los controles que no se pueden
demostrar inactivos conservan la obligación de evaluar la lectura. Esto no
certifica toda C11, hardware cuántico ni aceleración. La ciberseguridad sigue clásica.
''', encoding='utf-8')
summary = {'status': 'verified-targeted-cpu-loads-not-integrated',
           'archived_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
           'source_inputs': len(frozen), 'source_sha256': frozen,
           'closed_targeted_commands': 12, 'native_matrices': matrices,
           'per_configuration': {'native': 393, 'controlled_operands': 1270, 'phase': 3380, 'query': 76},
           'compiler_identical_to_v2': True,
           'pending': ['independent compiled-load complex states', 'broad regression', 'integration', 'complete C11'],
           'security_changes': False,
           'artifacts': {path.name: {'sha256': digest(path), 'bytes': path.stat().st_size}
                         for path in sorted(out.iterdir()) if path.is_file()}}
write(out / 'summary.json', summary)
print(json.dumps({'status': summary['status'], 'files': len(summary['artifacts']) + 1,
                  'source_inputs': len(frozen), 'archived_evidence_files': len(selected)}, indent=2))
