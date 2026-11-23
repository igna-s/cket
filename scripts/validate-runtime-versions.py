#!/usr/bin/env python3
"""Check finite native iteration histories against values, phases and gate inversion."""
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
PROFILES = ('scalar', 'transfer', 'effects', 'branches', 'function')


def source(profile):
    initialize = 'fixed=coin?9:3;' if profile in ('scalar', 'branches') else ''
    operation = {
        'scalar': 'fixed%=2;',
        'transfer': 'size_t n=(i&1)?0:1;memcpy((void*)&fixed,(const void*)p,n);p[0]++;',
        'effects': 'sum+=p[0]++;memcpy((void*)&fixed,(const void*)p,1);',
        'branches': 'if(i&1)fixed=coin?5:2;else fixed%=2;',
        'function': 'unsigned n=(i&1)?0:1;take(p,&fixed,n);bad|=n!=((i&1)?0u:1u);p[0]++;',
    }[profile]
    return f'''#include <quantum.h>
#include <stdlib.h>
#include <string.h>
void ownership_checkpoint(const char*);
void take(quant const uint8_t *p,quant uint8_t *out,unsigned n){{
    while(n--)memcpy((void*)out,(const void*)p,1);
}}
int main(int argc,char **argv){{
    unsigned rounds=argc>1?(unsigned)strtoul(argv[1],0,10):3;
    if(rounds>3)return 2;
    quant uint8_t *p=calloc(1,1);if(!p)return 2;
    quant bool coin=superposition(0,1);
    quant uint8_t fixed=7,sum=0;{initialize}
    p[0]=coin?9:3;quant uint8_t saved=fixed^1;
    if(coin)phase(.31);ownership_checkpoint("start");
    unsigned i=0;int bad=0;
    while(i<rounds){{{operation}
        if(fixed&1)phase(.07);if(sum&1)phase(.11);
        ownership_checkpoint("step");++i;
    }}
    free((void*)p);if(saved==8)phase(.17);ownership_checkpoint("finish");
    measure(fixed);measure(sum);measure(coin);return bad;
}}'''


def check_states(profile, points, rounds):
    assert [p['tag'] for p in points] == ['start'] + ['step'] * rounds + ['finish']
    wires, gates, phases = {}, [], [0, 0.31]
    fixed = [3, 9] if profile in ('scalar', 'branches') else [7, 7]
    heap, total, saved = [3, 9], [0, 0], [value ^ 1 for value in fixed]
    state = {0: 1 + 0j}
    for position, point in enumerate(points):
        assert point['measurements'] == point['resets'] == 0
        names = {r['name']: r for r in point['registers'] if r['name'] and not r['history']}
        current = {r['id']: r for r in point['registers']}

        def assign(register, values):
            for bit, wire in enumerate(register['wires']):
                wires[wire] = [(v >> bit) & 1 for v in values]

        for patch in point['patches']:
            old = [list(wires[w]) for w in patch['target_wires']]
            for bit, wire in enumerate(patch['history_wires']):
                wires[wire] = old[bit] if (patch['bit_mask'] >> bit) & 1 else [0, 0]

        if point['tag'] == 'step':
            iteration = position - 1
            previous = list(heap)
            old_wires = names['fixed']['wires']
            if profile == 'scalar':
                fixed = [v % 2 for v in fixed]
            elif profile == 'branches':
                fixed = [2, 5] if iteration & 1 else [v % 2 for v in fixed]
            elif profile in ('transfer', 'function'):
                if not iteration & 1:
                    fixed = list(heap)
                else:
                    assert old_wires == previous_fixed_wires, 'empty transfer changed its owner'
                heap = [v + 1 for v in heap]
            else:
                total = [a + b for a, b in zip(total, heap)]
                heap = [v + 1 for v in heap]
                fixed = list(heap)
                captures = [r for name, r in names.items() if name.startswith('__cket_heap_value_')]
                assert len(captures) == 1, 'postfix capture must have one current owner'
                assign(captures[0], previous)
            phases = [
                angle + 0.07 * (v & 1) + 0.11 * (s & 1) for angle, v, s in zip(phases, fixed, total)
            ]
        elif point['tag'] == 'finish':
            phases = [angle + (0.17 if value == 8 else 0) for angle, value in zip(phases, saved)]
        assign(names['coin'], [0, 1])
        assign(names['fixed'], fixed)
        if 'sum' in names:
            assign(names['sum'], total)
        if 'saved' in names:
            assign(names['saved'], saved)
        if point['heaps']:
            assert len(point['heaps']) == 1 and len(point['heaps'][0]['ids']) == 1
            assign(current[point['heaps'][0]['ids'][0]], heap)
        previous_fixed_wires = names['fixed']['wires']
        alive = {wire for r in point['registers'] for wire in r['wires']}
        # Old live versions keep their previous physical bits. Released wires
        # must be zero; values never come from the engine's basis certificates.
        assert alive <= wires.keys(), 'an unmodelled history register survived'
        expected = {}
        for coin in (0, 1):
            basis = sum(wires[wire][coin] << wire for wire in alive)
            expected[basis] = cmath.exp(1j * phases[coin]) / math.sqrt(2)
        native = {int(b, 0): complex(r, i) for b, r, i in point['state']}
        state = REFERENCE['evolve'](state, point['gates'])
        REFERENCE['equal_states'](native, state)
        REFERENCE['equal_states'](native, expected)
        gates.extend(point['gates'])
    inverse = [
        [kind, target, mask, pattern, -angle if kind == 2 else angle]
        for kind, target, mask, pattern, angle in reversed(gates)
    ]
    REFERENCE['equal_states'](REFERENCE['evolve'](state, inverse), {0: 1 + 0j})
    return len(points)


def parse_points(log):
    patches, points = [], []
    for line in log.splitlines():
        if line.startswith('PATCH '):
            patches.append(json.loads(line[6:]))
        elif line.startswith('{'):
            point = json.loads(line)
            point['patches'] = patches
            points.append(point)
            patches = []
    return points


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler-root', type=Path, default=ROOT)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--levels', nargs='+', default=['O2'])
    parser.add_argument('--sanitized', action='store_true')
    args = parser.parse_args()
    compiler, output = args.compiler_root.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    levels = [level.lstrip('-') for level in args.levels]
    flags = (
        ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
        if args.sanitized
        else []
    )
    env = dict(os.environ, CC='gcc-11', CKET_SIMULATOR='sparse', CKET_SEED='7')
    if args.sanitized:
        env.update(CC=str(compiler / 'build/gcc-wrapper'), CKET_REAL_GCC='gcc-11')
    commands = []

    def run(label, command, environment=env):
        r = subprocess.run(
            command,
            cwd=compiler,
            env=environment,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=120,
        )
        (output / (label + '.log')).write_text(r.stdout)
        commands.append(dict(command=command, exit_code=r.returncode, log=label + '.log'))
        (output / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
        assert r.returncode == 0, r.stdout
        return r.stdout

    adapter = output / 'trace.o'
    run(
        'trace-compile',
        [
            'gcc-11',
            '-std=c11',
            '-O1',
            *flags,
            '-c',
            str(ROOT / 'tests/heap_transfer_trace.c'),
            '-o',
            str(adapter),
        ],
    )
    links = ['-Wl,' + str(adapter)] + [
        '-Wl,--wrap=' + name
        for name in (
            'qc_create_sparse',
            '__cket_apply',
            'qc_apply',
            'qc_apply_bound',
            '__cket_read_byte',
        )
    ]
    checkpoints = inverses = 0
    for level in levels:
        for profile in PROFILES:
            path, binary = output / (profile + '.c'), output / (profile + level)
            path.write_text(source(profile))
            run(
                profile + level + '-compile',
                [
                    str(compiler / 'build/cket'),
                    str(path),
                    '-' + level,
                    *flags,
                    *links,
                    '-o',
                    str(binary),
                ],
            )
            for rounds in (0, 1, 3):
                for tables in (0, 1):
                    log = run(
                        f'{profile}{level}-n{rounds}-tables{tables}',
                        [str(binary), str(rounds)],
                        dict(env, OWNERSHIP_TABLES=str(tables)),
                    )
                    points = parse_points(log)
                    checkpoints += check_states(profile, points, rounds)
                    inverses += 1
    summary = dict(
        status='passed',
        levels=levels,
        sanitized=args.sanitized,
        profiles=list(PROFILES),
        rounds=[0, 1, 3],
        synthesis_policies=[0, 1],
        full_state_checkpoints=checkpoints,
        complete_gate_inverses=inverses,
        scope='Finite iterative integer, heap result and byte-transfer programs; full complex joint states and complete gate inversion, not arbitrary C or state proof.',
    )
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary))


if __name__ == '__main__':
    main()
