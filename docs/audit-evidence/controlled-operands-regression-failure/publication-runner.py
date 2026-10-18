"""Preserve the failed broad candidate and publish its finite successful scopes."""
from pathlib import Path
import hashlib
import importlib.util
import io
import json
import os
import shutil
import subprocess
import sys
import tarfile

root = Path(__file__).resolve().parent.parent
out = root / 'build/controlled-status-publication-v1'
out.mkdir()
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
write = lambda path, value: path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')
before = json.loads((root / 'build/controlled-docs-before.json').read_text())
for name, expected in before.items():
    assert digest(root / name) == expected, name
integration = json.loads((root / 'docs/audit-evidence/coherent-memory-integration/integration-summary.json').read_text())
for name, expected in integration['source_sha256'].items():
    assert digest(root / name) == expected, name
evidence = root / 'docs/audit-evidence'
experimental = evidence / 'controlled-operands'
maintained = evidence / 'controlled-operands-maintained'
failed = evidence / 'controlled-operands-regression-failure'
failed.mkdir()
candidate = root / 'build/coherent-body-maintained'
campaign = candidate / 'build/full-campaign-v1'
progress = json.loads((campaign / 'progress.json').read_text())
terminal = json.loads((campaign / 'exit.json').read_text())
frozen = json.loads((candidate / 'build/maintained-sources.json').read_text())
assert terminal['exit_code'] == 1 and terminal['status'] == 'failed-broad-campaign'
assert len(progress['commands']) == 2 and [row['returncode'] for row in progress['commands']] == [0, 1]
for name, expected in frozen.items():
    assert digest(candidate / name) == expected, name
for row in progress['commands']:
    assert digest(campaign / row['log']) == row['log_sha256']
native_log = (campaign / '002-normal-native.log').read_text()
assert '391 passed, 2 failed' in native_log
for path in campaign.iterdir():
    if path.is_file():
        shutil.copyfile(path, failed / path.name)
for name in ('run-controlled-full.py', 'start-controlled-full.py'):
    shutil.copyfile(root / 'build' / name, failed / name)
fixtures = {
    'controlled-boolean-xor.c': '#include <quantum.h>\nint main(void) { quant bool a=superposition(0,1),b=false; if(a)b^=true; unsigned flag=measure(a); return measure(b)!=flag; }\n',
    'controlled-static-loop.c': '#include <quantum.h>\nint main(void) { quant bool c=1; quant uint8_t x=0; for(int i=0;i<3;++i)if(c)x+=i+1; int a=measure(x),b=measure(c); return a!=6||b!=1; }\n',
}
for name, text in fixtures.items():
    (failed / name).write_text(text)
shutil.copyfile(Path(__file__), failed / 'publication-runner.py')
failure = {
    'status': 'failed-broad-candidate-not-integrated', 'exit_code': 1,
    'source_files': len(frozen), 'source_sha256': frozen,
    'source_archive': '../controlled-operands-maintained/sources.tar.gz',
    'commands': progress['commands'], 'native_passed': 391, 'native_failed': 2,
    'failures': [
        {'fixture': 'controlled-boolean-xor.c', 'test': 'GCC boolean complement preserves a controlled XOR',
         'observed': 'Compilation rejects measurement of the control because a live computed value remains.'},
        {'fixture': 'controlled-static-loop.c', 'test': 'static for coherent condition',
         'observed': 'Compilation succeeds; execution exceeds the native simulator limit of 24 qubits.'},
    ],
    'scope': 'Fresh normal build passed; the first native suite failed. No later broad suite or sanitizer build ran in this campaign. The separate 1270-check normal/sanitizer scopes remain valid for their recorded tools and sources, but do not qualify promotion.',
    'remaining': 'Fix both regressions, rerun broad validation, perform guarded integration, resolve phase-angle and inactive invalid CPU reads, and complete C11. Cybersecurity unchanged.',
    'artifact_sha256': {path.name: digest(path) for path in failed.iterdir() if path.is_file()},
}
write(failed / 'failure-summary.json', failure)
(experimental / 'README.es.md').write_text('''# Operand​​os CPU bajo control cuántico: pruebas separadas

Esta cápsula conserva cuatro prototipos y sus fallos, con 137 entradas congeladas por versión.
La versión v4 pasa 1000 ejecuciones nativas definidas y 200 diagnósticos de división fuera del
dominio por configuración, normal y ASan/UBSan. Hay 100 ejecuciones C ordinarias separadas por
configuración. Además pasan 60 diagnósticos de efectos CPU y 10 ejecuciones de funciones privadas
homónimas por configuración. Los diagnósticos son rechazos comprobados; no cuentan como soporte.

La prueba independiente comprueba 4000 estados complejos e inversas por frontend, 8000 en total,
a partir de capturas reales de sus enlaces CPU. Usa exportaciones de dos políticas de síntesis
y simulación dispersa independiente, con un máximo de 34 cables físicos. Estas capturas no
ejecutan el motor cuántico nativo. El límite de este último sigue en 24 qubits; no se afirma una
ejecución nativa densa de 34 qubits ni una ejecución en hardware cuántico físico.

Las versiones anteriores conservan sus fallos de compilación, metadatos instrumentados y
semántica. La v4 resuelve los seis perfiles de operandos enteros de su matriz; `phase` y una
lectura CPU inválida en una rama inactiva siguen fallando, como muestra `pending-after-v4.json`.

El [resumen](experimental-summary.json) vincula fuentes, registros y circuitos por SHA-256.
El [candidato recurrente](../controlled-operands-maintained/maintained-summary.json) añade una
suite C, pero su [regresión amplia falló](../controlled-operands-regression-failure/failure-summary.json).
**La corrección nueva no está integrada.** La ciberseguridad permanece clásica.
'''.replace('Operand​​os', 'Operandos'))
(maintained / 'README.es.md').write_text('''# Suite recurrente de operandos controlados

El [resumen](maintained-summary.json) conserva 138 entradas exactas y una suite nueva de C.
Pasan 1270 comprobaciones en normal y otras 1270 con ASan/UBSan: 1000 ejecuciones nativas
definidas, 200 diagnósticos de división fuera del dominio, 60 diagnósticos de efectos CPU y 10
ejecuciones de funciones privadas homónimas por configuración. Las 100 referencias C ordinarias
por configuración se registran aparte. Se prueban cinco niveles de optimización y ambos modos
de builtins. La suite comprueba correlaciones medidas; la prueba de fases completas pertenece a
la [cápsula experimental](../controlled-operands/experimental-summary.json).

Los validadores C se compilaron de nuevo. Estas pruebas específicas reutilizaron herramientas
normales e instrumentadas de v4 cuyos hashes quedaron registrados; no se presentan como dos
reconstrucciones nuevas de todo el compilador. La preparación conserva también un fallo del
script de inserción y su recuperación antes de ejecutar la suite.

La [regresión amplia posterior](../controlled-operands-regression-failure/failure-summary.json)
sí reconstruyó el compilador normal, pero se detuvo en la primera suite nativa con 391 casos
exitosos y dos fallos. No llegó a reconstruir el compilador instrumentado de esa campaña.
**El candidato no está integrado.** Faltan esos dos casos, la regresión completa, la integración
y los pendientes de `phase`, cargas CPU inválidas y C11. La ciberseguridad permanece clásica.
''')
(failed / 'README.es.md').write_text('''# Dos regresiones del candidato de operandos controlados

El [registro terminal](failure-summary.json) conserva la reconstrucción normal exitosa y la
suite nativa posterior: 391 casos pasan y dos fallan. El XOR booleano controlado se rechaza
por una dependencia computada viva al medir su control; el bucle estático supera el límite
nativo de 24 qubits. El registro íntegro conserva los programas exactos y los diagnósticos.
Los dos archivos C aquí publicados transcriben los cuerpos de esos casos para su inspección.

Las 138 entradas del candidato están identificadas por hash y disponibles en la
[cápsula de fuentes](../controlled-operands-maintained/sources.tar.gz). La campaña no ejecutó
las suites posteriores ni una reconstrucción instrumentada. Las pruebas específicas anteriores
siguen siendo evidencia de sus casos concretos y no demuestran una regresión completa exitosa.
El candidato no fue aplicado al compilador integrado. La ciberseguridad permanece clásica.
''')
path = root / 'docs/estado-general-del-proyecto.es.md'
text = path.read_text()
marker = '## Qué significa que una operación cuántica esté implementada\n'
assert text.count(marker) == 1
text = text.replace(marker, '''**Hay una corrección nueva en un candidato separado; no está integrada.** Pasaron 1270
comprobaciones normales y otras 1270 con ASan/UBSan, además de 8000 comprobaciones independientes
de estados complejos e inversas. Los [resultados específicos](audit-evidence/controlled-operands-maintained/README.es.md)
separan 1000 ejecuciones nativas definidas, 260 diagnósticos y 10 ejecuciones de homónimos por
configuración; las referencias C se registran aparte. El candidato corrige seis perfiles enteros
de su matriz, pero su [regresión amplia](audit-evidence/controlled-operands-regression-failure/README.es.md)
encontró **dos regresiones**: un XOR booleano controlado se rechaza y un bucle estático supera el
límite nativo de 24 qubits. La primera suite nativa terminó con 391 casos exitosos y dos fallos.
Falta corregirlos, completar la regresión e integrar el cambio. `phase`, las cargas CPU inválidas
y los contratos generales de C11 siguen pendientes. Las comprobaciones específicas no sustituyen
la regresión amplia.

''' + marker)
text = text.replace('El próximo bloque de memoria es completar y verificar los consumidores de punteros devueltos, además\nde los contratos generales de tamaños, layouts y efectos. Después continúa el trabajo por familias\npendientes.', 'El próximo paso concreto es corregir las dos regresiones del candidato de operandos controlados,\ncompletar sus pruebas amplias e integrar el cambio. También quedan `phase`, las cargas CPU inválidas,\nlos consumidores generales de punteros devueltos y los contratos de tamaños, layouts y efectos.\nDespués continúa el trabajo por las demás familias pendientes.')
path.write_text(text)
path = root / 'docs/paper/audit_paper.py'
text = path.read_text()
marker = '    targets = [\n'
assert text.count(marker) == 1
text = text.replace(marker, '''    controlled = json.loads((ROOT / 'docs/audit-evidence/controlled-operands/experimental-summary.json').read_text())
    controlled_v4 = controlled['versions']['v4']
    controlled_maintained = json.loads((ROOT / 'docs/audit-evidence/controlled-operands-maintained/maintained-summary.json').read_text())
    controlled_failure = json.loads((ROOT / 'docs/audit-evidence/controlled-operands-regression-failure/failure-summary.json').read_text())
    check(
        'Candidate operands: finite native and complete-state scopes remain distinct from main',
        controlled_v4['status'] == 'verified-scoped'
        and all(row['defined_native_executions'] == 1000 and row['undefined_domain_diagnostics'] == 200 for row in controlled_v4['native_configurations'])
        and sum(row['checks'] for row in controlled_v4['states'].values()) == 8000
        and controlled_v4['source_sha256'] != current_integration['source_sha256'],
    )
    check(
        'Candidate operands: maintained C suite does not certify full compatibility',
        controlled_maintained['status'] == 'verified-scoped-maintained'
        and controlled_maintained['source_files'] == 138
        and sum(value for key, value in controlled_maintained['checks_per_configuration'].items() if key != 'ordinary_c_executions_separate') == 1270
        and all(controlled_maintained['source_sha256'][name] == controlled_v4['source_sha256'][name] for name in ('src/quantum_frontend.inc', 'src/quantum_semantics.inc')),
    )
    check(
        'Candidate operands: two actual broad failures prevent promotion',
        controlled_failure['status'] == 'failed-broad-candidate-not-integrated'
        and controlled_failure['exit_code'] == 1
        and controlled_failure['source_sha256'] == controlled_maintained['source_sha256']
        and controlled_failure['native_passed'] == 391 and controlled_failure['native_failed'] == 2
        and [row['returncode'] for row in controlled_failure['commands']] == [0, 1]
        and 'dos regresiones' in overview and 'no está integrada' in overview,
    )
''' + marker)
marker = "                'coherent-arm-operands',\n"
assert text.count(marker) == 1
text = text.replace(marker, marker + "                'controlled-operands',\n                'controlled-operands-maintained',\n                'controlled-operands-regression-failure',\n")
path.write_text(text)
sys.path.insert(0, str(root / '.deps/format-venv/lib/python3.10/site-packages'))
spec = importlib.util.spec_from_file_location('controlled_status_formatter', root / 'scripts/format.py')
formatter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(formatter)
doc_names = ['docs/estado-general-del-proyecto.es.md', 'docs/paper/audit_paper.py'] + [path.relative_to(root).as_posix() for folder in (experimental, maintained, failed) for path in folder.glob('*.md')]
for name in doc_names:
    path = root / name
    formatted = formatter.format_text(path, path.read_text())
    assert formatter.format_text(path, formatted) == formatted, name
    path.write_text(formatted)
commands = []
for argv in ([str(root / '.deps/format-venv/bin/python'), 'scripts/format.py', '--check'], [str(root / '.deps/paper-venv/bin/python'), 'docs/paper/audit_paper.py', '--language', 'all']):
    log = out / (str(len(commands) + 1) + '.log')
    with log.open('x') as handle:
        result = subprocess.run(argv, cwd=root, env=dict(os.environ, CC='gcc-11', CXX='g++-11'), stdout=handle, stderr=subprocess.STDOUT)
    commands.append({'command': argv, 'returncode': result.returncode, 'log': log.name, 'log_sha256': digest(log)})
    write(out / 'commands.json', commands)
    assert result.returncode == 0, log.read_text()[-8000:]
for name in ('paper-audit.json', 'paper-audit-en.json'):
    path = root / 'docs/paper/evidence' / name
    report = json.loads(path.read_text())
    assert report['status'] == 'passed'
    report['audit_checkout']['execution_scope'] = 'Integrated137 targeted checks remain separate from candidate138. The controlled-operand candidate passes1270 scoped checks per normal/sanitizer configuration and8000 independent states/inverses, but its fresh broad normal native suite fails391/2. No candidate promotion or broad pass. Historical source identities and paper experiments preserved. Windows import is hash-guarded, not a repeated test execution. Cybersecurity classical and unchanged.'
    write(path, report)
for name, expected in integration['source_sha256'].items():
    assert digest(root / name) == expected, name
files = sorted(set(before) | {path.relative_to(root).as_posix() for folder in (experimental, maintained, failed) for path in folder.iterdir() if path.is_file()})
manifest = {'status': 'ready-for-guarded-document-import', 'before_sha256': before,
            'file_sha256': {name: digest(root / name) for name in files},
            'source_sha256': integration['source_sha256'], 'commands': commands}
write(out / 'manifest.json', manifest)
with tarfile.open(out / 'documents.tar', 'x') as archive:
    for name in files:
        data = (root / name).read_bytes()
        member = tarfile.TarInfo(name)
        member.size, member.mode = len(data), 0o644
        archive.addfile(member, io.BytesIO(data))
write(out / 'exit.json', {'exit_code': 0, 'archive_sha256': digest(out / 'documents.tar'), 'manifest_sha256': digest(out / 'manifest.json'), 'files': len(files)})
print('Published scoped operand evidence and both broad failures without changing137 integrated sources.')
