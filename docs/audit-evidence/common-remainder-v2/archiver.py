from pathlib import Path
import hashlib
import importlib.util
import json
import shutil
import tarfile

root = Path('/tmp/quantum-audit-isolated-20261017')
old = root / 'build/common-remainder-v1'
project = root / 'build/common-remainder-v2'
capsule = root / 'docs/audit-evidence/common-remainder-v2'
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
write = lambda p, r: p.write_text(json.dumps(r, indent=2) + '\n')
load = lambda p: json.loads(p.read_text())
frozen = load(project / 'build/sources.json')
previous = load(old / 'build/sources.json')
parent = load(root / 'docs/audit-evidence/known-fill-cleanup-v2/verified-sources.json')
assert len(frozen) == 148 and len(previous) == 147 and len(parent) == 146
assert all(digest(project / name) == sha for name, sha in frozen.items())
assert all(digest(old / name) == sha for name, sha in previous.items())
production = [name for name in frozen if name.startswith(('src/', 'include/')) or name == 'cket']
assert all(previous[name] == frozen[name] for name in production)
qualification, proofs, edges, failures = {}, {}, {}, {}
for configuration in ['normal', 'sanitized']:
    qdir = project / ('build/qualification-' + configuration + '-v1')
    q = load(qdir / 'exit.json')
    assert q['status'] == 'passed-targeted-remainder-regression' and q['exit_code'] == 0
    assert q['source_sha256'] == frozen and len(q['commands']) == 5
    for row in q['commands']:
        assert row['returncode'] == 0 and digest(qdir / row['log']) == row['log_sha256']
    for suite, count in [
        ('validate_expressions', 907),
        ('validate_division', 990),
        ('cket_tests', 393),
        ('validate_inplace', 271),
    ]:
        text = (qdir / (suite + '.log')).read_text()
        assert str(count) in text, (configuration, suite, count)
    qualification[configuration] = q
    proofs[configuration] = load(
        old / ('build/compiled-remainder-' + configuration + '-v1/exit.json')
    )
    p = proofs[configuration]
    assert p['status'] == 'passed-independent-compiled-remainder' and p['exit_code'] == 0
    assert (
        p['source_sha256'] == previous
        and p['captures'] == 240
        and p['ordinary_c_executions'] == 120
        and p['complete_state_checks'] == 720
    )
    edges[configuration] = load(
        project / ('build/compiled-edges-' + configuration + '-v2/exit.json')
    )
    e = edges[configuration]
    assert e['status'] == 'passed-independent-compiled-remainder-edges' and e['exit_code'] == 0
    assert (
        e['source_sha256'] == frozen
        and e['captures'] == 88
        and e['ordinary_c_executions'] == 44
        and e['complete_state_checks'] == 264
    )
    failures[configuration] = load(
        project / ('build/compiled-edges-' + configuration + '-v1/exit.json')
    )
    f = failures[configuration]
    assert f['status'] == 'failed-independent-compiled-remainder-edges' and f['exit_code'] == 1
    assert (
        f['captures'] == 24
        and len(f['checks']) == 72
        and 'limited to 128 physical qubits' in f['error']
    )
    for directory, record in [
        (old / ('build/compiled-remainder-' + configuration + '-v1'), p),
        (project / ('build/compiled-edges-' + configuration + '-v2'), e),
        (project / ('build/compiled-edges-' + configuration + '-v1'), f),
    ]:
        for row in record['checks']:
            for field in ['qasm', 'map', 'events', 'reference']:
                assert digest(directory / row[field]) == row[field + '_sha256']
baseline = load(root / 'build/known-fill-cleanup-v2/build/common-types-normal-v1/exit.json')
common = load(old / 'build/common-types-normal-v2/exit.json')
assert baseline['captures'] == 632 and baseline['ordinary_c_executions'] == 320
assert common['captures'] == 640 and common['ordinary_c_executions'] == 320
assert sum(row['status'] == 'resource-limit' for row in baseline['cases']) == 4
assert all(row['status'] == 'passed-finite-complete-state' for row in common['cases'])
capsule.mkdir()
shutil.copy2(Path(__file__), capsule / 'archiver.py')
write(capsule / 'verified-sources.json', frozen)
write(capsule / 'qualification.json', qualification)
write(capsule / 'conditional64-resource-limit.json', failures)
for name, base, manifest in [
    ('sources.tar.gz', project, frozen),
    ('proof147-sources.tar.gz', old, previous),
]:
    with tarfile.open(capsule / name, 'x:gz', compresslevel=6) as archive:
        for relative in manifest:
            archive.add(base / relative, arcname=relative, recursive=False)
directories = [
    root / 'build/known-fill-cleanup-v2/build/common-types-normal-v1',
    old / 'build/common-types-normal-v2',
]
directories += [old / ('build/compiled-remainder-' + c + '-v1') for c in ['normal', 'sanitized']]
directories += [project / ('build/qualification-' + c + '-v1') for c in ['normal', 'sanitized']]
directories += [
    project / ('build/compiled-edges-' + c + '-v' + v)
    for c in ['normal', 'sanitized']
    for v in ['1', '2']
]
selected = []
with tarfile.open(capsule / 'evidence.tar.gz', 'x:gz', compresslevel=6) as archive:
    for directory in directories:
        for path in sorted(directory.iterdir()):
            if path.is_file() and path.suffix in {
                '.py',
                '.inc',
                '.json',
                '.qasm',
                '.log',
                '.c',
                '.sh',
            }:
                name = str(path.relative_to(root))
                archive.add(path, arcname=name, recursive=False)
                selected.append({'path': name, 'sha256': digest(path)})
write(capsule / 'evidence-files.json', selected)
(capsule / 'README.es.md').write_text(
    '''# Resto entero por constantes pequeñas

La revisión de 148 entradas añade un núcleo reversible para `%` con divisores de magnitud 2 a 16.
El camino existente de potencias de dos sigue teniendo prioridad. Se conserva el dividendo y se
acumulan sus bits ponderados en un residuo de hasta cuatro bits. Cada suma modular es una
permutación del residuo, implementada mediante X controladas y transposiciones por caminos Gray.
Se enumeran las codificaciones del pequeño acumulador, no los valores del dividendo. La corrección
de signo resta `2^ancho mod divisor` en la rama negativa y respeta el resto de C, incluido `INT_MIN`.
La inversa del plan limpia los auxiliares sin medir ni reiniciar.

La auditoría anterior sobre bool, caracteres, short e int, con variantes sin signo, tenía 632
capturas correctas y cuatro límites de recursos: `int % 3`. Con este núcleo pasan las 640 capturas
de sus 320 referencias C. Son perfiles finitos con dos etiquetas y diez expresiones; no cierran
todos los tipos ni sus contextos de C11.

En normal y ASan/UBSan pasan reconstrucciones nuevas y las suites de expresiones (907), división
(990), nativos (393) y actualizaciones (271), por configuración. Las 810 pruebas nuevas de
expresiones recorren todos los bytes y 47 etiquetas elegidas de cada dominio ancho, incluidos los
extremos y patrones deterministas. Comprueban promociones, resto firmado/sin signo, conversión a
bool, byte y entero completo de 32 bits, ambas ramas del control, fases y auxiliares cero tras la
inversa. Son recorridos de acción sobre bases de las puertas posteriores a la preparación; no una
simulación densa completa de 32 o 64 bits. El resultado completo de 64 bits sigue sujeto al límite
de 128 qubits junto con sus entradas y trabajo.

La prueba independiente de programas compilados sobre los 256 bytes pasa 720 estados complejos
completos por configuración: 240 capturas, 120 referencias C, todos los divisores de ±2 a ±16,
resultados int de 32 bits y tres puntos antes/después de las inversas de resultado y propietario.
Se conservan sus 147 entradas originales. Todas sus fuentes de producción son idénticas a las de
la revisión actual; las diferencias son el nuevo archivo de regresiones, su llamada y el contador
de archivo. No se presenta como otra ejecución sobre las 148 entradas.

Otra prueba actual pasa 264 estados completos por configuración: 88 capturas y 44 referencias C.
Usa dos etiquetas en límites de int32/int64/uint64, resultados bool/byte y divisores literales o CPU
modificados después de capturarlos. Comprueba las inversas reales del compilador y las fases, con
todos los auxiliares cero. Para 64 bits prepara las etiquetas mediante vistas de desplazamiento y
complemento: 0/INT64_MIN, -1/INT64_MAX o UINT64_MAX/INT64_MAX. Las referencias conservan las
conversiones del objetivo GCC Linux LP64. Entre ambas pruebas completas se verifican 1968 estados;
las 640 capturas de perfiles de tipos son una campaña distinta.

El intento anterior de seleccionar directamente entre INT64_MIN e INT64_MAX falló en ambas
configuraciones al preparar esa selección, antes de calcular el resto: superaba 128 qubits. Se
conservan el fallo, los eventos y su prefijo de 72 estados por configuración; ese prefijo no cierra
el experimento. La preparación por vistas posterior no elimina ese pendiente.

`sources.tar.gz` identifica las 148 entradas actuales. `proof147-sources.tar.gz` conserva las
fuentes de la prueba previa. `evidence.tar.gz` contiene fuentes de pruebas, oráculos independientes,
referencias, puertas exportadas, eventos, registros y cierres; `evidence-files.json` permite verificar
cada miembro por bytes. `qualification.json` mantiene los diez comandos cerrados. La regresión
completa y el CI de estas entradas siguen pendientes. No se afirma ejecución en hardware cuántico,
ventaja de velocidad ni C11 completo. La ciberseguridad permanece clásica.
'''
)
spec = importlib.util.spec_from_file_location('formatter', root / 'scripts/format.py')
formatter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(formatter)
for name in ['README.es.md', 'archiver.py']:
    path = capsule / name
    text = formatter.format_text(path, path.read_text())
    assert formatter.format_text(path, text) == text
    path.write_text(text)
stats = lambda record: {
    'captures': record['captures'],
    'ordinary_c_executions': record['ordinary_c_executions'],
    'complete_state_checks': record['complete_state_checks'],
    'maximum_amplitude_error': max(row['complex_state_error'] for row in record['checks']),
}
summary = {
    'status': 'verified-targeted-applied-broad-pending',
    'source_files': len(frozen),
    'source_sha256': frozen,
    'previous_integrated_source_sha256': parent,
    'changed_sources_from146': [name for name, sha in frozen.items() if parent.get(name) != sha],
    'proof147_source_sha256': previous,
    'production_sources_identical_to_proof147': production,
    'changed_sources_from_proof147': [
        name for name, sha in frozen.items() if previous.get(name) != sha
    ],
    'checks_per_configuration': {
        'expressions': 907,
        'division': 990,
        'native': 393,
        'inplace': 271,
    },
    'new_finite_basis_action_checks_per_configuration': 810,
    'targeted_commands_closed_zero': 10,
    'common_type_profiles': {
        'baseline_captures': 632,
        'baseline_resource_limits': 4,
        'current_captures': 640,
        'ordinary_c_executions': 320,
        'status': 'finite-profiles-passed-not-type-completeness',
    },
    'independent_compiled_byte_states_on_production_identical147': {
        c: stats(p) for c, p in proofs.items()
    },
    'independent_compiled_edge_states_on148': {c: stats(p) for c, p in edges.items()},
    'conditional64_resource_failure': {
        c: {
            'exit_code': f['exit_code'],
            'completed_captures_before_failure': f['captures'],
            'completed_state_checks_before_failure': len(f['checks']),
            'error': f['error'],
        }
        for c, f in failures.items()
    },
    'broad_regression': 'pending',
    'remote_ci': 'pending',
    'evidence_members': len(selected),
    'artifact_sha256': {p.name: digest(p) for p in capsule.iterdir() if p.is_file()},
}
write(capsule / 'integration-summary.json', summary)
print(
    json.dumps(
        {
            'status': summary['status'],
            'sources': len(frozen),
            'archived_members': len(selected),
            'compressed_bytes': (capsule / 'evidence.tar.gz').stat().st_size,
        }
    )
)
