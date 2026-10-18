from pathlib import Path
import hashlib
import io
import json
import shutil
import tarfile

root=Path(__file__).resolve().parent.parent
candidate=root/'build/coherent-cpu-maintained'
checks=candidate/'build/maintained-checks-v1'
destination=root/'docs/audit-evidence/coherent-cpu-conditions-maintained'
destination.mkdir()
terminal=json.loads((checks/'exit.json').read_text())
frozen=json.loads((candidate/'build/maintained-sources.json').read_text())
assert terminal['exit_code']==0 and terminal['source_sha256']==frozen and len(frozen)==137
digest=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
for name,expected in frozen.items():
    assert digest(candidate/name)==expected,name
for row in terminal['configurations']:
    assert row['checks']==170 and row['native_checks']==90 and row['diagnostic_checks']==40
    assert row['action_and_inverse_circuits']==40 and row['ordinary_c_executions_separate']==45
    assert digest(checks/(row['configuration']+'.log'))==row['log_sha256']
for path in checks.iterdir():
    if path.is_file():
        shutil.copyfile(path,destination/path.name)
for name in ('prepare-coherent-maintained.py','start-coherent-maintained-checks.py',
             'run-coherent-maintained-checks.py','archive-coherent-maintained.py'):
    shutil.copyfile(root/'build'/name,destination/name)
shutil.copyfile(candidate/'build/maintained-sources.json',destination/'verified-sources.json')
with tarfile.open(destination/'sources.tar.gz','x:gz') as archive:
    for name in [*frozen,'LICENSE','verified-sources.json']:
        data=(json.dumps(frozen,indent=2)+'\n').encode() if name=='verified-sources.json' else (candidate/name).read_bytes()
        member=tarfile.TarInfo(name)
        member.size,member.mode=len(data),0o644
        archive.addfile(member,io.BytesIO(data))
with tarfile.open(destination/'sources.tar.gz') as archive:
    for name,expected in frozen.items():
        assert hashlib.sha256(archive.extractfile(name).read()).hexdigest()==expected,name
summary={'status':'verified-scoped-maintained','source_files':137,'source_sha256':frozen,
    'base_experiment':'../coherent-cpu-conditions/experimental-summary.json',
    'checks_per_configuration':{'native':90,'diagnostic':40,'action_and_inverse_circuits':40,
        'state_comparisons_inside_circuits':80,'ordinary_c_executions_separate':45},
    'scope':terminal['scope'],
    'remaining':'Fresh broad regression and main application are in progress. Invalid conditional CPU-memory accesses, general ABI and complete C11 remain pending. Cybersecurity stays classical and unchanged.',
    'artifact_sha256':{path.name:digest(path) for path in destination.iterdir() if path.is_file()}}
(destination/'maintained-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
print('Archived170 normal +170 sanitizer recurrent checks on137 source inputs; no main application claim.')
