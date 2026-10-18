"""Archive closed scoped operand proofs, their source snapshots and failed versions."""
from pathlib import Path
import hashlib
import io
import json
import os
import resource
import shutil
import subprocess
import tarfile

root = Path('/tmp/quantum-audit-isolated-20261017')
destination = root / 'docs/audit-evidence/controlled-operands'
destination.mkdir()
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
owners = {name: root / 'build' / folder for name, folder in (
    ('v1', 'coherent-body-prototype'), ('v2', 'coherent-body-prototype-v2'),
    ('v3', 'coherent-body-prototype-v3'), ('v4', 'coherent-body-prototype-v4'))}
latest = owners['v4']
native = json.loads((latest / 'build/native-matrix-v2/exit.json').read_text())
assert native['exit_code'] == 0 and native['status'] == 'passed-scoped-native-matrix'
assert all(row['defined_native_executions'] == 1000 and row['undefined_domain_diagnostics'] == 200 and
           row['compiler_rows'] == 200 and row['failed_compiler_rows'] == 0 for row in native['configurations'])
states = {}
for configuration, folder in (('normal', 'states-v1'), ('sanitized', 'states-sanitized-v1')):
    directory = latest / 'build' / folder
    report = json.loads((directory / 'exit.json').read_text())
    assert report['exit_code'] == 0 and report['complete_state_checks'] == 4000 and report['captures'] == 1000
    assert report['frontend_configuration'] == configuration and report['maximum_physical_wires'] == 34
    for row in report['checks']:
        assert digest(directory / row['qasm']) == row['qasm_sha256']
    states[configuration] = {'checks': 4000, 'captures': 1000, 'maximum_physical_wires': 34}
effects = json.loads((latest / 'build/effect-contracts-v1/results.json').read_text())
assert effects['status'] == 'passed-scoped-effect-contracts'
assert all(row['diagnostic_checks'] == 60 and row['native_homonym_checks'] == 10 and row['failed_checks'] == 0
           for row in effects['configurations'])
versions = {}
for label, owner in owners.items():
    frozen = json.loads((owner / 'build/sources.json').read_text())
    assert len(frozen) == 137
    for name, expected in frozen.items():
        assert digest(owner / name) == expected, (label, name)
    versions[label] = {'source_files': 137, 'source_sha256': frozen}
    with tarfile.open(destination / ('sources-' + label + '.tar.gz'), 'x:gz') as archive:
        for name in [*frozen, 'LICENSE', 'verified-sources.json']:
            data = (json.dumps(frozen, indent=2) + '\n').encode() if name == 'verified-sources.json' else (owner / name).read_bytes()
            member = tarfile.TarInfo(name)
            member.size, member.mode = len(data), 0o644
            archive.addfile(member, io.BytesIO(data))
    with tarfile.open(destination / ('sources-' + label + '.tar.gz')) as archive:
        for name, expected in frozen.items():
            assert hashlib.sha256(archive.extractfile(name).read()).hexdigest() == expected
assert native['source_sha256'] == versions['v4']['source_sha256'] == effects['source_sha256']
first = json.loads((owners['v1'] / 'build/operand-probes-v1/results.json').read_text())
versions['v1'].update(status='not-promotable', reason='Four inactive integer updates still report an erroneous undefined-domain failure; phase still faults.',
    exploratory_results=first)
second = json.loads((owners['v2'] / 'build/native-matrix-v1/exit.json').read_text())
assert second['exit_code'] == 1
versions['v2'].update(status='not-promotable', reason='Normal matrix retains ten abs-without-builtins regressions. All sanitizer attempts stopped at missing GCC metadata before plugin loading.',
    configurations=second['configurations'])
third = json.loads((owners['v3'] / 'build/native-matrix-v2/exit.json').read_text())
assert third['exit_code'] == 1 and third['configurations'][0]['failed_compiler_rows'] == 0
assert third['configurations'][1]['failed_compiler_rows'] == 80
versions['v3'].update(status='not-promotable', reason='Normal values and 4000 independent normal states pass; eighty sanitizer compilations still reject actual UBSan SAVE_EXPR/abs trees.',
    configurations=third['configurations'], normal_complex_state_checks=4000)
versions['v4'].update(status='verified-scoped', native_configurations=native['configurations'], states=states,
    effect_checks_per_configuration=60, internal_homonym_native_checks_per_configuration=10,
    ordinary_c_executions_per_configuration=100)

pending = latest / 'build/pending-after-v4'
pending.mkdir()
pending_rows = []
header = '#include <quantum.h>\n'
cases = {
    'phase-angle': "int k=argc>1?argv[1][0]-'0':0;quant bool b=0;quant uint8_t x=5;if(b){phase(10/k);}unsigned r=measure(x),v=measure(b);return r!=5||v;",
    'invalid-cpu-read': 'int *p=0;quant bool b=0;quant uint8_t x=5;if(b){if(x>*p)phase(.17);}unsigned r=measure(x),v=measure(b);return r!=5||v;',
}
for name, body in cases.items():
    source = pending / (name + '.c')
    source.write_text(header + 'int main(int argc,char**argv){(void)argc;(void)argv;' + body + '}\n')
    ordinary = pending / (name + '-reference.c')
    ordinary.write_text('#include <stdint.h>\n#include <stdbool.h>\n#define quant\n#define measure(x) (x)\n#define phase(x) ((void)(x))\n' + source.read_text().replace(header, ''))
    binary = pending / (name + '-reference')
    result = subprocess.run(['gcc-11', '-std=c11', '-O2', str(ordinary), '-o', str(binary)], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    result = subprocess.run([str(binary), '0'], capture_output=True, text=True)
    assert result.returncode == 0
    row = {'name': name, 'ordinary_c_returncode': result.returncode, 'compilers': []}
    for label, owner in (('integrated137', root), ('prototype-v4', latest)):
        binary = pending / (name + '-' + label)
        result = subprocess.run([str(owner / 'build/cket'), str(source), '-O2', '-fbuiltin', '-o', str(binary)],
            cwd=owner, env=dict(os.environ, CC='gcc-11'), capture_output=True, text=True)
        item = {'compiler': label, 'compile_returncode': result.returncode, 'compile_stderr': result.stderr}
        assert result.returncode == 0, item
        result = subprocess.run([str(binary), '0'], capture_output=True, text=True,
            preexec_fn=lambda: resource.setrlimit(resource.RLIMIT_CORE, (0, 0)))
        item.update(run_returncode=result.returncode, run_stderr=result.stderr)
        assert result.returncode == (-8 if name == 'phase-angle' else -11), item
        row['compilers'].append(item)
    pending_rows.append(row)
(pending / 'results.json').write_text(json.dumps(pending_rows, indent=2) + '\n')
shutil.copyfile(pending / 'results.json', destination / 'pending-after-v4.json')
accepted_suffixes = {'.c', '.inc', '.py', '.json', '.log', '.qasm', '.ast', '.sh', '.txt'}
with tarfile.open(destination / 'scoped-evidence.tar.gz', 'x:gz') as archive:
    for label, owner in owners.items():
        for path in sorted((owner / 'build').rglob('*')):
            if path.is_file() and not path.is_symlink() and path.suffix in accepted_suffixes:
                archive.add(path, arcname=label + '/' + path.relative_to(owner / 'build').as_posix())
    for path in sorted((root / 'build').glob('*.py')):
        if any(word in path.name for word in ('coherent-body', 'controlled-operand', 'body-ubsan', 'body-abs', 'controlled-pure')):
            archive.add(path, arcname='runners/' + path.name)
shutil.copyfile(Path(__file__), destination / 'archiver.py')
summary = {'status': 'verified-scoped-with-preserved-failures', 'versions': versions,
    'base_integrated_evidence': '../coherent-memory-integration/integration-summary.json',
    'scope': 'V4 keeps controlled pure integer arithmetic inside typed reversible expressions, guards operand definedness as well as gate application, and preserves standard abs identity under both builtin modes. Actual UBSan handlers remain distinct from real CPU effects. Two thousand native defined executions and eight thousand complete sparse complex states/inverses are separate scopes; source snapshots identify every prototype. This is not a completed broad campaign or main application.',
    'remaining': 'The published/integrated source tree still has seven body-operand failures until a later guarded integration. V4 scoped proofs fix the six integer-operand profiles, but phase-angle and inactive invalid CPU-memory reads still fail in both main and v4 at O2 with builtins. General C11, full ABI and physical hardware claims remain unproven. Cybersecurity unchanged.',
    'artifact_sha256': {path.name: digest(path) for path in destination.iterdir() if path.is_file()}}
(destination / 'experimental-summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print('Archived four 137-input prototypes: v4 1000 normal +1000 sanitizer defined executions, 8000 independent states/inverses, real pending phase/NULL failures preserved.')
