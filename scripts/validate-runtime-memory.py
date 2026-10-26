#!/usr/bin/env python3
"""Check finite runtime byte prefixes against complete states, phases and inverses."""
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
PROFILES = ('copy', 'backward', 'forward', 'fill', 'partial')
BOUNDARIES = {
    'observe_return': (
        0,
        None,
        'unsigned got=measure(*(uint8_t*)memcpy((void*)b,(const void*)a,n));unsigned second=measure(b[1]),input=measure(a[0]);measure(a[1]);unsigned c=measure(coin);return got!=(n?input:7)||second!=(n>1?5:11)||input!=(c?9:3);',
    ),
    'cpu_read': (
        1,
        'measure explicitly',
        'unsigned bad=*(uint8_t*)memcpy((void*)b,(const void*)a,n);(void)bad;',
    ),
    'cpu_call': (
        1,
        'cannot escape to a CPU call',
        'printf("%s",(char*)memcpy((void*)b,(const void*)a,n));',
    ),
    'cpu_store': (
        1,
        'CPU storage escape',
        'struct box{void *p;};struct box box={memcpy((void*)b,(const void*)a,n)};(void)box;',
    ),
    'write_return': (
        1,
        'memory-return writes require',
        '*(uint8_t*)memcpy((void*)b,(const void*)a,n)=4;',
    ),
}


def boundary_source(name):
    body = BOUNDARIES[name][2]
    finish = (
        ''
        if name == 'observe_return'
        else 'measure(b[1]);measure(b[0]);measure(a[1]);measure(a[0]);measure(coin);return 0;'
    )
    return (
        '''#include <quantum.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int main(int argc,char **argv){size_t n=argc>1?strtoul(argv[1],0,10):2;
quant bool coin=superposition(0,1);quant uint8_t a[2]={0,5},b[2]={7,11};a[0]=coin?9:3;
'''
        + body
        + finish
        + '}\n'
    )


def source(profile):
    declarations = 'quant uint8_t a[4]={0,5,7,11};a[0]=coin?9:3;'
    if profile == 'partial':
        declarations = 'quant uint8_t a[2]={0,5};a[0]=coin?9:3;quant uint16_t word=0xa507;'
    operation = {
        'copy': 'memcpy((void*)(a+2),(const void*)a,n);a[0]++;',
        'backward': 'memmove((void*)(a+1),(const void*)a,n);a[0]++;',
        'forward': 'memmove((void*)a,(const void*)(a+1),n);a[3]++;',
        'fill': 'memset((void*)a,a[0]+1,n);',
        'partial': 'memcpy((void*)&word,(const void*)a,n);a[0]++;',
    }[profile]
    observe = (
        'measure(a[0]);measure(a[1]);measure(a[2]);measure(a[3]);'
        if profile == 'forward'
        else (
            'measure(word);measure(a[1]);measure(a[0]);'
            if profile == 'partial'
            else 'measure(a[3]);measure(a[2]);measure(a[1]);measure(a[0]);'
        )
    )
    return (
        '''#include <quantum.h>
#include <stdlib.h>
#include <string.h>
void ownership_checkpoint(const char*);
int main(int argc,char **argv){
unsigned length=argc>1?(unsigned)strtoul(argv[1],0,10):2;
unsigned rounds=argc>2?(unsigned)strtoul(argv[2],0,10):3;if(length>2||rounds>3)return 2;
quant bool coin=superposition(0,1);
'''
        + declarations
        + '''
quant uint8_t saved=a[0]^1;if(coin)phase(.31);ownership_checkpoint("start");
for(unsigned i=0;i<rounds;++i){size_t n=(length+i)%3;
'''
        + operation
        + '''
if(a[0]&1)phase(.07);ownership_checkpoint("step");}
if(saved==8)phase(.17);ownership_checkpoint("finish");
'''
        + observe
        + 'measure(coin);return 0;}\n'
    )


def check(profile, points, length, rounds):
    assert [p['tag'] for p in points] == ['start'] + ['step'] * rounds + ['finish']
    values = [[3, 9], [5, 5]] + ([] if profile == 'partial' else [[7, 7], [11, 11]])
    word, saved, phases = [0xA507, 0xA507], [2, 8], [0, 0.31]
    wire_values, previous_wires, gates, state = {}, {}, [], {0: 1 + 0j}
    for position, point in enumerate(points):
        assert point['measurements'] == point['resets'] == 0
        names = {r['name']: r for r in point['registers'] if r['name'] and not r['history']}

        def assign(register, content):
            for bit, wire in enumerate(register['wires']):
                wire_values[wire] = [(value >> bit) & 1 for value in content]

        for patch in point['patches']:
            old = [list(wire_values[w]) for w in patch['target_wires']]
            for bit, wire in enumerate(patch['history_wires']):
                wire_values[wire] = old[bit] if (patch['bit_mask'] >> bit) & 1 else [0, 0]

        if point['tag'] == 'step':
            count = (length + position - 1) % 3
            old = [list(pair) for pair in values]
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
                for i in range(count):
                    word = [
                        (v & ~(255 << (8 * i))) | (byte << (8 * i)) for v, byte in zip(word, old[i])
                    ]
                values[0] = [v + 1 for v in old[0]]
            targets = {
                'copy': ('a[2]', 'a[3]'),
                'backward': ('a[1]', 'a[2]', 'a[3]'),
                'forward': ('a[0]', 'a[1]', 'a[2]'),
                'fill': tuple(f'a[{i}]' for i in range(4)),
                'partial': ('word',),
            }[profile]
            if not count:
                assert all(
                    names[name]['wires'] == previous_wires[name] for name in targets
                ), 'empty prefix changed destination wires'
            phases = [angle + 0.07 * (v & 1) for angle, v in zip(phases, values[0])]
        elif point['tag'] == 'finish':
            phases = [angle + (0.17 if value == 8 else 0) for angle, value in zip(phases, saved)]
        assign(names['coin'], [0, 1])
        for i, pair in enumerate(values):
            assign(names[f'a[{i}]'], pair)
        if 'word' in names:
            assign(names['word'], word)
        if 'saved' in names:
            assign(names['saved'], saved)
        previous_wires = {name: register['wires'] for name, register in names.items()}
        alive = {wire for register in point['registers'] for wire in register['wires']}
        assert alive <= wire_values.keys(), 'unmodelled retained version'
        expected = {
            sum(wire_values[wire][coin] << wire for wire in alive): cmath.exp(1j * phases[coin])
            / math.sqrt(2)
            for coin in (0, 1)
        }
        actual = {int(b, 0): complex(r, i) for b, r, i in point['state']}
        state = REFERENCE['evolve'](state, point['gates'])
        REFERENCE['equal_states'](actual, state)
        REFERENCE['equal_states'](actual, expected)
        gates.extend(point['gates'])
    inverse = [[k, w, m, p, -a if k == 2 else a] for k, w, m, p, a in reversed(gates)]
    REFERENCE['equal_states'](REFERENCE['evolve'](state, inverse), {0: 1 + 0j})
    return len(points)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler-root', type=Path, default=ROOT)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--levels', nargs='+', default=['O2'])
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

    def run(label, command, environment=env, expected=0, diagnostic=None):
        r = subprocess.run(
            command,
            cwd=target,
            env=environment,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=120,
        )
        (output / (label + '.log')).write_text(r.stdout)
        commands.append(
            dict(
                command=command,
                exit_code=r.returncode,
                expected_exit_code=expected,
                log=label + '.log',
            )
        )
        (output / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
        assert r.returncode == expected and (not diagnostic or diagnostic in r.stdout), r.stdout
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
    for level in args.levels:
        for profile in PROFILES:
            path, binary = output / (profile + '.c'), output / (profile + level)
            path.write_text(source(profile))
            run(
                profile + level + '-compile',
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
            for length in (0, 1, 2):
                for rounds in (0, 1, 3):
                    for tables in (0, 1):
                        log = run(
                            f'{profile}{level}-n{length}-r{rounds}-tables{tables}',
                            [str(binary), str(length), str(rounds)],
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
                        checkpoints += check(profile, points, length, rounds)
                        inverses += 1
    boundary_executions = boundary_rejections = 0
    for level in args.levels:
        for name, (expected, diagnostic, _) in BOUNDARIES.items():
            path, binary = output / ('boundary-' + name + '.c'), output / (
                'boundary-' + name + level
            )
            path.write_text(boundary_source(name))
            run(
                name + level + '-compile',
                [str(target / 'build/cket'), str(path), '-' + level, *flags, '-o', str(binary)],
                expected=expected,
                diagnostic=diagnostic,
            )
            if expected:
                boundary_rejections += 1
            else:
                for length in (0, 1, 2):
                    run(name + level + '-n' + str(length), [str(binary), str(length)])
                    boundary_executions += 1
    summary = dict(
        status='passed',
        profiles=list(PROFILES),
        lengths=[0, 1, 2],
        rounds=[0, 1, 3],
        levels=args.levels,
        synthesis_policies=[0, 1],
        sanitized=args.sanitized,
        full_state_checkpoints=checkpoints,
        complete_gate_inverses=inverses,
        boundary_native_executions=boundary_executions,
        compile_boundary_rejections=boundary_rejections,
        scope='Finite runtime CPU byte prefixes over fixed integer views, complete correlated complex states and gate inversion; no arbitrary state/C11 or unlimited-resource claim.',
    )
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary))


if __name__ == '__main__':
    main()
