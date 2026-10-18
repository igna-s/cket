"""Archive a successful segmented memset campaign against its exact base."""
from pathlib import Path
import datetime
import hashlib
import importlib.util
import io
import json
import shutil
import subprocess
import sys
import tarfile

root = Path(__file__).resolve().parent.parent
candidate = root / 'build/memset-prototype'
build = candidate / 'build'
destination = root / 'docs/audit-evidence/memset'
assert not destination.exists()
terminal = json.loads((build / 'memset-resume-exit.json').read_text())
assert terminal['exit_code'] == terminal['runner_returncode'] == 0 and terminal['error'] is None
job = json.loads((build / 'memset-resume-job.json').read_text())
manifest = build / 'memset-full-sources-before.json'
assert hashlib.sha256(manifest.read_bytes()).hexdigest() == job['source_manifest_sha256']
prefix = (build / 'memset-interrupted-prefix.log').read_bytes()
assert hashlib.sha256(prefix).hexdigest() == job['interrupted_prefix_sha256']
assert prefix == (build / 'memset-full-verify.log').read_bytes()
tail = (build / 'memset-resume.log').read_bytes()
assert tail.rstrip().endswith(b'All compiler, runtime, example, synthesis and independent QASM checks passed.')
frozen = json.loads(manifest.read_text())
spec = importlib.util.spec_from_file_location('memset_completed_archiver', candidate / 'scripts/archive-audit.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
assert len(frozen) == 132 and module.source_hashes() == frozen
base = json.loads((root / 'docs/audit-evidence/byte-snapshots/integration-summary.json').read_text())
assert base['root_verification']['status'] == 'passed'
module.ROOT = root
assert module.source_hashes() == base['source_sha256'], 'Preserve changed main inputs.'
changes = sorted(name for name in frozen.keys() | base['source_sha256'].keys()
                 if frozen.get(name) != base['source_sha256'].get(name))
assert changes == sorted(['include/string.h', 'scripts/archive-audit.py', 'scripts/build.sh',
                          'scripts/inventory_c11.py', 'scripts/sanitize-all.sh', 'scripts/verify.sh',
                          'src/cket_plugin.cpp', 'src/quantum_frontend.inc', 'src/quantum_memory.inc',
                          'src/quantum_semantics.inc', 'tests/memset_effects.inc', 'tests/memset_state.inc',
                          'tests/validate_library.c', 'tests/validate_memset.c', 'tests/validate_qasm.c']), changes
old = json.loads((root / 'docs/c11-library.json').read_text())['interfaces']
new = json.loads((candidate / 'docs/c11-library.json').read_text())['interfaces']
assert len(old) == len(new) == 1224
assert [(a['header'], a['name']) for a, b in zip(old, new) if a != b] == [('string.h', 'memset')]
assert {key: sum(row['quantum'] == key for row in new) for key in ('pending', 'partial', 'metadata-only')} == {'pending': 782, 'partial': 74, 'metadata-only': 368}
merged = build / 'memset-completed-segments.log'
with merged.open('xb') as handle:
    handle.write(prefix)
    handle.write(b'\nSEGMENT BOUNDARY: interrupted during sanitizer examples; matching 132 frozen inputs; full incomplete example suite and remaining sanitizer tail resumed below.\n')
    handle.write(tail)
subprocess.run([sys.executable, str(candidate / 'scripts/archive-audit.py'), '--log', str(merged),
                '--sources', str(manifest), '--destination', str(destination)], cwd=candidate, check=True)
result_path = destination / 'results.json'
result = json.loads(result_path.read_text())
assert result['status'] == 'passed' and result['source_sha256'] == frozen
result['status'] = 'passed-in-segments'
result['command'] = ['Fresh CC=gcc-11 CXX=g++-11 CKET_FUZZ_CASES=128 CKET_FUZZ_SEED=20261012 bash scripts/verify.sh; interrupted during sanitizer examples.',
                     'bash build/resume-memset-tail.sh; full incomplete sanitizer examples and remaining tail on identical sources.']
result['scope'] = job['scope']
result_path.write_text(json.dumps(result, indent=2) + '\n')
for source, name in [
    (candidate / 'docs/c11-memset.md', 'contract-before-integration.md'),
    (candidate / 'docs/c11-library.json', 'inventory.json'),
    (manifest, 'verified-sources.json'),
    (build / 'memset-full-job.json', 'original-job.json'),
    (build / 'memset-resume-job.json', 'resume-job.json'),
    (build / 'memset-resume-exit.json', 'resume-exit.json'),
    (build / 'memset-interrupted-prefix.log', 'interrupted-prefix.log'),
    (build / 'memset-resume.log', 'resumed-tail.log'),
    (root / 'build/launch-memset-full.py', 'original-launcher.py'),
    (root / 'build/resume-memset-tail.py', 'resume-launcher.py'),
    (root / 'build/resume-memset-tail.sh', 'resume-checker.sh'),
    (Path(__file__).resolve(), 'archiver.py'),
]:
    shutil.copyfile(source, destination / name)
with tarfile.open(destination / 'sources.tar.gz', 'x:gz') as archive:
    for name, data in [(name, (candidate / name).read_bytes()) for name in frozen] + [
        ('LICENSE', (candidate / 'LICENSE').read_bytes()), ('verified-sources.json', manifest.read_bytes()),
    ]:
        assert name not in frozen or hashlib.sha256(data).hexdigest() == frozen[name]
        member = tarfile.TarInfo(name)
        member.size = len(data)
        member.mode = 0o755 if name == 'cket' or name.endswith('.sh') else 0o644
        archive.addfile(member, io.BytesIO(data))
with tarfile.open(destination / 'exploratory-evidence.tar.gz', 'x:gz') as archive:
    for label in ('validator-v1-evidence', 'validator-v2-evidence', 'alias-effects-v1/evidence'):
        directory = build / label
        report = json.loads((directory / 'results.json').read_text())
        for name, digest in report['artifact_sha256'].items():
            assert hashlib.sha256((directory / name).read_bytes()).hexdigest() == digest, (label, name)
        for path in sorted(directory.iterdir()):
            if path.is_file():
                archive.add(path, arcname=label + '/' + path.name)
summary = {
    'status': 'verified-isolated', 'archived_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
    'base_evidence': '../byte-snapshots/integration-summary.json',
    'source_files': 132, 'source_sha256': frozen, 'changed_inputs_from_base': changes,
    'checks_per_configuration': result['counts_per_configuration'], 'scope': job['scope'],
    'library_index': {'interfaces': 1224, 'pending': 782, 'partial': 74, 'metadata-only': 368,
                      'partial_functions': 27, 'partial_types': 47},
    'remaining': 'Main application, fresh main normal/sanitizer rebuilds and targeted checks still pending. Quantum return-pointer ABI, general memory/counts/layouts/control flow and the complete C11 objective remain open.',
    'user_scope': 'Cybersecurity stays classical and is excluded from quantum adaptation.',
    'artifact_sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                        for p in destination.iterdir() if p.is_file()},
}
(destination / 'integration-summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print('Archived successful segmented normal/ASan/UBSan memset campaign and 132 frozen inputs. No main application inferred.')
