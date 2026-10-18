"""Archive closed137/138 campaigns without rewriting historical evidence."""
from pathlib import Path
import csv
import hashlib
import importlib.util
import json
import shutil
import tarfile

root = Path(__file__).resolve().parent.parent
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
spec = importlib.util.spec_from_file_location('closed_full_campaign_checks', root / 'build/run-isolated-memory-audit.py')
original = importlib.util.module_from_spec(spec)
spec.loader.exec_module(original)
for label, candidate_name, count, commands in (
        ('coherent-control-full', 'coherent-cpu-maintained', 137, 74),
        ('controlled-operands-full', 'coherent-body-maintained-v5', 138, 76)):
    candidate = root / 'build' / candidate_name
    campaign = candidate / 'build/full-campaign-v1'
    terminal = json.loads((campaign / 'exit.json').read_text())
    progress = json.loads((campaign / 'progress.json').read_text())
    frozen = original.source_check(candidate, candidate / 'build/maintained-sources.json')
    assert len(frozen) == count and terminal['status'] == 'passed-in-segments' and terminal['exit_code'] == 0
    assert terminal['normal_status'] == terminal['sanitized_status'] == 'passed-in-segments'
    assert len(progress['commands']) == commands and all(row['returncode'] == 0 for row in progress['commands'])
    for row in progress['commands']:
        assert digest(campaign / row['log']) == row['log_sha256']
    assert digest(campaign / 'combined-verify.log') == terminal['combined_log_sha256']
    original.check_transcript(candidate, (campaign / 'combined-verify.log').read_text())
    fuzz = {}
    for configuration in ('normal', 'sanitized'):
        coverage = json.loads((campaign / (configuration + '-fuzz-coverage.json')).read_text())
        assert coverage['case_indices'] == list(range(133)) and coverage['compilations'] == 665
        assert coverage['seed'] == 20261012 and coverage['optimization_levels'] == 5 and coverage['byte_inputs_per_compilation'] == 256
        for row in coverage['segments']:
            assert digest(campaign / row['log']) == row['sha256']
        fuzz[configuration] = coverage
    with (candidate / 'build/synthesis-growth.csv').open() as handle:
        assert len(list(csv.DictReader(handle))) == 24
    destination = root / 'docs/audit-evidence' / label
    destination.mkdir()
    for path in campaign.iterdir():
        if path.is_file() and path.suffix in ('.json', '.log', '.py', '.c', '.sh', '.txt'):
            shutil.copyfile(path, destination / path.name)
    shutil.copyfile(candidate / 'build/synthesis-growth.csv', destination / 'synthesis-growth.csv')
    shutil.copyfile(Path(__file__), destination / 'archiver.py')
    for name in ('run-isolated-memory-audit.py', 'resume-linux-memory-audit-v2.py'):
        shutil.copyfile(root / 'build' / name, destination / name)
    summary = {'status': 'verified-broad-campaign', 'source_files': count, 'source_sha256': frozen,
        'source_archive': '../coherent-cpu-conditions-maintained/sources.tar.gz' if count == 137 else '../controlled-operands-v5/sources.tar.gz',
        'closed_commands': commands, 'exit_code': 0, 'finished_utc': terminal['finished_utc'],
        'normal_fresh_core_sha256': terminal['normal_fresh_core_sha256'],
        'sanitized_fresh_core_sha256': terminal['sanitized_fresh_core_sha256'],
        'counts_per_configuration': {'native': 393, 'memory_returns': 290, 'pointer_differences': 170,
            'controlled_operands': 1270 if count == 138 else None, 'frontend_compilations': 665,
            'frontend_original_indices': 133, 'byte_inputs_per_compilation': 256,
            'synthesis_circuits': 24, 'independent_quest_states': 64, 'independent_quest_channels': 6},
        'scope': 'Fresh normal and ASan/UBSan builds followed by closed individual suites. Full original133-index fuzz corpus is executed in derived-driver ranges; generator, programs, scalar oracle, seed20261012 and all five optimization levels are preserved. This is an explicit aggregate, not one original scripts/verify.sh invocation. Source hashes match frozen endpoints. The finite campaign does not certify all C11 or absence of known bugs. No host/WSL/security setting was changed.',
        'artifact_sha256': {path.name: digest(path) for path in destination.iterdir() if path.is_file()}}
    (destination / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print('Archived closed campaign:', label, count, commands)
