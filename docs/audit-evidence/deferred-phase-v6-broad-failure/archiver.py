"""Preserve the actual failed broad run without rewriting its earlier snapshot."""
from pathlib import Path
import hashlib
import importlib.util
import json
import shutil
import sys
import tarfile

root=Path('/tmp/quantum-audit-isolated-20261017')
project=root/'build/phase-maintained-v6/build/full-project-v1'
campaign=project/'build/full-campaign-v1'
out=root/'docs/audit-evidence/deferred-phase-v6-broad-failure'
out.mkdir()
digest=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
terminal=json.loads((campaign/'exit.json').read_text())
progress=json.loads((campaign/'progress.json').read_text())
frozen=json.loads((project/'build/sources.json').read_text())
assert terminal['status']=='failed-broad-campaign' and terminal['exit_code']==1
commands=progress['commands']
assert len(commands)==57 and all(row.get('returncode')==0 for row in commands[:-1])
assert commands[-1]['name']=='sanitized-bindings' and commands[-1]['returncode']==1
assert all(digest(project/name)==expected for name,expected in frozen.items())
phase=json.loads((root/'docs/audit-evidence/deferred-phase-v6/summary.json').read_text())
assert frozen==phase['source_sha256']
for name in ['exit.json','progress.json']:
    shutil.copyfile(campaign/name,out/name)
shutil.copyfile(campaign/commands[-1]['log'],out/'sanitized-bindings.log')
with tarfile.open(out/'transcript.tar.gz','x:gz') as archive:
    for row in commands:
        path=campaign/row['log'];assert digest(path)==row['log_sha256']
        archive.add(path,arcname=path.name,recursive=False)
    for name in ['worker.log','job.json','exit.json','progress.json']:
        archive.add(campaign/name,arcname=name,recursive=False)
    for path in [root/'build/run-phase-v6-full.py',project/'build/sources.json']:
        archive.add(path,arcname=path.name,recursive=False)
shutil.copyfile(Path(__file__),out/'archiver.py')
(out/'README.es.md').write_text('''# Regresión amplia de fases v6: ejecución fallida conservada

La campaña terminó con salida 1. Cerró 57 comandos: 56 con salida cero y el
último, `sanitized-bindings`, con salida 1. No es una regresión amplia aprobada.

La misma prueba que ya existía para `n ? 1 / d : false`, con `n` falso y `d`
cero, fue rechazada como efecto CPU al compilar con sanitizadores. La revisión
identificó que el control de volatilidad se aplicaba antes de reconocer el
wrapper de GCC. La corrección está en pruebas en un candidato posterior.

Se conservan los registros completos de los comandos cerrados, el controlador,
la salida final y los hashes de las 143 entradas. Son las mismas fuentes de
`deferred-phase-v6/sources.tar.gz`. La fotografía anterior que decía «en curso»
permanece intacta como registro histórico; este cierre agrega su resultado real.

Los 3380 controles dirigidos y los estados independientes anteriores mantienen
su alcance, pero no justifican integrar este candidato frente a una regresión
confirmada. El árbol principal sigue en 138 entradas. La ciberseguridad no cambia.
''',encoding='utf-8')
sys.path.insert(0,str(root/'.deps/format-venv/lib/python3.10/site-packages'))
spec=importlib.util.spec_from_file_location('phase_failure_format',root/'scripts/format.py')
formatter=importlib.util.module_from_spec(spec);spec.loader.exec_module(formatter)
path=out/'README.es.md';text=formatter.format_text(path,path.read_text())
assert formatter.format_text(path,text)==text;path.write_text(text)
summary={'status':'failed-broad-campaign','exit_code':1,'closed_commands':57,
         'closed_zero_commands':56,'failed_command':'sanitized-bindings','source_files':143,
         'source_sha256':frozen,'source_archive':'../deferred-phase-v6/sources.tar.gz',
         'source_archive_sha256':digest(root/'docs/audit-evidence/deferred-phase-v6/sources.tar.gz'),
         'artifact_sha256':{path.name:digest(path) for path in sorted(out.iterdir())}}
(out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
with tarfile.open(root/'build/phase-v6-broad-failure-publication.tar','x') as archive:
    for path in sorted(out.iterdir()):archive.add(path,arcname=path.relative_to(root).as_posix(),recursive=False)
print('Preserved failed broad run:',len(commands),'commands,',len(summary['artifact_sha256'])+1,'public files')
