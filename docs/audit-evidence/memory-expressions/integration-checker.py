"""Archive or apply the single verified memset/sequence batch, without a 132-input hop."""
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
candidate = root / 'build/memory-sequence-prototype'
build = candidate / 'build'
destination = root / 'docs/audit-evidence/memory-expressions'
assert sys.argv[1:] in (['--archive'], ['--apply'])
base_path = root / 'docs/audit-evidence/byte-snapshots/integration-summary.json'
base = json.loads(base_path.read_text())
assert base['root_verification']['status'] == 'passed'
manifest = build / 'sequence-full-sources-before.json'
frozen = json.loads(manifest.read_text())
assert len(frozen) == 134
spec = importlib.util.spec_from_file_location('memory_batch_archiver', candidate / 'scripts/archive-audit.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
assert module.source_hashes() == frozen
module.ROOT = root
assert module.source_hashes() == base['source_sha256'], 'Main inputs changed; preserve user edits.'
changes = sorted(name for name in frozen.keys() | base['source_sha256'].keys()
                 if frozen.get(name) != base['source_sha256'].get(name))
assert changes == sorted([
    'include/string.h', 'scripts/archive-audit.py', 'scripts/build.sh', 'scripts/inventory_c11.py',
    'scripts/sanitize-all.sh', 'scripts/verify.sh', 'src/cket_plugin.cpp', 'src/quantum_frontend.inc',
    'src/quantum_memory.inc', 'src/quantum_semantics.inc', 'tests/memset_effects.inc',
    'tests/memset_state.inc', 'tests/validate_library.c', 'tests/validate_memset.c',
    'tests/validate_qasm.c', 'tests/validate_memory_sequences.c', 'tests/memory_sequence_state.inc',
]), changes
index = json.loads((candidate / 'docs/c11-library.json').read_text())['interfaces']
old_index = json.loads((root / 'docs/c11-library.json').read_text())['interfaces']
assert len(index) == len(old_index) == 1224
assert [(a['header'], a['name']) for a, b in zip(old_index, index) if a != b] == [
    ('string.h', 'memcpy'), ('string.h', 'memmove'), ('string.h', 'memset')]
assert {key: sum(row['quantum'] == key for row in index)
        for key in ('pending', 'partial', 'metadata-only')} == {
            'pending': 782, 'partial': 74, 'metadata-only': 368}
summary_path = destination / 'integration-summary.json'

if sys.argv[1:] == ['--apply']:
    summary = json.loads(summary_path.read_text())
    assert summary['status'] == 'verified-isolated' and summary['source_sha256'] == frozen
    for name, digest in summary['artifact_sha256'].items():
        assert hashlib.sha256((destination / name).read_bytes()).hexdigest() == digest, name
    new = sorted(set(frozen) - set(base['source_sha256']))
    assert new == sorted(['tests/memset_effects.inc', 'tests/memset_state.inc', 'tests/validate_memset.c',
                           'tests/validate_memory_sequences.c', 'tests/memory_sequence_state.inc'])
    for name in new + ['docs/c11-memset.md', 'docs/c11-memory-sequences.md']:
        assert not (root / name).exists(), name
    for name in changes + ['docs/c11-library.json', 'docs/c11-memset.md', 'docs/c11-memory-sequences.md']:
        (root / name).write_bytes((candidate / name).read_bytes())
    assert module.source_hashes() == frozen
    summary['status'] = 'verified-isolated-and-applied'
    summary['applied_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    summary['remaining'] = 'Separate fresh main normal/sanitizer targeted checks and current documentation/paper updates remain pending. Complete C11 remains open; cybersecurity stays classical.'
    summary_path.write_text(json.dumps(summary, indent=2) + '\n')
    print('Applied the combined 17-input delta directly from 129 to 134; all hashes match. Main checks and docs still required.')
    raise SystemExit(0)

terminal = json.loads((build / 'sequence-full-exit.json').read_text())
assert terminal['exit_code'] == 0 and terminal['error'] is None
segmented = terminal.get('mode') == 'passed-in-segments'
linux_copy = terminal.get('execution_mode') in ('linux-copy-after-second-interruption', 'linux-copy-with-resumable-fuzz-driver')
resumable_fuzz = terminal.get('execution_mode') == 'linux-copy-with-resumable-fuzz-driver'
resume_directory = terminal.get('resume_directory', 'linux-isolated-resume-v2' if resumable_fuzz else 'linux-isolated-resume-v1')
assert resume_directory in ('linux-isolated-resume-v1', 'linux-isolated-resume-v2', 'linux-isolated-resume-v3')
assert terminal['resume_returncode'] == 0 if segmented else terminal['verify_script_returncode'] == 0
job = json.loads((build / 'sequence-full-job.json').read_text())
assert job['source_manifest_sha256'] == hashlib.sha256(manifest.read_bytes()).hexdigest()
assert not destination.exists()
subprocess.run([sys.executable, str(candidate / 'scripts/archive-audit.py'),
                '--log', str(build / 'sequence-full-verify.log'), '--sources', str(manifest),
                '--destination', str(destination)], cwd=candidate, check=True)
result = json.loads((destination / 'results.json').read_text())
if segmented:
    result['status'] = 'passed-in-segments'
    result['scope'] += (' Original fresh full normal and sanitizer builds were interrupted during the sanitizer library suite. '
                        'The entire incomplete library suite and remaining tail resumed on identical inputs; '
                        'original exit status is unknown. Prefix, resume logs and metadata are retained separately.')
    if linux_copy:
        result['scope'] += (' A first continuation was also interrupted without a terminal status. '
                            'The successful tail ran in a hash-verified Linux /tmp copy without Windows drive mounts; '
                            'existing instrumented core artifacts were transferred, not rebuilt for this tail. '
                            'The validation tools were freshly rebuilt in the successful tail as recorded in its commands.')
    if resumable_fuzz:
        result['scope'] += (' A further Linux-copy interruption preserved 63 complete sanitizer fuzz cases. '
                            'An archived derived driver retained the exact frozen expression generator, program '
                            'and oracle and executed the remaining original case indices at the original seed. '
                            'The 665-compilation sanitizer fuzz result is a coverage-checked aggregate of '
                            '133 distinct cases at five GCC levels, each exercising all 256 byte inputs; '
                            'it is not an uninterrupted execution of the original fuzzer. See fuzz-coverage.json.')
    if resume_directory == 'linux-isolated-resume-v3':
        result['scope'] += (' A later restart interrupted the final sanitizer QASM suite after the fuzz '
                            'aggregate and growth checks had closed. Only that unfinished QASM suite '
                            'was rerun in v3; closed checks and all interrupted raw logs are retained.')
    (destination / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
assert result['status'] in ('passed', 'passed-in-segments') and result['source_sha256'] == frozen
assert result['counts_per_configuration']['memory_set_checks'] == 800
assert result['counts_per_configuration']['memory_sequence_checks'] == 360
assert result['counts_per_configuration']['external_qasm_states'] == 64
assert result['counts_per_configuration']['external_qasm_channels'] == 6
for source, name in [
    (manifest, 'verified-sources.json'), (build / 'sequence-full-job.json', 'job.json'),
    (build / 'sequence-full-exit.json', 'exit.json'),
    (root / 'build/launch-memory-sequence-full.py', 'runner.py'),
    (Path(__file__).resolve(), 'integration-checker.py'),
    (candidate / 'docs/c11-memset.md', 'memset-contract-draft.md'),
    (candidate / 'docs/c11-memory-sequences.md', 'sequence-contract-draft.md'),
    (candidate / 'docs/c11-library.json', 'inventory.json'),
]:
    shutil.copyfile(source, destination / name)
if segmented:
    for source, name in [
        (build / 'sequence-interrupted-prefix.log', 'interrupted-prefix.log'),
        (build / 'sequence-resume.log', 'resume.log'),
        (build / 'sequence-resume-job.json', 'resume-job.json'),
        (root / 'build/resume-memory-sequence-tail.py', 'resume-runner.py'),
        (root / 'build/resume-memory-sequence-tail.sh', 'resume-checker.sh')]:
        shutil.copyfile(source, destination / name)
    if linux_copy:
        for source, name in [
            (build / 'sequence-first-resume-interrupted.log', 'first-resume-interrupted.log'),
            (build / 'sequence-first-resume-job.json', 'first-resume-job.json'),
            (build / resume_directory / 'progress.json', 'linux-resume-results.json'),
            (root / 'build/run-isolated-memory-audit.py', 'linux-resume-runner.py'),
            (root / 'build/wsl-isolated-package.json', 'linux-transfer-inputs.json')]:
            shutil.copyfile(source, destination / name)
        with tarfile.open(destination / 'linux-resume-logs.tar.gz', 'x:gz') as archive:
            folders = ['linux-isolated-resume-v1', 'linux-isolated-resume-v2'] if resumable_fuzz else ['linux-isolated-resume-v1']
            if resume_directory == 'linux-isolated-resume-v3':
                folders.append(resume_directory)
            for folder in folders:
                for path in sorted((build / folder).iterdir()):
                    assert path.is_file() and not path.is_symlink()
                    archive.add(path, arcname=(folder + '/' if resumable_fuzz else '') + path.name)
        if resumable_fuzz:
            for source, name in [
                (build / 'linux-isolated-resume-v2/fuzz-coverage.json', 'fuzz-coverage.json'),
                (build / 'linux-isolated-resume-v2/resumable-fuzz-driver.c', 'resumable-fuzz-driver.c'),
                (root / 'build/resume-linux-memory-audit-v2.py', 'segmented-resume-runner.py')]:
                shutil.copyfile(source, destination / name)
            if resume_directory == 'linux-isolated-resume-v3':
                shutil.copyfile(root / 'build/resume-linux-memory-audit-v3.py', destination / 'final-resume-runner.py')
        # A missing terminal record remains unknown, not an invented exit zero.
        (destination / 'resume-exit.json').write_text(json.dumps({
            'original_exit_code': None, 'first_resume_exit_code': None,
            'successful_resume_exit_code': 0,
            'execution_mode': terminal['execution_mode'],
            'source': 'linux-resume-results.json',
        }, indent=2) + '\n')
    else:
        shutil.copyfile(build / 'sequence-resume-exit.json', destination / 'resume-exit.json')
with tarfile.open(destination / 'sources.tar.gz', 'x:gz') as archive:
    for name, data in [(name, (candidate / name).read_bytes()) for name in frozen] + [
        ('LICENSE', (candidate / 'LICENSE').read_bytes()), ('verified-sources.json', manifest.read_bytes()),
    ]:
        assert name not in frozen or hashlib.sha256(data).hexdigest() == frozen[name]
        member = tarfile.TarInfo(name)
        member.size = len(data)
        member.mode = 0o755 if name == 'cket' or name.endswith('.sh') else 0o644
        archive.addfile(member, io.BytesIO(data))
with tarfile.open(destination / 'earlier-scoped-evidence.tar.gz', 'x:gz') as archive:
    for owner, label in [
        (build, 'sequence-normal-evidence'), (build, 'sequence-sanitized-evidence'),
        (root / 'build/memset-prototype/build', 'validator-v1-evidence'),
        (root / 'build/memset-prototype/build', 'validator-v2-evidence'),
        (root / 'build/memset-prototype/build', 'alias-effects-v1/evidence'),
    ]:
        directory = owner / label
        report = json.loads((directory / 'results.json').read_text())
        for name, digest in report['artifact_sha256'].items():
            assert hashlib.sha256((directory / name).read_bytes()).hexdigest() == digest, (label, name)
        for path in sorted(directory.iterdir()):
            if path.is_file():
                archive.add(path, arcname=label + '/' + path.name)
summary = {
    'status': 'verified-isolated', 'archived_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
    'base_evidence': '../byte-snapshots/integration-summary.json',
    'source_files': 134, 'source_sha256': frozen, 'changed_inputs_from_base': changes,
    'scope': job['scope'] + (' Completed in segments after the confirmed interrupted sanitizer library suite; original exit code unknown and resumed tail exit zero. Original fresh core builds retained, source hashes checked again before/after resume.' if segmented else '') + (' Earlier continuations also stopped without terminal records. Successful completion used a verified Linux-only copy, transferred existing instrumented core tools and freshly rebuilt validation tools; see linux-resume-results.json and linux-transfer-inputs.json.' if linux_copy else '') + (' Sanitizer fuzzing completed across a preserved 63-case prefix and a derived resumable driver with unchanged generator/program/oracle logic. All original 133 indices, five levels and 256 byte inputs are checked explicitly in fuzz-coverage.json. Extra harness sources are archived separately from the 134 unchanged repository inputs.' if resumable_fuzz else ''),
    'checks_per_configuration': result['counts_per_configuration'],
    'library_index': {'interfaces': 1224, 'pending': 782, 'partial': 74, 'metadata-only': 368,
                      'partial_functions': 27, 'partial_types': 47},
    'earlier_evidence_scope': 'Original scoped normal/sanitizer and exploratory capsules retain their own older source identities and finite proof scopes. Draft contracts precede integration; the new full report supplies the completed broad campaign.',
    'remaining': 'Main application, separate targeted integration checks and current documentation updates remain pending. General quantum C11 remains incomplete; cybersecurity stays classical.',
    'artifact_sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                        for p in destination.iterdir() if p.is_file()},
}
summary_path.write_text(json.dumps(summary, indent=2) + '\n')
print('Archived the single fresh full normal/ASan/UBSan memory batch: 134 inputs, 800 memset/360 sequence checks and 70 QuEST circuits per configuration. No intermediate 132-input promotion required.')
