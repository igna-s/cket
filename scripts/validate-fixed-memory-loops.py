#!/usr/bin/env python3
"""Finite complex-state and inverse checks for fixed-buffer CPU loops."""
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


def source(profile):
    if profile == 'partial':
        declarations = 'quant uint16_t a[1]={0x5a07};quant uint8_t input=0;input=coin?9:3;'
        operation = 'memcpy((unsigned char*)a+1,(const void*)&input,1);input++;'
        observe = 'measure(a[0]);measure(input);'
    else:
        declarations = 'quant uint8_t a[3]={0,5,7};a[0]=coin?9:3;'
        if profile == 'forward':
            declarations = 'quant uint8_t a[3]={7,5,0};a[2]=coin?9:3;'
        operation = {
            'copy': 'memcpy((void*)(a+1),(const void*)a,1);a[0]++;',
            'backward': 'memmove((void*)(a+1),(const void*)a,2);a[0]++;',
            'forward': 'memmove((void*)a,(const void*)(a+1),2);a[2]++;',
            'fill': 'memset((void*)a,a[0]+1,3);',
        }[profile]
        observe = (
            'measure(a[0]);measure(a[1]);measure(a[2]);'
            if profile == 'forward'
            else 'measure(a[2]);measure(a[1]);measure(a[0]);'
        )
    return (
        '''#include <quantum.h>
#include <stdlib.h>
#include <string.h>
void ownership_checkpoint(const char*);
int main(int argc,char **argv){
unsigned rounds=argc>1?(unsigned)strtoul(argv[1],0,10):3;if(rounds>3)return 2;
quant bool coin=superposition(0,1);
'''
        + declarations
        + '''
quant uint8_t saved=(uint8_t)a[0]^1;if(coin)phase(.31);ownership_checkpoint("start");
for(unsigned i=0;i<rounds;++i){
'''
        + operation
        + '''
if(a[0]&1)phase(.07);ownership_checkpoint("step");}
if(saved==8)phase(.17);ownership_checkpoint("finish");
'''
        + observe
        + 'measure(coin);return 0;}\n'
    )


def check(profile, points, rounds):
    assert [p['tag'] for p in points] == ['start'] + ['step'] * rounds + ['finish']
    words = [[3, 9], [5, 5], [7, 7]]
    if profile == 'forward':
        words = [[7, 7], [5, 5], [3, 9]]
    elif profile == 'partial':
        words = [[0x5A07, 0x5A07]]
    saved = [(value & 255) ^ 1 for value in words[0]]
    inputs, phases = [3, 9], [0, 0.31]
    wire_values, gates, state = {}, [], {0: 1 + 0j}
    for point in points:
        assert point['measurements'] == point['resets'] == 0
        names = {r['name']: r for r in point['registers'] if r['name'] and not r['history']}

        def assign(register, values):
            for bit, wire in enumerate(register['wires']):
                wire_values[wire] = [(v >> bit) & 1 for v in values]

        if point['tag'] == 'step':
            if profile == 'copy':
                words[1] = list(words[0])
                words[0] = [v + 1 for v in words[0]]
            elif profile == 'backward':
                words = [[v + 1 for v in words[0]], list(words[0]), list(words[1])]
            elif profile == 'forward':
                words = [list(words[1]), list(words[2]), [v + 1 for v in words[2]]]
            elif profile == 'fill':
                fill = [v + 1 for v in words[0]]
                words = [list(fill) for _ in words]
                captures = [
                    r for name, r in names.items() if name.startswith('__cket_memset_fill_')
                ]
                assert len(captures) == 1, 'fill needs one once-evaluated current capture'
                assign(captures[0], fill)
            else:
                words[0] = [(old & 255) | (value << 8) for old, value in zip(words[0], inputs)]
                inputs = [v + 1 for v in inputs]
            phases = [angle + 0.07 * (value & 1) for angle, value in zip(phases, words[0])]
        elif point['tag'] == 'finish':
            phases = [angle + (0.17 if value == 8 else 0) for angle, value in zip(phases, saved)]
        assign(names['coin'], [0, 1])
        for index, values in enumerate(words):
            assign(names[f'a[{index}]'], values)
        if 'saved' in names:
            assign(names['saved'], saved)
        if 'input' in names:
            assign(names['input'], inputs)
        alive = {wire for register in point['registers'] for wire in register['wires']}
        assert alive <= wire_values.keys(), 'unmodelled retained quantum storage'
        expected = {}
        for coin in (0, 1):
            basis = sum(wire_values[wire][coin] << wire for wire in alive)
            expected[basis] = cmath.exp(1j * phases[coin]) / math.sqrt(2)
        actual = {int(b, 0): complex(real, imaginary) for b, real, imaginary in point['state']}
        state = REFERENCE['evolve'](state, point['gates'])
        REFERENCE['equal_states'](actual, state)
        REFERENCE['equal_states'](actual, expected)
        gates.extend(point['gates'])
    inverse = [
        [kind, target, mask, pattern, -angle if kind == 2 else angle]
        for kind, target, mask, pattern, angle in reversed(gates)
    ]
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

    def run(label, command, environment=env):
        result = subprocess.run(
            command,
            cwd=target,
            env=environment,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=120,
        )
        (output / (label + '.log')).write_text(result.stdout)
        commands.append(dict(command=command, exit_code=result.returncode, log=label + '.log'))
        (output / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
        assert result.returncode == 0, result.stdout
        return result.stdout

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
            for rounds in (0, 1, 3):
                for tables in (0, 1):
                    log = run(
                        f'{profile}{level}-n{rounds}-tables{tables}',
                        [str(binary), str(rounds)],
                        dict(env, OWNERSHIP_TABLES=str(tables)),
                    )
                    points = [json.loads(line) for line in log.splitlines() if line.startswith('{')]
                    checkpoints += check(profile, points, rounds)
                    inverses += 1
    summary = dict(
        status='passed',
        profiles=list(PROFILES),
        rounds=[0, 1, 3],
        levels=args.levels,
        synthesis_policies=[0, 1],
        sanitized=args.sanitized,
        full_state_checkpoints=checkpoints,
        complete_gate_inverses=inverses,
        scope='Finite fixed-buffer CPU loops: independent full complex-state formulas, retained versions and complete gate inverses; constant checked addresses/counts, not arbitrary C or states.',
    )
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary))


if __name__ == '__main__':
    main()
