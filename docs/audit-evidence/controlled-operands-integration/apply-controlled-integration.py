"""Apply the verified138-input integration and prepare guarded Windows import."""
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

root = Path(__file__).resolve().parent.parent
out = root / 'build/controlled-integration-v1'
project = out / 'project'
destination = root / 'docs/audit-evidence/controlled-operands-integration'
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
write = lambda path, value: path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')
terminal = json.loads((out / 'exit.json').read_text())
assert terminal['status'] == 'passed-targeted-integration' and terminal['exit_code'] == 0
assert len(terminal['configurations']) == 2 and all(row['native_checks'] == 393 and row['controlled_operand_checks'] == 1270
    and row['exact_contract_executions'] == 40 for row in terminal['configurations'])
for row in terminal['commands']:
    assert row['returncode'] == 0 and digest(out / row['log']) == row['log_sha256']
frozen = terminal['source_sha256']
broad = json.loads((root / 'docs/audit-evidence/controlled-operands-full/summary.json').read_text())
assert broad['source_sha256'] == frozen and broad['exit_code'] == 0 and broad['closed_commands'] == 76
base = json.loads((root / 'docs/audit-evidence/coherent-memory-integration/integration-summary.json').read_text())
for name, expected in base['source_sha256'].items():
    assert digest(root / name) == expected
for name, expected in frozen.items():
    assert digest(project / name) == expected
changes = sorted(name for name in frozen if frozen[name] != base['source_sha256'].get(name))
assert len(changes) == 7 and len(frozen) == 138
assert set(frozen) - set(base['source_sha256']) == {'tests/validate_controlled_operands.c'}
before = json.loads((root / 'build/controlled-integration-windows-before.json').read_text())
for name, expected in before.items():
    if expected is None:
        assert not (root / name).exists(), name
    else:
        assert digest(root / name) == expected, name
with tarfile.open(out / 'main-before.tar', 'x') as archive:
    for name, expected in before.items():
        if expected is not None:
            archive.add(root / name, arcname=name)
write(out / 'application-before.json', before)
destination.mkdir()
for path in out.iterdir():
    if path.is_file() and path.suffix in ('.json', '.log', '.py', '.sh', '.md', '.c'):
        shutil.copyfile(path, destination / path.name)
for name in ('apply-controlled-integration.py', 'update-controlled-integration-docs.py', 'update-controlled-integration-paper.py'):
    shutil.copyfile(root / 'build' / name, destination / name)
summary = {'status': 'verified-targeted-and-applied', 'applied_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
    'source_files': 138, 'source_sha256': frozen,
    'previous_integration': '../coherent-memory-integration/integration-summary.json',
    'broad_campaign': {'status': 'passed-in-segments', 'evidence': '../controlled-operands-full/summary.json',
        'source_sha256': broad['source_sha256'], 'exit_code': 0, 'closed_commands': 76},
    'root_verification': {'status': 'passed-targeted', 'configurations': terminal['configurations'],
        'scope': terminal['scope']},
    'remaining': 'Deferred phase angles and inactive CPU loads remain bugs. General ABI, memory and complete C11 remain pending. No hardware/speedup claim. Cybersecurity stays classical and unchanged.',
    'artifact_sha256': {path.name: digest(path) for path in destination.iterdir() if path.is_file()}}
write(destination / 'integration-summary.json', summary)
for name in changes:
    (root / name).parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(project / name, root / name)
for name in ('cket', 'cket_backend', 'cket_plugin.so', 'cket_runtime.o', 'libcket_quantum.a', 'gcc-version.txt'):
    shutil.copyfile(project / 'build' / name, root / 'build' / name)
    (root / 'build' / name).chmod(0o755)
for helper in ('update-controlled-integration-docs.py', 'update-controlled-integration-paper.py'):
    spec = importlib.util.spec_from_file_location(helper[:-3], root / 'build' / helper)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.update(root)
for label, count, steps in (('coherent-control-full', 137, 74), ('controlled-operands-full', 138, 76)):
    (root / 'docs/audit-evidence' / label / 'README.es.md').write_text(f'''# Campaña completa de {count} entradas

El [resumen](summary.json) conserva {steps} comandos cerrados, reconstrucciones nuevas normales y
ASan/UBSan, todas las suites del flujo, 665 compilaciones de fuzzing por configuración y 70
circuitos independientes con QuEST por configuración: 64 estados y 6 canales explícitos.

El corpus de 133 índices, cinco niveles, 256 valores de byte y semilla 20261012 se ejecuta en
tramos con un controlador derivado. Se conservan generador, programas, referencia escalar,
registros originales e identidades de fuentes. Los resultados son un agregado explícito de
comandos, sin atribuirlos a una sola invocación de `scripts/verify.sh`.

Las fuentes de 137 y 138 entradas son campañas diferentes. Esta evidencia finita no completa C11
ni demuestra ausencia de bugs. `phase` y cargas CPU de ramas inactivas siguen pendientes. La
ciberseguridad permanece clásica.
''')
(destination / 'README.es.md').write_text('''# Integración de operandos controlados

El [resumen](integration-summary.json) identifica las 138 entradas aplicadas, la campaña amplia
completa previa y las reconstrucciones separadas de instalación. Pasaron, por configuración,
393 casos nativos, 1270 controles de operandos y 40 ejecuciones de cuatro programas exactos del
contrato, en cinco niveles de optimización y con builtins activados y desactivados.

Los 1270 controles separan 1000 ejecuciones nativas definidas, 200 diagnósticos de dominio,
60 diagnósticos de efectos y 10 ejecuciones de homónimos privados. Las 100 referencias C
ordinarias se registran aparte. Los 8000 estados de operandos y 1200 de XOR/bucles pertenecen
a la cápsula independiente de v5; su alcance y sus fallos de coordinación se conservan.

La aplicación de fuentes y herramientas normales a la copia Linux sigue a esas verificaciones.
La importación de fuentes a Windows comprueba hashes y conserva un respaldo; no constituye una
segunda ejecución de la regresión completa. La semántica del ángulo de `phase`, las cargas CPU
inactivas y C11 completo siguen pendientes. La ciberseguridad permanece clásica y sin cambios.
''')
sys.path.insert(0, str(root / '.deps/format-venv/lib/python3.10/site-packages'))
spec = importlib.util.spec_from_file_location('controlled_application_format', root / 'scripts/format.py')
formatter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(formatter)
documents = [name for name in before if name.endswith(('.md', '.py')) and name not in changes]
documents += [path.relative_to(root).as_posix() for folder in ('coherent-control-full', 'controlled-operands-full', 'controlled-operands-integration')
              for path in (root / 'docs/audit-evidence' / folder).glob('*.md')]
for name in documents:
    path = root / name
    formatted = formatter.format_text(path, path.read_text())
    assert formatter.format_text(path, formatted) == formatted
    path.write_text(formatted)
# Formatting must preserve the exact four compiled C programs, not only their behavior.
actual_examples = re.findall(r'^```c\n(.*?)^```', (root / 'docs/c11-controlled-operands.md').read_text(), re.M | re.S)
for index, program in enumerate(actual_examples):
    assert program == (out / (f'contract-{index}.c')).read_text(), ('formatted contract changed', index)
spec = importlib.util.spec_from_file_location('applied_controlled_sources', root / 'scripts/archive-audit.py')
source_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(source_module)
assert source_module.source_hashes() == frozen
commands = []
for argv in ([str(root / '.deps/format-venv/bin/python'), 'scripts/format.py', '--check'],
             [str(root / '.deps/paper-venv/bin/python'), 'docs/paper/audit_paper.py', '--language', 'all']):
    log = out / (f'application-{len(commands) + 1}.log')
    with log.open('x') as handle:
        result = subprocess.run(argv, cwd=root, env=dict(os.environ, CC='gcc-11', CXX='g++-11'), stdout=handle, stderr=subprocess.STDOUT)
    commands.append({'command': argv, 'returncode': result.returncode, 'log': log.name, 'log_sha256': digest(log)})
    write(out / 'application-commands.json', commands)
    assert result.returncode == 0, log.read_text()[-8000:]
for name in ('paper-audit.json', 'paper-audit-en.json'):
    path = root / 'docs/paper/evidence' / name
    report = json.loads(path.read_text())
    assert report['status'] == 'passed'
    report['audit_checkout']['execution_scope'] = 'Current138 inputs match the passed76-command broad normal/sanitizer campaign and a separate fresh installation that passed393 native,1270 controlled checks and40 exact contract executions per configuration. Historical137 CI/fuzz and operand failures retain their identities. Windows source application is separately hash-guarded and is not a second broad execution. Deferred phase angles, inactive CPU loads and complete C11 remain pending. Cybersecurity classical and unchanged.'
    write(path, report)
assert source_module.source_hashes() == frozen
files = sorted(set(before) | {path.relative_to(root).as_posix() for folder in ('coherent-control-full', 'controlled-operands-full', 'controlled-operands-integration')
    for path in (root / 'docs/audit-evidence' / folder).iterdir() if path.is_file()})
manifest = {'status': 'ready-for-guarded-source-import', 'before_sha256': before,
    'file_sha256': {name: digest(root / name) for name in files}, 'source_sha256': frozen, 'commands': commands}
write(out / 'application-manifest.json', manifest)
with tarfile.open(out / 'verified-application.tar', 'x') as archive:
    for name in files:
        data = (root / name).read_bytes()
        member = tarfile.TarInfo(name)
        member.size, member.mode = len(data), 0o644
        archive.addfile(member, io.BytesIO(data))
write(out / 'application-exit.json', {'exit_code': 0, 'files': len(files), 'archive_sha256': digest(out / 'verified-application.tar'),
    'manifest_sha256': digest(out / 'application-manifest.json')})
print('Prepared guarded138-source integration after complete76-command regression and fresh installation checks.')
