from pathlib import Path, PurePosixPath
import datetime
import hashlib
import importlib.util
import json
import shutil
import subprocess
import tarfile

root = Path('/tmp/quantum-audit-isolated-20261017')
project = root / 'build/known-fill-cleanup-v2'
out = root / 'docs/audit-evidence/known-fill-cleanup-v2'
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
read = lambda p: json.loads(p.read_text())
write = lambda p, r: p.write_text(json.dumps(r, indent=2) + '\n')
frozen = read(project / 'build/sources.json')
before = read(root / 'docs/audit-evidence/strings-integration-v6/integration-summary.json')[
    'source_sha256'
]
assert len(frozen) == 146 and len(before) == 144
assert all(digest(project / name) == sha for name, sha in frozen.items())
assert all(digest(root / name) == sha for name, sha in before.items())
changed = [name for name, sha in frozen.items() if before.get(name) != sha]
assert set(changed) == {
    'src/cket_plugin.cpp',
    'src/quantum_semantics.inc',
    'src/quantum_lifetimes.inc',
    'tests/validate_memset.c',
    'tests/memset_lifetimes.inc',
    'scripts/archive-audit.py',
}
qualifications = {}
states = {}
limits = {}
directories = []
for configuration in ['normal', 'sanitized']:
    initial = project / ('build/qualification-' + configuration + '-v2')
    resumed = project / ('build/qualification-' + configuration + '-resume-v1')
    original = read(initial / 'progress.json')
    continuation = read(resumed / 'exit.json')
    assert not (initial / 'exit.json').exists()
    assert continuation['status'] == 'passed-in-segments' and continuation['exit_code'] == 0
    assert continuation['source_sha256'] == original['source_sha256'] == frozen
    assert original['status'] == 'running'
    assert len(original['commands']) == (25 if configuration == 'normal' else 22)
    assert len(continuation['commands']) == (3 if configuration == 'normal' else 6)
    for record, folder, prefix in [
        (original, initial, 'initial'),
        (continuation, resumed, 'continuation'),
    ]:
        for row in record['commands']:
            log = folder / row.get('log', row['name'] + '.log')
            assert row['returncode'] == 0 and digest(log) == row['log_sha256']
    combined = [*original['commands'], *continuation['commands']]
    assert len(combined) == 28
    for name, marker in [
        ('validate_memset', '940 memset checks passed;'),
        ('cket_tests', '393 passed, 0 failed'),
        ('validate_controlled_operands', '1270 controlled operand checks passed;'),
        ('validate_history', '2332 versioned history checks passed;'),
        ('validate_inplace', '271 in-place checks passed;'),
        ('validate_memory_sequences', '360 memory sequence checks passed;'),
    ]:
        folder = initial if any(row['name'] == name for row in original['commands']) else resumed
        text = (folder / (name + '.log')).read_text()
        assert text.count(marker) == 1, (configuration, name)
    qualifications[configuration] = {'initial': original, 'continuation': continuation}
    proof_dir = project / ('build/compiled-scope-cleanup-' + configuration + '-v6')
    proof = read(proof_dir / 'exit.json')
    assert proof['exit_code'] == 0 and proof['source_sha256'] == frozen
    assert proof['captures'] == 588 and proof['ordinary_c_executions'] == 294
    assert len(proof['checks']) == proof['complete_state_checks'] == 1764
    for row in proof['checks']:
        for key in ['qasm', 'map', 'events', 'reference']:
            assert digest(proof_dir / row[key]) == row[key + '_sha256']
    states[configuration] = {
        'captures': 588,
        'ordinary_c_executions': 294,
        'complete_state_checks': 1764,
        'maximum_amplitude_error': max(row['complex_state_error'] for row in proof['checks']),
        'core_sha256': proof['core_sha256'],
    }
    signed_dir = project / ('build/signed-fill-cleanup-' + configuration + '-v1')
    signed = read(signed_dir / 'exit.json')
    assert signed['exit_code'] == 1 and signed['captures'] == 100 and len(signed['checks']) == 300
    assert 'limited to 128 physical qubits' in signed['error']
    assert {row['width'] for row in signed['checks']} == {8, 16, 32}
    limits[configuration] = {
        'actual_status': signed['status'],
        'exit_code': 1,
        'completed_captures_before_failure': 100,
        'completed_state_checks_before_failure': 300,
        'ordinary_c_executions_including_failed_case_reference': 51,
        'validated_prefix_widths': [8, 16, 32],
        'failure': 'signed64 low-byte fill reached the128-physical-qubit limit at O0/builtin; the wide proof did not pass',
        'error': signed['error'],
    }
    directories.extend([initial, resumed, proof_dir, signed_dir])
out.mkdir()
suffixes = {'.py', '.c', '.inc', '.json', '.qasm', '.log', '.txt', '.sh', '.stderr', '.ast'}
with tarfile.open(out / 'sources.tar.gz', 'x:gz', compresslevel=6) as archive:
    for name in frozen:
        archive.add(project / name, arcname=name, recursive=False)
    archive.add(project / 'LICENSE', arcname='LICENSE', recursive=False)
    archive.add(project / 'build/sources.json', arcname='verified-sources.json', recursive=False)
with tarfile.open(out / 'scoped-evidence.tar.gz', 'x:gz', compresslevel=6) as archive:
    selected = {
        p
        for d in directories
        for p in d.rglob('*')
        if p.is_file() and not p.is_symlink() and p.suffix in suffixes
    }
    for p in sorted(selected):
        archive.add(p, arcname=p.relative_to(root).as_posix(), recursive=False)
with tarfile.open(out / 'earlier-attempts.tar.gz', 'x:gz', compresslevel=6) as archive:
    selected = set()
    for version in ['v2', 'v3', 'v4']:
        for configuration in ['normal', 'sanitized']:
            d = root / ('build/compiled-scope-cleanup-' + configuration + '-' + version)
            selected.update(
                p
                for p in d.rglob('*')
                if p.is_file() and not p.is_symlink() and p.suffix in suffixes
            )
    earlier = root / 'build/known-fill-cleanup-v1'
    old = read(earlier / 'build/sources.json')
    assert len(old) == 145
    assert all(digest(earlier / name) == sha for name, sha in old.items())
    selected.update(earlier / name for name in old)
    for name in [
        'edge-probes-v1',
        'compiled-scope-cleanup-normal-v5',
        'qualification-normal-v1',
        'qualification-sanitized-build-v1',
    ]:
        selected.update(
            p
            for p in (earlier / 'build' / name).rglob('*')
            if p.is_file() and not p.is_symlink() and p.suffix in suffixes
        )
    selected.add(earlier / 'build/sources.json')
    for p in sorted(selected):
        archive.add(p, arcname=p.relative_to(root).as_posix(), recursive=False)
for path in out.glob('*.tar.gz'):
    with tarfile.open(path) as archive:
        members = archive.getmembers()
        assert len(members) == len({m.name for m in members})
        assert all(
            m.isfile()
            and not PurePosixPath(m.name).is_absolute()
            and '..' not in PurePosixPath(m.name).parts
            for m in members
        )
write(out / 'targeted-segments.json', qualifications)
write(out / 'signed-resource-limit.json', limits)
write(out / 'verified-sources.json', frozen)
shutil.copy2(Path(__file__), out / 'archiver.py')
(out / 'README.es.md').write_text(
    '''# Limpieza de temporales conocidos en `memset`

Esta integración conserva una definición invertible para los destinos enteros conocidos que no se observan ni transforman en la función. El compilador deshace esos temporales al terminar su vida. Los destinos que alimentan observaciones o transformaciones conservan la ruta mutable, incluido el orden de medición previo. La evaluación CPU se captura una vez y su inversa utiliza la captura original.

Las146 entradas pasaron, por configuración normal y ASan/UBSan, 940 comprobaciones de `memset`,393 casos nativos,1270 controles de operandos,2332 de historia,271 actualizaciones y360 expresiones de memoria. Se añadieron14 fixtures mantenidos que producen140 ejecuciones nativas adicionales por configuración. Dos segmentos conservan los cierres anteriores y repiten solo las suites sin estado terminal tras el cambio de arranque de WSL. No se inventa un código de salida para los procesos interrumpidos ni se afirma una ejecución única de `scripts/verify.sh`.

Una prueba independiente pasó3528 estados complejos completos:588 capturas y1764 estados en cada configuración, con294 referencias C por configuración. Los checkpoints solo exportan puertas y metadatos. Las asignaciones y llenados no inyectivos utilizan las inversas emitidas por el compilador; XOR, suma y efectos de fase tienen inversas explícitas en el programa C de prueba. Las entradas desconocidas retenidas permanecen como salida conjunta. La prueba comprueba fases distintas, cargas CPU válidas o ramas inactivas, mutaciones CPU posteriores y ceros en los temporales.

Otra prueba verificó un prefijo de600 estados con valores negativos de8,16 y32 bits, pero **terminó con error** en el llenado parcial de64 bits por el límite de128 qubits físicos. Los dos cierres fallidos, sus fuentes y su prefijo positivo quedan en `signed-resource-limit.json` y el archivo de evidencia. No se cuenta esa prueba ancha como aprobada.

`sources.tar.gz` contiene las fuentes exactas. `scoped-evidence.tar.gz` conserva referencias, circuitos, mapas, hashes, controladores y logs. `earlier-attempts.tar.gz` conserva los errores de los primeros oráculos, el rechazo original de limpieza y el candidato anterior que emitía una constante con signo fuera de su tipo. No se atribuyen esos errores del candidato a la versión original.

La aplicación por hashes al repositorio no equivale a una regresión ejecutada en Windows. La campaña completa y el CI de estas146 entradas siguen pendientes. El CI anterior aprobado corresponde a144 entradas y se conserva por separado. C11 completo, hardware físico y aceleración siguen sin demostrarse. La ciberseguridad permanece clásica.
'''
)
spec = importlib.util.spec_from_file_location('format_known_fill', root / 'scripts/format.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
for name in ['archiver.py', 'README.es.md']:
    path = out / name
    formatted = module.format_text(path, path.read_text())
    assert module.format_text(path, formatted) == formatted
    path.write_text(formatted)
backup = root / 'build/pre-known-fill-v2-main-backup'
backup.mkdir()
for name in changed:
    target = root / name
    if target.exists():
        saved = backup / name
        saved.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(target, saved)
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(project / name, target)
assert all(digest(root / name) == sha for name, sha in frozen.items())
summary = {
    'status': 'verified-targeted-segmented-applied-broad-pending',
    'source_files': 146,
    'source_sha256': frozen,
    'previous_integrated_source_sha256': before,
    'changed_sources': changed,
    'targeted_commands_closed_zero': 56,
    'checks_per_configuration': {
        'memset': 940,
        'native': 393,
        'controlled_operands': 1270,
        'history': 2332,
        'inplace': 271,
        'memory_sequences': 360,
    },
    'new_maintained_fixtures': 14,
    'extra_native_cases_per_configuration': 140,
    'exact_interference_program_executions_per_configuration': 10,
    'independent_compiled_scope_states': states,
    'signed_word_prefix_and_resource_failure': limits,
    'root_verification': {
        'platform': 'isolated Linux',
        'status': 'source-hashes-matched',
        'fresh_root_build': 'pending',
        'Windows_native_execution': False,
    },
    'broad_regression': {'source_files': 146, 'status': 'pending'},
    'remote_ci': {'source_files': 146, 'status': 'pending'},
    'remaining': 'Whole146 regression and remote CI;64-bit partial-fill resource budget; flow-sensitive observation/lifetime coverage beyond this conservative function analysis; broader C11 type/operator/library implementation. The later147-input small-remainder prototype is separate. Cybersecurity stays classical.',
    'published_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
    'artifact_sha256': {p.name: digest(p) for p in out.iterdir() if p.is_file()},
}
write(out / 'integration-summary.json', summary)
print(
    'Applied verified146 source inputs in Linux:',
    len(changed),
    'changed files;56 closed targeted commands,3528 independent scope states. Broad146 remains pending.',
)
