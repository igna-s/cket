"""Publish closed phase proofs and preserve failed or interrupted precursors."""

from pathlib import Path
import hashlib
import importlib.util
import io
import json
import shutil
import sys
import tarfile

root = Path('/tmp/quantum-audit-isolated-20261017')
candidate = root / 'build/phase-maintained-v6'
destination = root / 'docs/audit-evidence/deferred-phase-v6'
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
read = lambda path: json.loads(path.read_text())
frozen = read(candidate / 'build/sources.json')
assert len(frozen) == 143
assert all(digest(candidate / name) == expected for name, expected in frozen.items())
qualification = read(candidate / 'build/targeted-v1/exit.json')
assert (
    qualification['status'] == 'passed-maintained-phase-targeted'
    and qualification['exit_code'] == 0
)
assert qualification['source_sha256'] == frozen and len(qualification['commands']) == 4
for row in qualification['commands']:
    assert (
        row['returncode'] == 0
        and digest(candidate / 'build/targeted-v1' / row['log']) == row['log_sha256']
    )
for name in ['normal-phase.log', 'sanitized-phase.log']:
    assert '3380 phase checks passed;' in (candidate / 'build/targeted-v1' / name).read_text()
states = {}
for configuration in ['normal', 'sanitized']:
    directory = candidate / 'build' / ('phase-state-proof-' + configuration + '-v1')
    proof = read(directory / 'exit.json')
    assert (
        proof['status'] == 'passed-phase-state-proof'
        and proof['exit_code'] == 0
        and proof['source_sha256'] == frozen
    )
    assert (
        proof['captures'] == proof['ordinary_c_executions'] == 400 and len(proof['checks']) == 800
    )
    for row in proof['checks']:
        assert row['configuration'] == configuration and row['complete_state_error'] < 1e-10
        for kind in ['trace', 'qasm', 'map']:
            assert digest(directory / row[kind]) == row[kind + '_sha256']
        assert read(directory / row['map'])['phase_bits'] == row['angle_bits']
    states[configuration] = {
        'captures': 400,
        'ordinary_c_executions': 400,
        'complete_state_checks': 800,
        'maximum_amplitude_error': max(row['complete_state_error'] for row in proof['checks']),
    }
v2 = root / 'build/phase-prototype-v2'
normal = read(v2 / 'build/phase-probes-v2/results.json')
sanitized = read(v2 / 'build/phase-probes-sanitized-v2/results.json')
assert (normal['rows_passed'], normal['rows_failed']) == (240, 0)
assert (sanitized['rows_passed'], sanitized['rows_failed']) == (70, 170)
original = read(v2 / 'build/phase-state-proof-v1/exit.json')
assert original['exit_code'] == 1 and len(original['checks']) == 800
assert read(v2 / 'build/qualification-continuation-v2/exit.json')['exit_code'] == 1
for version in ['phase-maintained-v4', 'phase-maintained-v5']:
    previous = root / 'build' / version
    assert read(previous / 'build/targeted-v1/exit.json')['exit_code'] == 1
    old = read(previous / 'build/sources.json')
    assert all(
        old[name] == expected
        for name, expected in frozen.items()
        if name.startswith(('src/', 'include/'))
    )
broad_directory = candidate / 'build/full-project-v1/build/full-campaign-v1'
broad = read(broad_directory / 'progress.json')
assert broad['status'] == 'running' and not (broad_directory / 'exit.json').exists()
assert read(candidate / 'build/full-project-v1/build/sources.json') == frozen
for row in broad['commands']:
    assert row['returncode'] == 0 and digest(broad_directory / row['log']) == row['log_sha256']
destination.mkdir()


def add_bytes(archive, name, data):
    member = tarfile.TarInfo(name)
    member.size = len(data)
    member.mode = 0o644
    archive.addfile(member, io.BytesIO(data))


def inputs_archive(path, versions):
    with tarfile.open(path, 'x:gz') as archive:
        for label, project, manifest in versions:
            prefix = label + '/' if label else ''
            for name, expected in manifest.items():
                assert digest(project / name) == expected
                add_bytes(archive, prefix + name, (project / name).read_bytes())
            add_bytes(
                archive,
                prefix + 'verified-sources.json',
                (json.dumps(manifest, indent=2) + '\n').encode(),
            )
            add_bytes(archive, prefix + 'LICENSE', (project / 'LICENSE').read_bytes())


inputs_archive(destination / 'sources.tar.gz', [('', candidate, frozen)])
older = [
    'phase-prototype-v1',
    'phase-prototype-v2',
    'phase-maintained-v3',
    'phase-maintained-v4',
    'phase-maintained-v5',
]
inputs_archive(
    destination / 'historical-sources.tar.gz',
    [
        (name, root / 'build' / name, read(root / 'build' / name / 'build/sources.json'))
        for name in older
    ],
)
suffixes = {'.py', '.json', '.c', '.inc', '.qasm', '.log', '.txt', '.sh', '.ast', '.stderr'}
folders = [
    ('current/' + name, candidate / 'build' / name)
    for name in [
        'targeted-v1',
        'phase-state-proof-normal-v1',
        'phase-state-proof-sanitized-v1',
        'harness-development-v1',
    ]
]
folders += [
    ('v2/' + name, v2 / 'build' / name)
    for name in [
        'phase-probes-v2',
        'phase-probes-sanitized-v2',
        'phase-schema-v1',
        'phase-state-proof-v1',
        'qualification-v1',
        'qualification-continuation-v2',
        'sanitizer-ast-v1',
    ]
]
folders += [
    (version + '/targeted-v1', root / 'build' / version / 'build/targeted-v1')
    for version in ['phase-maintained-v4', 'phase-maintained-v5']
]
with tarfile.open(destination / 'scoped-evidence.tar.gz', 'x:gz') as archive:
    for label, directory in folders:
        for path in sorted(directory.rglob('*')):
            if path.is_file() and not path.is_symlink() and path.suffix in suffixes:
                add_bytes(
                    archive, label + '/' + path.relative_to(directory).as_posix(), path.read_bytes()
                )
    for path in sorted((root / 'build/phase-maintained-v3/build').glob('prepar*')):
        if path.is_file():
            add_bytes(archive, 'v3-preparation/' + path.name, path.read_bytes())
    for row in broad['commands']:
        add_bytes(
            archive,
            'broad-closed-prefix/' + row['log'],
            (broad_directory / row['log']).read_bytes(),
        )
    for pattern in [
        '*phase*.py',
        'phase_capture_maintained.inc',
        'validate_phase_maintained.c',
        'coherent-cpu-trace.inc',
    ]:
        for path in sorted((root / 'build').glob(pattern)):
            if path.is_file():
                # Disjoint patterns avoid duplicate member names.
                add_bytes(archive, 'runners/' + path.name, path.read_bytes())
for source, name in [
    (candidate / 'build/targeted-v1/exit.json', 'targeted.json'),
    (candidate / 'build/phase-state-proof-normal-v1/exit.json', 'states-normal.json'),
    (candidate / 'build/phase-state-proof-sanitized-v1/exit.json', 'states-sanitized.json'),
    (v2 / 'build/phase-probes-sanitized-v2/results.json', 'v2-sanitizer-failures.json'),
]:
    shutil.copyfile(source, destination / name)
(destination / 'broad-campaign-at-archive.json').write_text(json.dumps(broad, indent=2) + '\n')
shutil.copyfile(Path(__file__), destination / 'archiver.py')
(destination / 'README.es.md').write_text(
    '''# Ángulos diferidos de phase: candidato v6

Estas 143 entradas congeladas todavía no están integradas. En normal y ASan/UBSan pasan 3380 comprobaciones por configuración: 1540 ejecuciones del runtime nativo, 1540 estados completos de programas compilados, 220 diagnósticos de dominio, 20 controles anidados y 60 comprobaciones de esquema y dominio perezoso. Los estados revisan las fases relativas y los cables físicos liberados, que deben quedar en cero.

Una prueba adicional conserva 400 capturas de circuitos con valores CPU reales por configuración y 400 ejecuciones C ordinarias que calculan los bits exactos de los ángulos con la misma optimización y modo de builtins. Esas referencias C no usan el plugin ni los sanitizadores. Ambas políticas de síntesis pasan 800 estados complejos independientes por configuración: 1600 en total. No son mediciones de hardware.

Se conservan los fallos anteriores. V2 pasa sus 240 filas normales, pero solo 70 de 240 instrumentadas: 170 compilaciones rechazan las comprobaciones de división que inserta GCC. V6 serializa la operación entera tipada y sustituye ese guard por las comprobaciones de dominio del backend, aplicadas únicamente al dominio seleccionado. V4 y V5 fallaron por el comprobador: primero supuso que no había cables físicos liberados; después intentó leer puertas sin activar su registro. El compilador se mantuvo idéntico entre V4, V5 y V6.

La primera prueba independiente de V2 terminó con error al intentar leer un cierre instrumentado todavía ausente; sus 800 estados normales no convierten ese proceso en una ejecución global exitosa. La primera regresión instrumentada quedó interrumpida; la continuación conservó su resultado real fallido. Todos esos registros permanecen separados.

La regresión amplia de V6 sigue en curso en la fotografía archivada y debe terminar antes de aplicar el código. Siguen pendientes las cargas CPU inválidas, long double, llamadas reales generales y C11 completo. La aritmética de estos ángulos es clásica; no implementa registros cuánticos float o double. La ciberseguridad permanece clásica.
'''
)
sys.path.insert(0, str(root / '.deps/format-venv/lib/python3.10/site-packages'))
spec = importlib.util.spec_from_file_location('phase_capsule_format', root / 'scripts/format.py')
formatter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(formatter)
for name in ['README.es.md', 'archiver.py']:
    path = destination / name
    text = formatter.format_text(path, path.read_text())
    assert formatter.format_text(path, text) == text
    path.write_text(text)
base = read(root / 'docs/audit-evidence/controlled-operands-integration/integration-summary.json')[
    'source_sha256'
]
summary = {
    'status': 'verified-scoped-phase-not-integrated',
    'source_files': 143,
    'source_sha256': frozen,
    'integrated_base_source_sha256': base,
    'checks_per_configuration': {
        'native': 1540,
        'compiled_complete_states': 1540,
        'domain_diagnostics': 220,
        'nested_controls': 20,
        'schema_and_lazy_domains': 60,
        'total': 3380,
    },
    'independent_states': states,
    'fresh_build_commands_closed_zero': 4,
    'core_sha256': {
        configuration: {
            name: digest(stage / 'build' / name)
            for name in [
                'cket',
                'cket_backend',
                'cket_plugin.so',
                'cket_runtime.o',
                'libcket_quantum.a',
                'validate_phase',
            ]
        }
        for configuration, stage in [
            ('normal', candidate),
            ('sanitized', candidate / 'build/sanitized-phase-v1'),
        ]
    },
    'history': {
        'v2_normal_passed': 240,
        'v2_sanitized_passed': 70,
        'v2_sanitized_failed': 170,
        'v2_original_state_runner_exit': 1,
        'v2_original_state_checks_before_error': 800,
        'v2_original_instrumented_runner_exit': None,
        'v2_instrumented_continuation_exit': 1,
        'v4_harness_exit': 1,
        'v5_harness_exit': 1,
    },
    'broad_campaign_at_archive': {
        'status': broad['status'],
        'closed_commands': len(broad['commands']),
        'source_files': 143,
    },
    'remaining': 'Broad regression and guarded main integration; invalid CPU loads, long double and arbitrary real library calls; complete C11. No quantum floating registers, hardware or speedup claim. Cybersecurity stays classical.',
    'artifact_sha256': {
        path.name: digest(path) for path in destination.iterdir() if path.is_file()
    },
}
(destination / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print(
    'Archived phase v6:3380 normal+3380 SAN;1600 independent states; earlier failures preserved; broad regression still open.'
)
