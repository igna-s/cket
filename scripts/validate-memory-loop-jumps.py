#!/usr/bin/env python3
"""Finite byte-operation, history-mask and coherence checks across CPU loop jumps."""
from __future__ import annotations

import argparse
import cmath
import json
import math
import os
from pathlib import Path
import runpy
import subprocess

ROOT = Path(__file__).resolve().parent.parent
REFERENCE = runpy.run_path(str(ROOT / 'scripts/validate-ownership.py'))
MEMORY = runpy.run_path(str(ROOT / 'scripts/validate-runtime-memory.py'))
PROFILES = ('copy', 'backward', 'forward', 'fill', 'partial')
FLOWS = ('break', 'continue', 'mixed', 'while', 'do')


def iterations(flow, selector):
    if flow == 'break':
        return list(range(selector))
    if flow == 'continue':
        return [i for i in range(4) if i != selector]
    result = []
    for i in range(4):
        if i == selector:
            continue
        if i == 2:
            break
        result.append(i)
    return result


def source(profile, flow):
    text = MEMORY['source'](profile)
    text = text.replace(
        'unsigned rounds=argc>2?(unsigned)strtoul(argv[2],0,10):3;if(length>2||rounds>3)return 2;',
        'unsigned selector=argc>2?(unsigned)strtoul(argv[2],0,10):2;'
        'if(length>2||selector>3)return 2;',
    )
    header = {
        'break': 'for(unsigned i=0;i<4;++i){if(i==selector)break;',
        'continue': 'for(unsigned i=0;i<4;++i){if(i==selector)continue;',
        'mixed': 'for(unsigned i=0;i<4;++i){if(i==selector)continue;if(i==2)break;',
        'while': 'unsigned cursor=0;while(cursor<4){unsigned i=cursor++;'
        'if(i==selector)continue;if(i==2)break;',
        'do': 'unsigned cursor=0;do{unsigned i=cursor++;' 'if(i==selector)continue;if(i==2)break;',
    }[flow]
    text = text.replace(
        'for(unsigned i=0;i<rounds;++i){size_t n=(length+i)%3;',
        header + 'size_t n=(length+i)%' + ('2' if profile == 'partial' else '3') + ';',
    )
    text = text.replace('if(a[0]&1)phase(.07);', 'if(a[0]&1)phase(.07*(i+1));')
    if flow == 'do':
        text = text.replace(
            'ownership_checkpoint("step");}', 'ownership_checkpoint("step");}while(cursor<4);'
        )
    if profile == 'partial':
        text = text.replace(
            'quant uint16_t word=0xa507;', 'quant uint16_t word=superposition(0,1);word^=0xa506;'
        )
        text = text.replace(
            'memcpy((void*)&word,(const void*)a,n);',
            'memcpy((unsigned char*)&word+1,(const void*)a,n);',
        )
        text = text.replace(
            'ownership_checkpoint("start");', 'if(word&1)phase(-.23);ownership_checkpoint("start");'
        )
        text = text.replace(
            'ownership_checkpoint("step");',
            'if(word&1)phase(.05*(i+1));ownership_checkpoint("step");',
        )
    assert 'rounds' not in text
    return text


def check_low_coherence(state, wire, angle):
    grouped = {}
    for basis, amplitude in state.items():
        pair = grouped.setdefault(basis & ~(1 << wire), [0j, 0j])
        pair[(basis >> wire) & 1] += amplitude
    density = [
        [sum(pair[i] * pair[j].conjugate() for pair in grouped.values()) for j in range(2)]
        for i in range(2)
    ]
    ideal = [[0.5, 0.5 * cmath.exp(-1j * angle)], [0.5 * cmath.exp(1j * angle), 0.5]]
    assert all(abs(density[i][j] - ideal[i][j]) < 1e-10 for i in range(2) for j in range(2)), (
        'untouched low-byte coherence lost',
        density,
        ideal,
    )


def check(profile, flow, points, length, selector, mutate=False):
    schedule = iterations(flow, selector)
    assert [p['tag'] for p in points] == ['start'] + ['step'] * len(schedule) + ['finish']
    rows = [
        (coin, bit) for bit in (range(2) if profile == 'partial' else range(1)) for coin in range(2)
    ]
    values = [[3 if coin == 0 else 9 for coin, bit in rows], [5] * len(rows)]
    if profile != 'partial':
        values += [[7] * len(rows), [11] * len(rows)]
    word = [0xA506 | bit for coin, bit in rows]
    saved = [v ^ 1 for v in values[0]]
    phases = [0.31 * coin - 0.23 * bit for coin, bit in rows]
    wires, previous, state, gates = {}, {}, {0: 1 + 0j}, []
    density_checks = mutations = 0
    for position, point in enumerate(points):
        assert point['measurements'] == point['resets'] == 0
        names = {r['name']: r for r in point['registers'] if r['name'] and not r['history']}

        def assign(register, content):
            for bit, wire in enumerate(register['wires']):
                wires[wire] = [(value >> bit) & 1 for value in content]

        if point['tag'] == 'step':
            iteration = schedule[position - 1]
            count = (length + iteration) % (2 if profile == 'partial' else 3)
            # Derive the write mask from C byte ranges, independently of the
            # compiler's trace. In particular, a high-byte patch must not
            # retain a second copy of the untouched, independently phased low byte.
            if profile == 'partial':
                masks = {'word': 0xFF00 if count else 0}
            else:
                start = 2 if profile == 'copy' else 1 if profile == 'backward' else 0
                maximum = 4 - start
                if profile == 'forward':
                    maximum = 3
                masks = {f'a[{start + i}]': 255 if i < count else 0 for i in range(maximum)}
            if not count:
                assert all(
                    names[name]['wires'] == previous[name] for name in masks
                ), 'empty byte prefix changed destination wires'
            for patch in point['patches']:
                targets = {
                    name
                    for name in masks
                    if names[name]['wires'] == patch['target_wires']
                    or previous.get(name) == patch['target_wires']
                }
                assert len(targets) == 1, ('unknown patch target', patch, targets)
                target = next(iter(targets))
                mask = masks[target]
                assert patch['bit_mask'] == mask, ('non-C write mask', target, patch, mask)
                old = [list(wires[wire]) for wire in patch['target_wires']]
                for bit, wire in enumerate(patch['history_wires']):
                    wires[wire] = old[bit] if (mask >> bit) & 1 else [0] * len(rows)
            old = [list(content) for content in values]
            if profile == 'copy':
                for i in range(count):
                    values[i + 2] = list(old[i])
                values[0] = [v + 1 for v in old[0]]
            elif profile == 'backward':
                for i in range(count):
                    values[i + 1] = list(old[i])
                values[0] = [v + 1 for v in old[0]]
            elif profile == 'forward':
                for i in range(count):
                    values[i] = list(old[i + 1])
                values[3] = [v + 1 for v in old[3]]
            elif profile == 'fill':
                fill = [v + 1 for v in old[0]]
                for i in range(count):
                    values[i] = list(fill)
                captures = [
                    r for name, r in names.items() if name.startswith('__cket_dynamic_fill_')
                ]
                assert len(captures) == 1
                assign(captures[0], fill)
            else:
                if count:
                    word = [(v & 255) | (byte << 8) for v, byte in zip(word, old[0])]
                values[0] = [v + 1 for v in old[0]]
            phases = [
                angle
                + 0.07 * (iteration + 1) * (v & 1)
                + (0.05 * (iteration + 1) * bit if profile == 'partial' else 0)
                for angle, v, (coin, bit) in zip(phases, values[0], rows)
            ]
        else:
            assert not point['patches'], 'write outside an executed C iteration'
            if point['tag'] == 'finish':
                phases = [angle + (0.17 if v == 8 else 0) for angle, v in zip(phases, saved)]
        assign(names['coin'], [coin for coin, bit in rows])
        for i, content in enumerate(values):
            assign(names[f'a[{i}]'], content)
        if 'word' in names:
            assign(names['word'], word)
        if 'saved' in names:
            assign(names['saved'], saved)
        for name, register in names.items():
            if name.startswith('__cket_string_domain_'):
                assign(register, [0] * len(rows))
            if name.startswith('__cket_dynamic_fill_') and position == 0:
                assign(register, [0] * len(rows))
        previous = {name: r['wires'] for name, r in names.items()}
        alive = {wire for r in point['registers'] for wire in r['wires']}
        assert alive <= wires.keys(), ('unmodelled storage', alive - wires.keys(), names)
        expected = {
            sum(wires[wire][row] << wire for wire in alive): cmath.exp(1j * phases[row])
            / math.sqrt(len(rows))
            for row in range(len(rows))
        }
        actual = {int(b, 0): complex(real, imag) for b, real, imag in point['state']}
        if profile == 'partial':
            completed = (
                position
                if point['tag'] == 'step'
                else len(schedule) if point['tag'] == 'finish' else 0
            )
            angle = -0.23 + 0.05 * sum(i + 1 for i in schedule[:completed])
            wire = names['word']['wires'][0]
            check_low_coherence(actual, wire, angle)
            density_checks += 1
            if mutate and point['tag'] == 'start':
                extra = max(alive) + 1
                copied = {b | (((b >> wire) & 1) << extra): a for b, a in actual.items()}
                try:
                    check_low_coherence(copied, wire, angle)
                except AssertionError:
                    mutations += 1
                else:
                    raise AssertionError(
                        'oracle accepted an unwanted basis copy of the untouched bit'
                    )
        REFERENCE['equal_states'](actual, expected)
        state = REFERENCE['evolve'](state, point['gates'])
        REFERENCE['equal_states'](actual, state)
        gates.extend(point['gates'])
    inverse = [[k, w, m, p, -a if k == 2 else a] for k, w, m, p, a in reversed(gates)]
    REFERENCE['equal_states'](REFERENCE['evolve'](state, inverse), {0: 1 + 0j})
    return len(points), density_checks, mutations


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler-root', type=Path, default=ROOT)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--levels', nargs='+', default=['O0', 'O2'])
    parser.add_argument('--sanitized', action='store_true')
    args = parser.parse_args()
    target, output = args.compiler_root.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    flags = (
        ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
        if args.sanitized
        else []
    )
    env = dict(os.environ, CKET_SIMULATOR='sparse', CKET_SEED='7')
    commands = []

    def run(label, command, environment=env):
        z = subprocess.run(
            command,
            cwd=target,
            env=environment,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=120,
        )
        (output / (label + '.log')).write_text(z.stdout)
        commands.append(dict(command=command, exit_code=z.returncode, log=label + '.log'))
        (output / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
        assert z.returncode == 0, z.stdout[-6000:]
        return z.stdout

    trace = output / 'trace.o'
    run(
        'trace',
        [
            'gcc-11',
            '-std=c11',
            '-O1',
            *flags,
            '-c',
            str(ROOT / 'tests/heap_transfer_trace.c'),
            '-o',
            str(trace),
        ],
    )
    links = ['-Wl,' + str(trace)] + [
        '-Wl,--wrap=' + name
        for name in (
            'qc_create_sparse',
            '__cket_apply',
            'qc_apply',
            'qc_apply_bound',
            '__cket_read_byte',
        )
    ]
    checkpoints = inverses = density_checks = mutations = 0
    for level in args.levels:
        for profile in PROFILES:
            for flow in FLOWS:
                label = profile + '-' + flow + '-' + level
                path, binary = output / (label + '.c'), output / label
                path.write_text(source(profile, flow))
                run(
                    label + '-compile',
                    [
                        str(target / 'build/cket'),
                        str(path),
                        '-' + level,
                        *flags,
                        *links,
                        '-o',
                        str(binary),
                    ],
                )
                for length in range(3):
                    for selector in range(4):
                        for tables in (0, 1):
                            log = run(
                                label + f'-l{length}-s{selector}-t{tables}',
                                [str(binary), str(length), str(selector)],
                                dict(env, OWNERSHIP_TABLES=str(tables)),
                            )
                            points, patches = [], []
                            for line in log.splitlines():
                                if line.startswith('PATCH '):
                                    patches.append(json.loads(line[6:]))
                                elif line.startswith('{'):
                                    point = json.loads(line)
                                    point['patches'], patches = patches, []
                                    points.append(point)
                            checked, densities, rejected = check(
                                profile, flow, points, length, selector, mutate=not mutations
                            )
                            checkpoints += checked
                            density_checks += densities
                            mutations += rejected
                            inverses += 1
    summary = dict(
        status='passed',
        profiles=list(PROFILES),
        flows=list(FLOWS),
        levels=args.levels,
        sanitized=args.sanitized,
        length_selectors=[0, 1, 2],
        jump_selectors=[0, 1, 2, 3],
        synthesis_policies=[0, 1],
        full_state_checkpoints=checkpoints,
        complete_gate_inverses=inverses,
        untouched_low_byte_density_comparisons=density_checks,
        unwanted_low_bit_copy_mutations_rejected=mutations,
        scope='Finite runtime byte prefixes through CPU break/continue: complete correlated complex states, C-derived patch masks, unknown untouched low-byte coherence, old-value phases and complete gate inverses. Partial high-byte view counts 0/1; other views counts 0/1/2. No arbitrary-state, coherent-caller, general ABI/lifetime, all-C11 or physical QPU qualification.',
    )
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary))
    print('Memory loop jumps passed; finite states, C byte masks and complete inverses recorded.')


if __name__ == '__main__':
    main()
