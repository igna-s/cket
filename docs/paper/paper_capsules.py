"""Check finite native delivery records without presenting them as full C11 proof."""

from __future__ import annotations

import hashlib
import json
import re
import runpy
import tarfile


def deferred_delivery(root, folder, delivery, check):
    build_only = delivery['status'] == 'built-runtime-qualification-deferred'
    with tarfile.open(folder / 'evidence.tar.gz') as archive:
        sources = {
            name: hashlib.sha256(archive.extractfile('sources/' + name).read()).hexdigest()
            for name in delivery['source_sha256']
        }
        outcome = json.load(archive.extractfile('records/exit.json'))
        logs_match = all(
            hashlib.sha256(archive.extractfile('records/' + name).read()).hexdigest() == digest
            for name, digest in delivery['log_sha256'].items()
        )
        native = '' if build_only else archive.extractfile('records/native.log').read().decode()
    check(
        'Active deferred delivery: current/archive sources and qualification scope match',
        delivery['source_files'] == len(sources)
        and sources == delivery['source_sha256']
        and all(
            hashlib.sha256((root / name).read_bytes()).hexdigest() == digest
            for name, digest in sources.items()
        )
        and delivery['status']
        in {
            'built-and-examples-executed-full-qualification-deferred',
            'built-runtime-qualification-deferred',
        }
        and all(value == 'deferred' for value in delivery['qualification'].values()),
    )
    check(
        'Active deferred delivery: recorded build/runtime outcomes retain their actual scope',
        outcome['status'] == delivery['status']
        and all(row['exit_code'] == 0 for row in outcome['commands'])
        and logs_match
        and (
            (
                build_only
                and delivery['qualification']['native_examples'] == 'deferred'
                and delivery['qualification']['native_regression'] == 'deferred'
            )
            or (
                not build_only
                and delivery['native_regression']['exit_code'] == 0
                and re.search(
                    rf"^{delivery['native_regression']['checks']} passed, 0 failed ", native, re.M
                )
            )
        ),
    )
    return dict(
        status=delivery['status'],
        source_files=len(sources),
        scope='Build or examples only; full runtime qualification remains deferred.',
    )


def finite_native_delivery(root, folder, delivery, check):
    supported = 'finite-fixed-heap-transfer-qualified'
    check(
        'Active native delivery: supported explicit evidence schema',
        delivery['status'] == supported,
    )
    check(
        'Active native delivery: artifact bytes match their hashes',
        all(
            hashlib.sha256((folder / name).read_bytes()).hexdigest() == digest
            for name, digest in delivery['artifact_sha256'].items()
        ),
    )
    with tarfile.open(folder / 'evidence.tar.gz') as archive:

        def read(name):
            return archive.extractfile('records/' + name).read().decode()

        sources = {
            name: hashlib.sha256(archive.extractfile('sources/' + name).read()).hexdigest()
            for name in delivery['source_sha256']
        }
        check(
            'Active native delivery: archived and current compiler/test inputs match',
            len(sources) == delivery['source_files']
            and sources == delivery['source_sha256']
            and all(
                hashlib.sha256((root / name).read_bytes()).hexdigest() == digest
                for name, digest in sources.items()
            ),
        )
        validator = runpy.run_path(str(root / 'scripts/validate-heap-transfer.py'))
        reference = validator['REFERENCE']
        totals = {
            'full_state_checkpoints': 0,
            'complete_gate_inverses': 0,
            'native_program_executions': 0,
            'semantic_rejections': 0,
        }
        for mode in ('normal', 'sanitized'):
            steps = json.loads(read(mode + '/exit.json'))
            required = {'transfer-matrix', 'heap-engine', 'byte-regression', 'heap-regression'}
            required |= (
                {'native-core', 'block05', 'static-export', 'inventory'}
                if mode == 'normal'
                else {'example-compile', 'example-run'}
            )
            check(
                mode + ': required campaign steps are present exactly once',
                len(steps) == len(required) and {s['label'] for s in steps} == required,
            )
            for step in steps:
                log = read(mode + '/' + step['log'])
                static = step['label'] == 'static-export'
                check(
                    f'{mode}: recorded command {step["label"]} has its required outcome',
                    (
                        (step['exit_code'] == 1 and 'static export is unsupported' in log)
                        if static
                        else step['exit_code'] == 0
                    ),
                )
            summary = json.loads(read(mode + '/matrix/summary.json'))
            check(
                mode + ': matrix has the recorded finite scope and instrumentation mode',
                summary == delivery[mode]
                and summary['status'] == 'passed'
                and summary['sanitized'] == (mode == 'sanitized')
                and summary['synthesis_policies'] == [0, 1],
            )
            commands = json.loads(read(mode + '/matrix/commands.json'))
            indexed = {command['log']: command for command in commands}
            expected_logs = {'trace-compile.log'}
            required_successes = {'trace-compile.log'}
            for level in summary['levels']:
                for profile in validator['PROFILES']:
                    expected_logs.add(profile + level + '-compile.log')
                    expected_logs.update(
                        profile + level + f'-tables{table}.log' for table in (0, 1)
                    )
                    required_successes.add(profile + level + '-compile.log')
                for name in validator['VALID']:
                    for suffix in ('-compile.log', '-run.log'):
                        key = name + level + suffix
                        expected_logs.add(key)
                        check(
                            mode + ': required positive program command completed: ' + key,
                            key in indexed and indexed[key]['exit_code'] == 0,
                        )
                for name, (_, diagnostic) in validator['INVALID'].items():
                    compile_key = name + level + '-compile.log'
                    expected_logs.add(compile_key)
                    check(
                        mode
                        + ': required negative fixture compiled or was rejected: '
                        + compile_key,
                        compile_key in indexed and indexed[compile_key]['exit_code'] in (0, 1),
                    )
                    selected = compile_key
                    if indexed[compile_key]['exit_code'] == 0:
                        selected = name + level + '-run.log'
                        expected_logs.add(selected)
                    check(
                        mode + ': negative fixture has its semantic outcome: ' + selected,
                        selected in indexed
                        and indexed[selected]['exit_code'] == 1
                        and diagnostic in read(mode + '/matrix/' + selected),
                    )
            if mode == 'sanitized':
                for component in (
                    'cket_plugin.so',
                    'json.o',
                    'quantum.o',
                    'verify.o',
                    'simulator.o',
                    'cket_runtime.o',
                    'cket',
                ):
                    key = 'instrumentation-' + component + '.log'
                    expected_logs.add(key)
                    instrumented = read(mode + '/matrix/' + key)
                    check(
                        'Sanitizer instrumentation: ' + component,
                        '__asan_init' in instrumented and '__ubsan_handle' in instrumented,
                    )
            check(
                mode + ': all finite fixtures and logs are accounted for once',
                len(commands) == len(indexed)
                and set(indexed) == expected_logs
                and summary['native_programs'] == len(validator['VALID']) * len(summary['levels'])
                and summary['semantic_rejections']
                == len(validator['INVALID']) * len(summary['levels']),
            )
            check(
                mode + ': adapters and traced programs compiled successfully',
                all(
                    key in indexed and indexed[key]['exit_code'] == 0 for key in required_successes
                ),
            )
            points = inverses = 0
            for command in commands:
                log = read(mode + '/matrix/' + command['log'])
                code = command['exit_code']
                check(
                    mode
                    + ': matrix command has no signal/infrastructure/sanitizer failure: '
                    + command['log'],
                    (code == 0 or (code == 1 and '|c>' in log))
                    and not re.search(
                        r'AddressSanitizer:|LeakSanitizer:|UndefinedBehaviorSanitizer:|runtime error:|internal compiler error|Segmentation fault',
                        log,
                    ),
                )
                match = re.fullmatch(r'(.+)(O[0123s])-tables([01])\.log', command['log'])
                if not match:
                    continue
                profile = match[1]
                check(
                    mode + ': traced program completed',
                    code == 0 and profile in validator['PROFILES'],
                )
                snapshots = [json.loads(line) for line in log.splitlines() if line.startswith('{')]
                check(
                    mode + ': four state checkpoints retain their order',
                    [p['tag'] for p in snapshots] == ['prepared', 'written', 'freed', 'inverse'],
                )
                state, context, gates = {0: 1 + 0j}, {}, []
                for point in snapshots:
                    check(
                        mode + ': no hidden measurement/reset before the checkpoint',
                        point['measurements'] == point['resets'] == 0,
                    )
                    gates.extend(point['gates'])
                    state = reference['evolve'](state, point['gates'])
                    native = {int(b, 0): complex(r, i) for b, r, i in point['state']}
                    reference['equal_states'](native, state)
                    reference['equal_states'](
                        native, validator['expected'](profile, point, context)
                    )
                    points += 1
                inverse = [[k, w, m, p, -a if k == 2 else a] for k, w, m, p, a in reversed(gates)]
                reference['equal_states'](reference['evolve'](state, inverse), {0: 1 + 0j})
                inverses += 1
            check(
                mode + ': counted states and full gate inverses match the summary',
                points == summary['full_state_checkpoints']
                and inverses == summary['complete_gate_inverses'],
            )
            totals['full_state_checkpoints'] += points
            totals['complete_gate_inverses'] += inverses
            totals['native_program_executions'] += summary['native_programs']
            totals['semantic_rejections'] += summary['semantic_rejections']
        check(
            'Active native delivery: repeated finite totals are not broader coverage',
            all(delivery[name] == count for name, count in totals.items()),
        )
    return dict(
        status=supported,
        source_files=len(sources),
        totals=totals,
        scope='Archived states rechecked against the existing finite formulas and independent gate interpreter; no new broad compiler campaign.',
    )
