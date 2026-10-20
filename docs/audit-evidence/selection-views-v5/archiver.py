from pathlib import Path
import hashlib
import importlib.util
import json
import shutil
import tarfile

root = Path('/tmp/quantum-audit-isolated-20261017')
project = root / 'build/select-views-v5'
proof_project = root / 'build/select-views-v4'
capsule = root / 'docs/audit-evidence/selection-views-v5'
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
load = lambda p: json.loads(p.read_text())
write = lambda p, r: p.write_text(json.dumps(r, indent=2) + '\n')
frozen = load(project / 'build/sources.json')
proof_sources = load(proof_project / 'build/sources.json')
previous = load(root / 'docs/audit-evidence/common-remainder-v2/verified-sources.json')
assert len(frozen) == len(proof_sources) == 151 and len(previous) == 148
assert all(digest(project / name) == sha for name, sha in frozen.items())
assert all(digest(proof_project / name) == sha for name, sha in proof_sources.items())
assert [name for name in frozen if frozen[name] != proof_sources[name]] == [
    'tests/selection_state.inc'
]
production = [name for name in frozen if name.startswith(('src/', 'include/')) or name == 'cket']
assert all(frozen[name] == proof_sources[name] for name in production)
qualifications, fresh_build_records, proofs = {}, {}, {}
directories = []
for c in ['normal', 'sanitized']:
    directory = project / ('build/qualification-' + c + '-v1')
    record = load(directory / 'exit.json')
    assert record['status'] == 'passed-targeted-selection-regression' and record['exit_code'] == 0
    assert record['source_sha256'] == frozen and len(record['commands']) == 6
    for row in record['commands']:
        assert row['returncode'] == 0 and digest(directory / row['log']) == row['log_sha256']
    for suite, expected in [
        ('validate_expressions', '1195 expression checks passed'),
        ('validate_shifts', '55 shift checks passed'),
        ('validate_division', '990 division checks passed'),
        ('cket_tests', '393 passed, 0 failed'),
        ('validate_inplace', '271 in-place checks passed'),
    ]:
        assert expected in (directory / (suite + '.log')).read_text(), (c, suite)
    assert record['tool_reuse']['production_inputs_byte_identical'] is True
    inherited = root / ('build/select-views-v3/build/qualification-' + c + '-v1')
    prior = load(inherited / 'exit.json')
    assert prior['status'] == 'failed-targeted-selection-regression'
    assert prior['core_sha256'] == record['core_sha256']
    assert prior['commands'][0]['name'] == 'fresh-build' and prior['commands'][0]['returncode'] == 0
    assert digest(inherited / prior['commands'][0]['log']) == prior['commands'][0]['log_sha256']
    assert all(frozen[name] == prior['source_sha256'][name] for name in production)
    qualifications[c], fresh_build_records[c] = record, prior
    directories.append(directory)
    proofs[c] = {}
    for kind, folder, status in [
        ('extremes', 'compiled-select-', 'passed-independent-compiled-selection'),
        ('shared_columns', 'compiled-mux-', 'passed-independent-compiled-mux'),
    ]:
        directory = proof_project / ('build/' + folder + c + '-v1')
        p = load(directory / 'exit.json')
        assert p['status'] == status and p['exit_code'] == 0
        assert p['source_sha256'] == proof_sources
        assert (
            p['captures'] == 64
            and p['ordinary_c_executions'] == 32
            and p['complete_state_checks'] == 192
        )
        assert p['core_sha256'] == record['core_sha256']
        for row in p['checks']:
            assert row['complex_state_error'] < 1e-10
            for name in ['qasm', 'map', 'events', 'reference']:
                assert digest(directory / row[name]) == row[name + '_sha256']
        proofs[c][kind] = p
        directories.append(directory)
attempts = {}
for version, error in [
    ('v2', 'negative constant is outside'),
    ('v3', 'requires an explicit C conversion'),
    ('v4', 'unexpectedly allocated a full-width mux temporary'),
]:
    p = root / ('build/select-views-' + version)
    attempted_sources = load(p / 'build/sources.json')
    assert all(
        attempted_sources[name] == sha
        for name, sha in frozen.items()
        if name != 'tests/selection_state.inc'
    )
    attempts[version] = {'source_sha256': attempted_sources, 'configurations': {}}
    for c in ['normal', 'sanitized']:
        directory = p / ('build/qualification-' + c + '-v1')
        failed = load(directory / 'exit.json')
        assert (
            failed['status'] == 'failed-targeted-selection-regression' and failed['exit_code'] == 1
        )
        assert error in failed['error']
        assert failed['source_sha256'] == attempted_sources
        attempts[version]['configurations'][c] = failed
        directories.append(directory)
capsule.mkdir()
shutil.copy2(Path(__file__), capsule / 'archiver.py')
write(capsule / 'verified-sources.json', frozen)
write(capsule / 'proof-sources.json', proof_sources)
write(capsule / 'qualification.json', qualifications)
write(capsule / 'inherited-fresh-builds.json', fresh_build_records)
write(capsule / 'earlier-test-attempts.json', attempts)
for filename, base, source_map in [
    ('sources.tar.gz', project, frozen),
    ('proof-sources.tar.gz', proof_project, proof_sources),
]:
    with tarfile.open(capsule / filename, 'x:gz', compresslevel=6) as archive:
        for relative in source_map:
            archive.add(base / relative, arcname=relative, recursive=False)
members = []
with tarfile.open(capsule / 'evidence.tar.gz', 'x:gz', compresslevel=6) as archive:
    for directory in directories:
        for path in sorted(directory.iterdir()):
            if path.is_file() and path.suffix in {
                '.c',
                '.inc',
                '.py',
                '.json',
                '.qasm',
                '.log',
                '.sh',
            }:
                relative = str(path.relative_to(root))
                archive.add(path, arcname=relative, recursive=False)
                members.append({'path': relative, 'sha256': digest(path)})
    for version in ['v2', 'v3', 'v4']:
        for relative in ['tests/selection_state.inc', 'build/sources.json']:
            path = root / ('build/select-views-' + version) / relative
            name = str(path.relative_to(root))
            archive.add(path, arcname=name, recursive=False)
            members.append({'path': name, 'sha256': digest(path)})
    for diagnostic in ['v1', 'v2']:
        directory = proof_project / ('build/budget-diagnostic-' + diagnostic)
        for path in sorted(directory.iterdir()):
            if path.is_file() and path.suffix in {'.c', '.inc', '.log', '.json'}:
                relative = str(path.relative_to(root))
                archive.add(path, arcname=relative, recursive=False)
                members.append({'path': relative, 'sha256': digest(path)})
write(capsule / 'evidence-files.json', members)
broad_directory = project / 'build/full-project-v1/build/full-campaign-v1'
broad_record = load(
    broad_directory / ('exit.json' if (broad_directory / 'exit.json').exists() else 'progress.json')
)
assert broad_record['source_files'] == 151
write(capsule / 'broad-campaign-at-archive.json', broad_record)
(capsule / 'README.es.md').write_text(
    '''# Selecciones enteras mediante vistas internas

Esta revisión reduce el trabajo privado de `condición ? sí : no` sobre enteros. Las columnas
idénticas de ambas ramas usan la misma expresión interna; dos constantes distintas usan el control
o su complemento. Las demás columnas se calculan una vez por par de expresiones y se reutilizan
dentro del plan. Cada bit del resultado público sigue teniendo su propio qubit y recibe una X
controlada; no se copian estados cuánticos arbitrarios ni se comparten los qubits públicos. El plan
se invierte después de copiar el resultado, sin medir ni reiniciar.

El caso que antes fallaba al seleccionar directamente entre INT64_MIN e INT64_MAX ahora pasa la
prueba completa con su resultado de resto y las inversas reales del compilador. El fallo original
de 148 entradas sigue en el archivo anterior; no se cambia su estado histórico.

La revisión final tiene 151 entradas congeladas. Pasan, por configuración normal y ASan/UBSan,
1195 comprobaciones de expresiones, 55 de desplazamientos, 990 de división, 393 nativas y 271 de
actualizaciones. Se añadieron 288 dominios finitos de selección: todos los bytes, ambas etiquetas
y polaridades del control, resultados de 8/32/64 bits con y sin signo, columnas compartidas o
complementadas, selección anidada y ramas aritméticas. El recorrido de puertas compara todos los
qubits, conserva las fases y comprueba auxiliares cero tras invertir. Son pruebas de acción sobre
bases; no simulaciones densas de registros de 64 bits. Sus perfiles usan como límite ancho del
resultado más 40 qubits: nueve entradas públicas y hasta 31 columnas/guardas privados.

Los núcleos reutilizados proceden de reconstrucciones normales y ASan/UBSan de la revisión previa,
con todas las fuentes de producción idénticas. Se verifican sus hashes, se recompila el validador
cambiado y se ejecutan las cinco suites sobre las fuentes finales. El registro de esa revisión
previa conserva su fallo del test; un build correcto no se presenta como una suite correcta. No se
afirma una reconstrucción nueva del núcleo en esta ejecución dirigida.

Dos matrices independientes de programas realmente compilados pasan 768 estados complejos
completos: por configuración, cada matriz tiene 64 capturas, 32 referencias C y 192 estados, en tres
puntos antes/después de la limpieza. La primera comprueba selecciones directas de límites
int32/int64/uint64, resultados bool/byte y divisores CPU modificados después de capturarlos. La
segunda prepara los 256 bytes coherentes y comprueba resultados completos de 64 bits, columnas
repetidas/complementadas, condiciones anidadas, ramas con restos y ambas polaridades. Incluyen
fases distintas, dos niveles GCC, ambos modos de builtins y ambas políticas de síntesis. El máximo
observado en estas matrices compiladas es 102 qubits. Se comprueban las inversas emitidas por el
compilador y todos los auxiliares cero; no se inserta una inversa manual de apoyo.

Estas pruebas de estados conservan su revisión de 151 entradas anterior al ajuste del presupuesto
del test. El único archivo diferente de la revisión final es `tests/selection_state.inc`; todas
las fuentes de producción son idénticas. `proof-sources.tar.gz` conserva esa identidad por separado.
No se atribuyen ejecuciones nuevas a las pruebas ya cerradas.

Los intentos previos del test quedan conservados en `earlier-test-attempts.json` y `evidence.tar.gz`:
máximos sin signo codificados como constantes negativas, máscaras sin conversión explícita al tipo
de la operación y un presupuesto que omitía parte del trabajo de selecciones anidadas. El
verificador rechazó los dos primeros IR incorrectos; el tercer caso usaba 98 qubits frente al
presupuesto inicial de 96. La copia diagnóstica se identifica como tal y no sustituye las suites.

`sources.tar.gz` y `verified-sources.json` describen la revisión final. `qualification.json`
conserva los doce comandos cerrados; `inherited-fresh-builds.json`, la procedencia de las
herramientas; `evidence-files.json`, los hashes de cada miembro de la evidencia comprimida.
`broad-campaign-at-archive.json` es una fotografía de la nueva campaña amplia, no un resultado de
éxito. La regresión amplia y el CI de esta revisión siguen separados. C11 completo, selecciones
generales que exceden los recursos, la división común y otros contextos siguen pendientes. La
ciberseguridad permanece clásica y no se afirma ejecución en hardware cuántico ni aceleración.
'''
)
spec = importlib.util.spec_from_file_location('formatter', root / 'scripts/format.py')
formatter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(formatter)
for name in ['README.es.md', 'archiver.py']:
    path = capsule / name
    path.write_text(formatter.format_text(path, path.read_text()))
stats = lambda p: {
    'captures': p['captures'],
    'ordinary_c_executions': p['ordinary_c_executions'],
    'complete_state_checks': p['complete_state_checks'],
    'maximum_amplitude_error': max(row['complex_state_error'] for row in p['checks']),
    'maximum_physical_wires': max(row['physical_wires'] for row in p['checks']),
}
summary = {
    'status': 'verified-targeted-production-identical-proofs-broad-pending',
    'source_files': 151,
    'source_sha256': frozen,
    'previous_integrated_source_sha256': previous,
    'changed_sources_from148': [name for name, sha in frozen.items() if previous.get(name) != sha],
    'proof_source_sha256': proof_sources,
    'production_sources_identical_to_proof': production,
    'changed_sources_from_proof': ['tests/selection_state.inc'],
    'checks_per_configuration': {
        'expressions': 1195,
        'shifts': 55,
        'division': 990,
        'native': 393,
        'inplace': 271,
    },
    'new_finite_selection_domains_per_configuration': 288,
    'targeted_commands_closed_zero': 12,
    'targeted_core_build_scope': 'Reused byte-verified cores from prior fresh production-identical builds; changed validator rebuilt. Fresh broad rebuilds are separate.',
    'independent_compiled_states': {
        c: {kind: stats(p) for kind, p in rows.items()} for c, rows in proofs.items()
    },
    'earlier_test_attempts': {
        version: {
            c: {'status': f['status'], 'exit_code': f['exit_code'], 'error': f['error']}
            for c, f in row['configurations'].items()
        }
        for version, row in attempts.items()
    },
    'broad_campaign_status_at_archive': broad_record['status'],
    'broad_regression': 'pending',
    'remote_ci': 'pending',
    'evidence_members': len(members),
    'artifact_sha256': {p.name: digest(p) for p in capsule.iterdir() if p.is_file()},
}
write(capsule / 'integration-summary.json', summary)
print(
    json.dumps(
        {
            'status': summary['status'],
            'sources': 151,
            'full_complex_states': sum(
                row['complete_state_checks']
                for rows in summary['independent_compiled_states'].values()
                for row in rows.values()
            ),
            'evidence_members': len(members),
        }
    )
)
