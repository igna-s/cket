"""Publish closed targeted evidence and apply the exact canonical sources in Linux."""

from pathlib import Path
import datetime
import hashlib
import json
import shutil
import tarfile

root = Path('/tmp/quantum-audit-isolated-20261017')
semantic = root / 'build/string-family-v5'
candidate = root / 'build/string-integration-v6'
out = root / 'docs/audit-evidence/strings-integration-v6'
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
read = lambda path: json.loads(path.read_text())
write = lambda path, value: path.write_text(json.dumps(value, indent=2) + '\n')
qualification_directory = semantic / 'build/family-qualification-v1'
if not (qualification_directory / 'exit.json').exists():
    print('Not published: current-source qualification still running')
    raise SystemExit(0)
qualification = read(qualification_directory / 'exit.json')
assert (
    qualification['status'] == 'passed-string-family-targeted' and qualification['exit_code'] == 0
)
assert len(qualification['commands']) == 18 and all(
    row['returncode'] == 0 for row in qualification['commands']
)
frozen, tested = read(candidate / 'build/sources.json'), read(semantic / 'build/sources.json')
assert len(frozen) == len(tested) == 144
assert [name for name in frozen if frozen[name] != tested[name]] == ['scripts/archive-audit.py']
for project, manifest in [(candidate, frozen), (semantic, tested)]:
    assert all(digest(project / name) == expected for name, expected in manifest.items())
assert qualification['source_sha256'] == tested
for row in qualification['commands']:
    assert digest(qualification_directory / row['log']) == row['log_sha256']
for configuration in ['normal', 'sanitized']:
    for name, pattern in [
        ('bindings', '519 CPU binding checks passed;'),
        ('strings', '460 string family checks passed;'),
        ('native', '393 passed, 0 failed'),
        ('phase', '3380 phase checks passed;'),
        ('library', '2100 C library checks passed;'),
    ]:
        text = (qualification_directory / (configuration + '-' + name + '.log')).read_text()
        if name == 'library':
            assert '2100' in text and 'passed' in text
        else:
            assert pattern in text, (configuration, name)
states = {}
load_states = {}
matrices = {}
evidence_directories = [qualification_directory]
for configuration in ['normal', 'sanitized']:
    directory = semantic / ('build/compiled-string-states-' + configuration + '-v1')
    proof = read(directory / 'exit.json')
    assert (
        proof['status'] == 'passed-independent-compiled-string-states' and proof['exit_code'] == 0
    )
    assert (
        proof['source_sha256'] == tested
        and proof['captures'] == 160
        and proof['ordinary_c_executions'] == 80
    )
    assert len(proof['checks']) == proof['complete_state_checks'] == 320
    for row in proof['checks']:
        assert row['complex_state_error'] < 1e-10
        for field in ['qasm', 'map', 'events', 'reference']:
            assert digest(directory / row[field]) == row[field + '_sha256']
    states[configuration] = {
        'captures': 160,
        'ordinary_c_executions': 80,
        'complete_state_checks': 320,
        'maximum_amplitude_error': max(row['complex_state_error'] for row in proof['checks']),
        'minimum_physical_wires': min(row['physical_wires'] for row in proof['checks']),
        'maximum_physical_wires': max(row['physical_wires'] for row in proof['checks']),
    }
    evidence_directories.append(directory)
    directory = semantic / ('build/compiled-load-states-' + configuration + '-v2')
    proof = read(directory / 'exit.json')
    assert (
        proof['status'] == 'passed-independent-compiled-cpu-load-states' and proof['exit_code'] == 0
    )
    assert (
        proof['source_sha256'] == tested
        and proof['captures'] == 280
        and proof['ordinary_c_executions'] == 140
    )
    assert len(proof['checks']) == proof['complete_state_checks'] == 560
    for row in proof['checks']:
        assert row['complex_state_error'] < 1e-10
        for field in ['qasm', 'map', 'events', 'reference']:
            assert digest(directory / row[field]) == row[field + '_sha256']
    load_states[configuration] = {
        'captures': 280,
        'ordinary_c_executions': 140,
        'complete_state_checks': 560,
        'maximum_amplitude_error': max(row['complex_state_error'] for row in proof['checks']),
        'scope': 'Distinct source phases, all seven CPU-load profiles, inactive-invalid and coherent-valid paths, complete forward states and saved inverses of the derived result. Inverses of controlled updates themselves remain separate.',
    }
    evidence_directories.append(directory)
    directory = semantic / ('build/current-load-matrix-' + configuration + '-v1')
    matrix = read(directory / 'exit.json')
    assert matrix['exit_code'] == 0 and matrix['rows_passed'] == 140 and matrix['rows_failed'] == 0
    assert (
        matrix['source_sha256'] == tested
        and matrix['native_executions'] == matrix['ordinary_c_executions'] == 420
    )
    assert len(matrix['rows']) == 140 and all(
        row['passed'] and row['runs'] for row in matrix['rows']
    )
    matrices[configuration] = {'rows': 140, 'native_executions': 420, 'ordinary_c_executions': 420}
    evidence_directories.append(directory)
contract_directory = semantic / 'build/exact-string-contract-v1'
contract = read(contract_directory / 'exit.json')
assert (
    contract['exit_code'] == 0
    and contract['executions'] == 40
    and contract['source_sha256'] == tested
)
evidence_directories.append(contract_directory)
broad_directory = candidate / 'build/full-project-v1/build/full-campaign-v1'
broad = read(broad_directory / 'progress.json')
assert broad['status'] == 'running' and not (broad_directory / 'exit.json').exists(), broad.get(
    'error'
)
assert read(candidate / 'build/full-project-v1/build/sources.json') == frozen
assert all(
    row['returncode'] == 0 and digest(broad_directory / row['log']) == row['log_sha256']
    for row in broad['commands']
)
old = read(root / 'docs/audit-evidence/controlled-operands-integration/integration-summary.json')[
    'source_sha256'
]
assert len(old) == 138 and all(digest(root / name) == expected for name, expected in old.items())
out.mkdir()
with tarfile.open(out / 'sources.tar.gz', 'x:gz') as archive:
    for name in frozen:
        archive.add(candidate / name, arcname=name, recursive=False)
    for name in ['LICENSE', 'docs/c11-library.json', 'build/sources.json']:
        archive.add(candidate / name, arcname=name, recursive=False)
suffixes = {'.py', '.json', '.c', '.inc', '.qasm', '.log', '.txt', '.sh', '.ast', '.stderr'}
selected = {
    path
    for directory in evidence_directories
    for path in directory.rglob('*')
    if path.is_file() and not path.is_symlink() and path.suffix in suffixes
}
selected.update(path for path in (root / 'build').glob('*string*v5*.py') if path.is_file())
selected.update(
    [
        root / 'build/prepare-string-integration-v6.py',
        root / 'build/start-string-v6-full.py',
        root / 'build/runtime_export_capture.inc',
        root / 'build/c11-strings-draft.md',
    ]
)
with tarfile.open(out / 'scoped-evidence.tar.gz', 'x:gz') as archive:
    for path in sorted(selected):
        archive.add(path, arcname=path.relative_to(root).as_posix(), recursive=False)
with tarfile.open(out / 'history.tar.gz', 'x:gz') as archive:
    for version in ['string-family-v2', 'string-family-v3']:
        project = root / 'build' / version
        manifest = read(project / 'build/sources.json')
        assert all(digest(project / name) == expected for name, expected in manifest.items())
        for name in manifest:
            archive.add(project / name, arcname=version + '/' + name, recursive=False)
        for path in (project / 'build/family-qualification-v1').iterdir():
            if path.is_file() and path.suffix in suffixes:
                archive.add(path, arcname=version + '/qualification/' + path.name, recursive=False)
        archive.add(
            project / 'build/sources.json',
            arcname=version + '/verified-sources.json',
            recursive=False,
        )
    diagnostic = root / 'build/binding-purity-diagnostic-v1/build/diagnostic.log'
    archive.add(diagnostic, arcname='diagnostic/volatile-result-type.log', recursive=False)
    # Reconstruct the exact tested inputs from canonical sources plus this one metadata delta.
    archive.add(
        semantic / 'scripts/archive-audit.py',
        arcname='targeted-source-delta/scripts/archive-audit.py',
        recursive=False,
    )
for path in out.glob('*.tar.gz'):
    with tarfile.open(path) as archive:
        members = archive.getmembers()
        assert len(members) == len({member.name for member in members})
        assert all(
            member.isfile()
            and not member.name.startswith('/')
            and '..' not in Path(member.name).parts
            for member in members
        )
write(out / 'targeted.json', qualification)
write(
    out / 'load-matrix-normal.json',
    read(semantic / 'build/current-load-matrix-normal-v1/exit.json'),
)
write(
    out / 'load-matrix-sanitized.json',
    read(semantic / 'build/current-load-matrix-sanitized-v1/exit.json'),
)
write(out / 'contract-programs.json', contract)
write(out / 'broad-campaign-at-archive.json', broad)
shutil.copyfile(Path(__file__), out / 'archiver.py')
backup = root / 'build/pre-string-v6-main-backup'
backup.mkdir()
for name, expected in frozen.items():
    target = root / name
    if target.exists() and digest(target) != expected:
        saved = backup / name
        saved.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(target, saved)
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(candidate / name, target)
shutil.copy2(candidate / 'docs/c11-library.json', root / 'docs/c11-library.json')
assert all(digest(root / name) == expected for name, expected in frozen.items())
summary = {
    'status': 'verified-targeted-applied-broad-pending',
    'source_files': 144,
    'source_sha256': frozen,
    'tested_source_sha256': tested,
    'tested_source_difference': ['scripts/archive-audit.py'],
    'source_reuse_scope': 'All C/C++ sources, headers and maintained tests are byte-identical. Canonical archive-audit.py only updates the binding count from399 to519.',
    'previous_integrated_source_sha256': old,
    'targeted_commands_closed_zero': 18,
    'checks_per_configuration': {
        'bindings': 519,
        'strings': 460,
        'native': 393,
        'controlled_operands': 1270,
        'memcmp': 302,
        'strncmp': 246,
        'library': 2100,
        'phase': 3380,
    },
    'independent_compiled_string_states': states,
    'independent_compiled_cpu_load_states': load_states,
    'current_cpu_load_matrix': matrices,
    'exact_documented_program_executions': 40,
    'inventory': {
        'headers': 29,
        'entries': 1224,
        'pending': 780,
        'partial': 76,
        'metadata_only': 368,
        'partial_function_names': 29,
        'complete': 0,
    },
    'root_verification': {
        'platform': 'isolated Linux',
        'status': 'source-hashes-matched',
        'source_files': 144,
    },
    'broad_campaign_at_archive': {
        'status': 'running',
        'closed_commands': len(broad['commands']),
        'planned_commands': 80,
        'source_files': 144,
    },
    'remaining': 'Broad regression closure and current remote CI; inverses of controlled CPU-load updates and cleanup of their retained history beyond the derived-result inverse checked here; dedicated missing-NUL diagnostic; dynamic pointers/layouts/extents; general quantum C11/library compatibility. Existing independent phase proofs remain historical143-input evidence. No physical hardware or speedup claim. Cybersecurity stays classical.',
    'published_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
}
write(out / 'integration-summary.json', summary)
print(
    'Applied canonical144 sources in Linux; closed targeted suites and640 independent string states. Broad80 campaign pending.'
)
