"""Apply the separately rebuilt finite integration; preserve pending failures."""
from pathlib import Path
import datetime
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

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / 'build/coherent-integration-v1'
PROJECT = OUT / 'project'
DEST = ROOT / 'docs/audit-evidence/coherent-memory-integration'
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')


def replace_once(name, before, after):
    path = ROOT / name
    text = path.read_text()
    assert text.count(before) == 1, (name, before)
    path.write_text(text.replace(before, after))


terminal = json.loads((OUT / 'exit.json').read_text())
assert terminal['status'] == 'passed-targeted-integration' and terminal['exit_code'] == 0
assert len(terminal['configurations']) == 2
assert all(row['native_checks'] == 393 and row['memory_return_checks'] == 290 and
           row['pointer_difference_checks'] == 170 and row['cpu_condition_native_executions'] == 180 and
           row['cpu_effect_diagnostics'] == 30 for row in terminal['configurations'])
for row in terminal['commands']:
    assert row['returncode'] == 0 and digest(OUT / row['log']) == row['log_sha256']
frozen = terminal['source_sha256']
base = json.loads((ROOT / 'docs/audit-evidence/memory-expressions/integration-summary.json').read_text())
before_sources = base['source_sha256']
for name, expected in before_sources.items():
    assert digest(ROOT / name) == expected, name
for name, expected in frozen.items():
    assert digest(PROJECT / name) == expected, name
changes = sorted(name for name in frozen if frozen[name] != before_sources.get(name))
assert len(changes) == 17 and len(frozen) == 137
new = set(frozen) - set(before_sources)
assert new == {'tests/memory_return_state.inc', 'tests/validate_memory_returns.c', 'tests/validate_pointer_differences.c'}
for name in new:
    assert not (ROOT / name).exists(), name
doc_names = ['README.md', 'docs/c-coverage.md', 'docs/c11-roadmap.md', 'docs/c11-memcopy.md',
             'docs/c11-memset.md', 'docs/c11-memory-sequences.md', 'docs/c11-library.json',
             'docs/c11-inventory.md', 'docs/estado-general-del-proyecto.es.md', 'docs/repository-audit.md',
             'docs/paper/audit_paper.py', 'docs/paper/evidence/paper-audit.json',
             'docs/paper/evidence/paper-audit-en.json', 'docs/paper/evidence/sha256-manifest.json',
             'docs/audit-evidence/coherent-cpu-conditions/README.es.md']
before = {name: digest(ROOT / name) if (ROOT / name).exists() else None
          for name in [*changes, *doc_names, 'docs/c11-memory-returns.md']}
write_json(OUT / 'application-before.json', before)
with tarfile.open(OUT / 'main-before.tar', 'x') as archive:
    for name, expected in before.items():
        if expected is not None:
            archive.add(ROOT / name, arcname=name)

example_rows = []
examples = re.findall(r'^```c\n(.*?)^```', (ROOT / 'build/coherent-memory-contract-draft.md').read_text(), re.M | re.S)
assert len(examples) == 3
for index, program in enumerate(examples):
    source = OUT / f'contract-{index}.c'
    source.write_text(program)
    for configuration in ('normal', 'sanitized'):
        stage = PROJECT if configuration == 'normal' else PROJECT / 'build/sanitized'
        options = dict(os.environ, CC='gcc-11' if configuration == 'normal' else str(stage / 'build/gcc-wrapper'),
                       CKET_REAL_GCC='gcc-11', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        for level in ('-O0', '-O1', '-O2', '-O3', '-Os'):
            for builtin in ('-fbuiltin', '-fno-builtin'):
                binary = OUT / f'contract-{index}-{configuration}-{level[1:]}-{builtin[2:]}'
                result = subprocess.run([str(stage / 'build/cket'), str(source), level, builtin, '-o', str(binary)],
                                        cwd=PROJECT, env=options, capture_output=True, text=True)
                row = {'example': index, 'configuration': configuration, 'optimization': level,
                       'builtins': builtin, 'compile_returncode': result.returncode, 'compile_stderr': result.stderr}
                assert result.returncode == 0, row
                result = subprocess.run([str(binary), '0'], env=options, capture_output=True, text=True)
                row.update(run_returncode=result.returncode, run_stderr=result.stderr)
                assert result.returncode == 0, row
                assert not any(marker in row['compile_stderr'] + result.stderr for marker in
                               ('AddressSanitizer:', 'runtime error:', 'internal compiler error:')), row
                example_rows.append(row)
                write_json(OUT / 'contract-examples.json', example_rows)
assert len(example_rows) == 60

DEST.mkdir()
for path in OUT.iterdir():
    if path.is_file() and path.suffix in ('.json', '.log', '.py', '.sh'):
        shutil.copyfile(path, DEST / path.name)
shutil.copyfile(Path(__file__), DEST / 'application-runner.py')
with tarfile.open(DEST / 'cpu-matrices.tar.gz', 'x:gz') as archive:
    for variant in ('normal', 'sanitized'):
        directory = OUT / (variant + '-cpu-matrix')
        for name in ('progress.json', 'results.json'):
            archive.add(directory / name, arcname=variant + '/' + name)
    for path in sorted((ROOT / 'build/coherent-cpu-branch-v3/build/native-v2').glob('*.c')):
        archive.add(path, arcname='fixtures/' + path.name)
with tarfile.open(DEST / 'sources.tar.gz', 'x:gz') as archive:
    for name in [*frozen, 'LICENSE']:
        data = (PROJECT / name).read_bytes()
        member = tarfile.TarInfo(name)
        member.size, member.mode = len(data), 0o644
        archive.addfile(member, io.BytesIO(data))
for name in changes:
    (ROOT / name).write_bytes((PROJECT / name).read_bytes())
spec = importlib.util.spec_from_file_location('applied_source_identity', ROOT / 'scripts/archive-audit.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
assert module.source_hashes() == frozen
for owner, target in ((PROJECT / 'build', ROOT / 'build'),
                       (PROJECT / 'build/sanitized/build', ROOT / 'build/sanitized/build')):
    target.mkdir(parents=True, exist_ok=True)
    for path in owner.iterdir():
        if path.is_file() and path.name not in ('maintained-sources.json',):
            shutil.copyfile(path, target / path.name)
            (target / path.name).chmod(path.stat().st_mode & 0o777)
summary = {'status': 'verified-targeted-and-applied',
           'applied_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
           'source_files': 137, 'source_sha256': frozen, 'changed_inputs_from_main134': changes,
           'base_evidence': '../memory-expressions/integration-summary.json',
           'root_verification': {'status': 'passed-targeted', 'configurations': terminal['configurations'],
                                 'execution_scope': terminal['scope']},
           'broad_campaign': {'status': 'pending', 'candidate': 'coherent-cpu-maintained',
                              'source_identity': 'Same 137 source hashes; separate ongoing normal/sanitizer campaign.'},
           'known_failures': ['../coherent-arm-operands/failure-summary.json',
                              '../coherent-cpu-conditions/experimental-summary.json'],
           'remaining': 'Seven inactive controlled-body operand failures and inactive invalid CPU-memory reads remain open. '
                        'Broad regression remains pending. General pointer ABI and complete C11 are incomplete. Cybersecurity unchanged.',
           'artifact_sha256': {path.name: digest(path) for path in DEST.iterdir() if path.is_file()}}
write_json(DEST / 'integration-summary.json', summary)
env = dict(os.environ, CC='gcc-11', CXX='g++-11')
spec = importlib.util.spec_from_file_location('current_inventory_contracts', ROOT / 'scripts/inventory_c11.py')
inventory_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(inventory_module)
index_path = ROOT / 'docs/c11-library.json'
index_data = json.loads(index_path.read_text())
changed_entries = []
for item in index_data['interfaces']:
    classification, classes, milestone, tests, contract = inventory_module.semantics(
        item['header'], item['name'], item['kind'], item['profile'] == 'annex-k-optional')
    replacement = dict(item, quantum=classification, restriction_classes=classes,
                       milestone=milestone, tests=tests, contract=contract)
    if replacement != item:
        changed_entries.append((item['header'], item['name']))
        item.update(replacement)
assert changed_entries == [('string.h', 'memcpy'), ('string.h', 'memmove'), ('string.h', 'memset')], changed_entries
write_json(index_path, index_data)
subprocess.run(['python3', 'scripts/inventory_c11.py'], cwd=ROOT, env=env, check=True)
inventory = json.loads((ROOT / 'docs/c11-library.json').read_text())['interfaces']
assert {key: sum(row['quantum'] == key for row in inventory) for key in
        ('pending', 'partial', 'metadata-only')} == {'pending': 782, 'partial': 74, 'metadata-only': 368}

contract = (ROOT / 'build/coherent-memory-contract-draft.md').read_text()
contract = contract.replace('Este contrato describe el candidato de 137 entradas. Su aplicación al árbol principal y las\npruebas de integración se registran por separado. La regresión amplia sigue en ejecución.',
    'El adaptador parcial está integrado sobre 137 entradas. El [registro de integración](audit-evidence/coherent-memory-integration/integration-summary.json)\nidentifica las reconstrucciones normales e instrumentadas y sus pruebas específicas. La regresión amplia sigue en ejecución.')
contract += '\nLos [ocho contrastes posteriores](audit-evidence/coherent-arm-operands/failure-summary.json)\nreprodujeron siete fallos aún abiertos en operandos de asignación, suma, XOR, `memset`, `abs` y\n`phase` bajo control inactivo. El octavo caso, la condición anidada, pasó con esta corrección.\nSe probaron a O2 con builtins; no se presenta esa exploración como una matriz de todos los niveles.\n'
(ROOT / 'docs/c11-memory-returns.md').write_text(contract)
replace_once('README.md', 'General returned pointers, runtime\ncounts, dynamic allocation and general layouts remain pending.',
    '[Local checked destination returns](docs/c11-memory-returns.md), constant pointer distances and\npure CPU predicates inside coherent branches are supported. General returned-pointer consumers,\nruntime counts, dynamic allocation and general layouts remain pending.')
replace_once('README.md', 'General byte layouts, unknown overwritten values, dynamic extents and returned-pointer uses remain\npending.',
    'Local checked return borrows preserve destination identity. General byte layouts, unknown\noverwritten values, dynamic extents and unrestricted returned-pointer consumers remain pending.')
replace_once('README.md', 'Validation includes 391 native checks;', 'Validation includes 393 native checks;')
replace_once('docs/c-coverage.md', 'General layouts, runtime extents and returned-pointer consumers remain P/R.',
    'Local checked destination returns and same-object distances are supported. General layouts, runtime extents and unrestricted returned-pointer consumers remain P/R. [Returns](c11-memory-returns.md).')
replace_once('docs/c11-roadmap.md', 'returned-pointer uses remain pending.',
    'unrestricted returned-pointer consumers remain pending. [Local checked destination returns and same-object distances](c11-memory-returns.md) have partial adapters.')
replace_once('docs/c11-memcopy.md', 'boundaries and returned-pointer uses have negative cases; classical return pointers and libc\nbehavior have positive cases.',
    'boundaries and unsupported pointer escapes have negative cases; local checked return borrows,\nclassical return pointers and libc behavior have positive cases. The separate\n[return/distance contract](c11-memory-returns.md) records its own finite tests.')
replace_once('docs/c11-memset.md', 'The returned pointer for quantum destinations does not yet have a checked return-pointer ABI. The\nimplemented calls discard that result; CPU initialization or other unsupported consumers diagnose\nthe quantum marker. Quantum indirect-call and separately compiled library ABIs also remain pending.',
    '[Local checked destination-return borrows](c11-memory-returns.md) now support compatible typed\npointers, void pointers, constant byte views and supported address relations. General pointer\nconsumers and escapes, quantum indirect calls and separately compiled library ABIs remain pending.')
replace_once('docs/c11-memory-sequences.md', 'possible unspecified-order expression. The general checked return-pointer ABI is still pending: the\nmemory call\'s pointer result is discarded here, not converted to a usable CPU pointer.',
    'possible unspecified-order expression. These discarded calls do not consume their pointer result.\n[Local destination-return borrows](c11-memory-returns.md) have a separate partial contract; general\npointer consumers and the general return-pointer ABI remain pending.')

path = ROOT / 'docs/estado-general-del-proyecto.es.md'
text = path.read_text()
start = text.index('**Hallazgo posterior a las regresiones anteriores:**')
end = text.index('\n## Qué significa', start)
text = text[:start] + '''**La división anticipada en condiciones anidadas ya tiene una corrección integrada.** Las
reconstrucciones nuevas normales y con ASan/UBSan pasaron, por configuración, 393 pruebas nativas,
290 de retornos de memoria, 170 de distancias, 180 ejecuciones de condiciones y 30 diagnósticos.
La [integración](audit-evidence/coherent-memory-integration/integration-summary.json) conserva las
137 entradas exactas y distingue esos controles de la regresión amplia, que sigue en ejecución.
Los 800 estados e inversas independientes pertenecen a la evidencia separada de la corrección.

**Siguen existiendo bugs confirmados:** ocho contrastes posteriores reprodujeron siete fallos de
operandos CPU dentro de ramas cuánticas inactivas: asignación, suma, XOR, `memset`, `abs` y `phase`.
El [registro](audit-evidence/coherent-arm-operands/failure-summary.json) conserva los programas,
sus referencias C exitosas y las señales de división por cero de los ejecutables aceptados a O2
con builtins. El octavo caso, la condición anidada, sí pasó. También queda pendiente la lectura
mediante un puntero CPU inválido en una rama inactiva. La corrección tiene un alcance concreto;
las regresiones anteriores no demuestran ausencia de todos los bugs.
''' + text[end:]
text = text.replace('Quedan punteros devueltos, tamaños dinámicos y layouts generales.',
                    'Hay retornos locales comprobados; quedan consumidores generales, tamaños dinámicos y layouts generales.')
start = text.index('El retorno de punteros de memoria tiene otro')
end = text.index('\n## Qué falta implementar', start)
text = text[:start] + '''Los retornos locales comprobados de `memcpy`, `memmove` y `memset`, las relaciones admitidas
entre direcciones y las distancias al mismo objeto ya forman parte de la integración de 137
entradas. Los [contratos](c11-memory-returns.md) describen los tipos, límites y consumidores
admitidos. Las suites recurrentes pasaron 290 y 170 comprobaciones por configuración en una
reconstrucción nueva separada. Los primeros fallos de expectativas antiguas se conservaron en su
[registro histórico](audit-evidence/memory-returns-regression-failure/failure-summary.json).
Las campañas anteriores de 136 y 137 entradas siguen separadas por sus fuentes. La regresión amplia
de la corrección actual continúa pendiente; los consumidores generales de punteros y la ABI general
siguen incompletos.
''' + text[end:]
text = text.replace('La [integración actual de memoria](audit-evidence/memory-expressions/integration-summary.json)\nidentifica 134 entradas y separa la regresión completa del candidato de las pruebas del árbol\nprincipal.',
    'La [integración actual](audit-evidence/coherent-memory-integration/integration-summary.json)\nidentifica 137 entradas y pruebas específicas reconstruidas en normal y ASan/UBSan. Su regresión\namplia sigue pendiente. La [campaña anterior de 134 entradas](audit-evidence/memory-expressions/integration-summary.json)\nconserva su regresión completa y sus comprobaciones de integración originales.')
start = text.index('La resta entre referencias comprobadas al mismo objeto tiene otro')
end = text.index('\nLa continuación que terminó', start)
text = text[:start] + '''La [evidencia histórica de distancias](audit-evidence/pointer-differences/README.es.md) conserva
sus fuentes y resultados originales. La integración posterior añade el uso positivo de una
distancia guardada en una variable clásica como condición bajo control cuántico, sin medición
implícita. Ese antiguo rechazo no se presenta como una restricción actual ni como prueba de que
la compatibilidad general de punteros esté terminada.
''' + text[end:]
path.write_text(text)
path = ROOT / 'docs/repository-audit.md'
text = path.read_text()
text = text.replace('# Auditoría del repositorio y de los papers\n', '''# Auditoría del repositorio y de los papers

La [integración del 18 de octubre](c11-memory-returns.md) añade retornos locales de memoria,
distancias comprobadas y una corrección de condiciones CPU dentro de control cuántico. Sus 137
entradas pasaron reconstrucciones normales/ASan/UBSan y controles específicos; la regresión amplia
sigue pendiente. Hay [siete fallos posteriores reproducidos](audit-evidence/coherent-arm-operands/failure-summary.json)
en operandos bajo control inactivo y un caso pendiente de dirección CPU inválida. El inventario
sigue incompleto y la ciberseguridad permanece clásica.
''', 1)
path.write_text(text)
path = ROOT / 'docs/audit-evidence/coherent-cpu-conditions/README.es.md'
text = path.read_text()
text = text.replace('**Hay un bug confirmado en el compilador principal. La corrección sigue en un candidato separado.**',
    '**Esta cápsula conserva el bug del árbol anterior de 134 entradas y sus prototipos.**\nLa [corrección posterior](../coherent-memory-integration/integration-summary.json) ya se integró\nsobre 137 entradas con reconstrucciones y pruebas específicas. Su regresión amplia sigue pendiente.')
text = text.replace('**El árbol principal todavía reproduce el bug de división\ninactiva descrito arriba.**',
    'El fallo descrito arriba corresponde a las 134 entradas históricas.\nLa integración posterior conserva una prueba positiva del caso corregido.')
text = text.replace('La regresión amplia del candidato corregido está en curso. Después faltan su integración y las\ncomprobaciones de esa integración.',
    'La regresión amplia del candidato corregido está en curso. Las pruebas específicas de\nintegración ya terminaron y están registradas por separado.')
text += '\nLos [contrastes posteriores](../coherent-arm-operands/failure-summary.json) reproducen siete\nfallos todavía abiertos en operandos del cuerpo de una rama cuántica inactiva. La corrección\nde condiciones no se presenta como una solución de todos los operandos ni de toda la memoria.\n'
path.write_text(text)

path = ROOT / 'docs/paper/audit_paper.py'
text = path.read_text()
text = text.replace("(ROOT / 'docs/audit-evidence/memory-expressions/integration-summary.json').read_text()\n    )\n    check(\n        'Current compiler:",
    "(ROOT / 'docs/audit-evidence/coherent-memory-integration/integration-summary.json').read_text()\n    )\n    check(\n        'Current compiler:")
text = text.replace("current_integration['root_verification']['status'] == 'passed'\n        and current_integration['source_files'] == len(current_integration['source_sha256']) == 134",
    "current_integration['root_verification']['status'] == 'passed-targeted'\n        and current_integration['broad_campaign']['status'] == 'pending'\n        and current_integration['source_files'] == len(current_integration['source_sha256']) == 137")
text = text.replace("'Current audit: the reproduced inactive-branch bug remains disclosed as unresolved in main'",
                    "'Current audit: historical inactive-condition bug and later positive integration remain distinct'")
text = text.replace("and reproduced['source_sha256'] == current_integration['source_sha256']",
                    "and reproduced['source_files'] == 134\n        and reproduced['source_sha256'] != current_integration['source_sha256']")
text = text.replace("and 'el compilador principal todavía tiene un bug' in re.sub(r'\\s+', ' ', overview)",
    "and all(row['cpu_condition_native_executions'] == 180 for row in current_integration['root_verification']['configurations'])\n        and 'corrección integrada' in re.sub(r'\\s+', ' ', overview)")
text = text.replace("and coherent_maintained['source_sha256'] != current_integration['source_sha256']",
                    "and coherent_maintained['source_sha256'] == current_integration['source_sha256']")
text = text.replace('Candidate audit: scoped coherent-control evidence is separate from current C11 compatibility',
                    'Integrated audit: scoped coherent-control evidence does not imply complete C11 compatibility')
marker = '    targets = [\n'
assert text.count(marker) == 1
text = text.replace(marker, '''    arm_failures = json.loads(
        (ROOT / 'docs/audit-evidence/coherent-arm-operands/results.json').read_text()
    )['rows']
    check(
        'Current audit: seven controlled-body failures remain disclosed despite the condition fix',
        len(arm_failures) == 8
        and sum(row['corrected137'].get('run_returncode') == -8 for row in arm_failures) == 7
        and all(row['ordinary_c_returncode'] == 0 for row in arm_failures)
        and 'siete fallos' in re.sub(r'\\s+', ' ', overview),
    )
''' + marker)
marker = "                'coherent-cpu-conditions-maintained',\n"
assert text.count(marker) == 1
text = text.replace(marker, marker + "                'coherent-memory-integration',\n                'coherent-arm-operands',\n")
path.write_text(text)

sys.path.insert(0, str(ROOT / '.deps/format-venv/lib/python3.10/site-packages'))
spec = importlib.util.spec_from_file_location('integration_document_format', ROOT / 'scripts/format.py')
formatter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(formatter)
for name in [*doc_names, 'docs/c11-memory-returns.md']:
    if name.endswith(('.md', '.py')):
        path = ROOT / name
        formatted = formatter.format_text(path, path.read_text())
        assert formatter.format_text(path, formatted) == formatted, name
        path.write_text(formatted)
commands = []
for argv in ([str(ROOT / '.deps/format-venv/bin/python'), 'scripts/format.py', '--check'],
             [str(ROOT / '.deps/paper-venv/bin/python'), 'docs/paper/audit_paper.py', '--language', 'all'],
             ['git', 'diff', '--check']):
    log = OUT / (f'application-{len(commands) + 1}.log')
    with log.open('x') as handle:
        result = subprocess.run(argv, cwd=ROOT, env=env, stdout=handle, stderr=subprocess.STDOUT)
    commands.append({'command': argv, 'returncode': result.returncode, 'log': log.name, 'log_sha256': digest(log)})
    write_json(OUT / 'application-commands.json', commands)
    assert result.returncode == 0, (commands[-1], log.read_text()[-5000:])
for name in ('paper-audit.json', 'paper-audit-en.json'):
    path = ROOT / 'docs/paper/evidence' / name
    data = json.loads(path.read_text())
    assert data['status'] == 'passed' and data['checks'] == 352
    data['audit_checkout']['execution_scope'] = (
        'Current main has 137 source inputs. Fresh normal and ASan/UBSan builds in a separate '
        'source-identical integration copy passed targeted suites and the 180-execution CPU-condition '
        'matrix per configuration. Broad regression is still pending. Seven controlled-body operand '
        'failures and invalid inactive CPU-memory reads remain disclosed. Historical papers and '
        '134-input bug evidence keep their original identities. Windows source import is separately '
        'hash-guarded; it is not a second regression execution. Cybersecurity unchanged.')
    write_json(path, data)
assert module.source_hashes() == frozen
all_files = sorted(set([*before, *[path.relative_to(ROOT).as_posix() for directory in
    (DEST, ROOT / 'docs/audit-evidence/coherent-arm-operands') for path in directory.iterdir() if path.is_file()]]))
package = {'status': 'ready-for-guarded-source-import', 'before_sha256': before,
           'file_sha256': {name: digest(ROOT / name) for name in all_files},
           'source_sha256': frozen, 'commands': commands,
           'scope': 'Targeted integration passed, main Linux source hashes match, broad campaign pending, seven operand failures remain open.'}
write_json(OUT / 'application-manifest.json', package)
with tarfile.open(OUT / 'verified-application.tar', 'x') as archive:
    for name in all_files:
        data = (ROOT / name).read_bytes()
        member = tarfile.TarInfo(name)
        member.size, member.mode = len(data), 0o644
        archive.addfile(member, io.BytesIO(data))
write_json(OUT / 'application-exit.json', {'exit_code': 0, 'files': len(all_files),
            'archive_sha256': digest(OUT / 'verified-application.tar'),
            'manifest_sha256': digest(OUT / 'application-manifest.json')})
print('Prepared guarded application of 137 inputs; targeted checks passed, both352-check papers passed, broad regression remains pending.')
