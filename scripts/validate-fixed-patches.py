#!/usr/bin/env python3
"""Finite fixed-word byte updates: public channels, phases and gate inverses."""
from __future__ import annotations

import argparse
import cmath
import itertools
import json
import math
import os
from pathlib import Path
import runpy
import subprocess

ROOT = Path(__file__).resolve().parent.parent
REFERENCE = runpy.run_path(str(ROOT / 'scripts/validate-ownership.py'))
DENSITY = runpy.run_path(str(ROOT / 'scripts/validate-heap-coherence.py'))
TYPES = ('uint16_t', 'int16_t', 'uint32_t', 'int32_t')
PROFILES = ('static_copy', 'copy', 'open', 'nested', 'fill', 'alias_fill', 'move', 'borrow', 'heap')


def width(type_name):
    return int(''.join(c for c in type_name if c.isdigit()))


def source(type_name, producer, profile):
    negative = type_name.startswith('int')
    initial = (
        'quant word y=superposition(0,1);y^=(word)(a?256:0);' + ('y^=(word)-1;' if negative else '')
        if producer == 'root'
        else 'quant bool b=superposition(0,1);quant word y=(word)'
        + ('~' if negative else '')
        + '((a?256:0)|b);'
    )
    operation = {
        'static_copy': 'memcpy((void*)&y,(const void*)&x,1);',
        'fill': 'memset((void*)&y,x,n);',
        'alias_fill': 'memset((void*)&y,y,n);',
        'move': 'memmove((char*)&y+1,(const void*)&y,n);',
        'heap': 'memcpy((void*)&y,(const void*)p,n);',
    }.get(profile, 'memcpy((void*)&y,(const void*)&x,n);')
    predicate = {'open': '!c', 'nested': 'c&&d'}.get(profile, 'c')
    effect = f'if({predicate}){{{operation}}}'
    if profile == 'borrow':
        effect = (
            '{quant word saved=y^1;'
            + effect
            + 'n=0;if(saved&1)phase(.47);ownership_checkpoint("borrowed");'
            + 'if(saved&256)phase(.13);}'
        )
    heap = 'quant uint8_t *p=calloc(1,1);if(!p)return 2;' if profile == 'heap' else ''
    return f'''#include <quantum.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
void ownership_checkpoint(const char*);
typedef {type_name} word;
int main(int argc,char **argv){{
size_t n=argc>1?strtoul(argv[1],0,10):1;
{heap}
quant bool a=superposition(0,1);quant uint8_t x=superposition(0,1);
quant bool c=superposition(0,1),d=superposition(0,1);
{initial}{'p[0]=x;' if profile == 'heap' else ''}
if(a)phase(.11);if(y&1)phase(.23);if(x)phase(.41);if(c)phase(.17);if(d)phase(.29);
ownership_checkpoint("before");{effect}ownership_checkpoint("after");
{'free((void*)p);' if profile == 'heap' else ''}
measure(y);{'measure(b);' if producer == 'derived' else ''}
measure(a);measure(x);measure(c);measure(d);return 0;}}
'''


def check(type_name, producer, profile, count, points):
    borrowed = profile == 'borrow'
    assert [p['tag'] for p in points] == (
        ['before', 'borrowed', 'after'] if borrowed else ['before', 'after']
    )
    bits = width(type_name)
    full = (1 << bits) - 1
    public = ['a'] + (['b'] if producer == 'derived' else []) + ['x', 'c', 'd', 'y']
    evolved, gates, original = {0: 1 + 0j}, [], None
    for point in points:
        assert point['measurements'] == point['resets'] == 0
        names = {r['name']: r for r in point['registers'] if r['name'] and not r['history']}
        registers = [names[n] for n in public]
        if profile == 'heap':
            by_id = {r['id']: r for r in point['registers']}
            registers.extend(by_id[i] for h in point['heaps'] for i in h['ids'])
        identities = [(r['id'], r['wires']) for r in registers]
        if original is None:
            original = identities
        assert identities == original, 'byte patch changed a public register identity'
        if point['tag'] == 'borrowed':
            registers.append(names['saved'])
        blocks = {}
        for a, b, x, c, d in itertools.product((0, 1), repeat=5):
            old = 256 * a + b
            if type_name.startswith('int'):
                old = (~old) & full
            active = bool(not c if profile == 'open' else c and d if profile == 'nested' else c)
            size = 1 if profile == 'static_copy' else count
            mask = ((1 << (8 * size)) - 1) << (8 if profile == 'move' else 0)
            incoming = (
                (old & 255) << 8
                if profile == 'move'
                else sum(
                    (old & 255 if profile == 'alias_fill' else x) << (8 * i) for i in range(size)
                )
            )
            changed = point['tag'] != 'before' and active and size
            value = ((old & ~mask) | incoming) & full if changed else old
            # Unknown roots retain overwritten bits. A derived producer's
            # pending old-value inverse also pins its saved decision while
            # the scoped borrower lives; both temporaries clean afterwards.
            retained = point['tag'] != 'before' and producer == 'root'
            history = old & mask if retained and active and size else 0
            if producer == 'derived' and point['tag'] == 'borrowed' and size:
                history = (int(active), old & mask if active else 0)
            # Filling a low byte with itself is a proved identity, including
            # the single-byte alias case. It needs no irreversible history.
            if profile == 'alias_fill' and size <= 1:
                history = 0
            outputs = [a] + ([b] if producer == 'derived' else []) + [x, c, d, value]
            if profile == 'heap':
                outputs.append(x)
            saved = old ^ 1
            if point['tag'] == 'borrowed':
                outputs.append(saved)
            angle = 0.11 * a + 0.23 * (old & 1) + 0.41 * x + 0.17 * c + 0.29 * d
            if borrowed and point['tag'] != 'before':
                angle += 0.47 * bool(saved & 1)
                if point['tag'] != 'borrowed':
                    angle += 0.13 * bool(saved & 256)
            local = shift = 0
            for register, value in zip(registers, outputs, strict=True):
                local |= (int(value) & ((1 << len(register['wires'])) - 1)) << shift
                shift += len(register['wires'])
            blocks.setdefault(history, {})[local] = cmath.exp(1j * angle) / math.sqrt(32)
        actual = DENSITY['actual_density'](point, registers)
        expected = DENSITY['density'](blocks)
        error = max(
            abs(actual.get(k, 0j) - expected.get(k, 0j)) for k in actual.keys() | expected.keys()
        )
        assert error < 1e-10, (type_name, producer, profile, count, point['tag'], error)
        current = {int(b, 0): complex(r, i) for b, r, i in point['state']}
        evolved = REFERENCE['evolve'](evolved, point['gates'])
        REFERENCE['equal_states'](current, evolved)
        gates.extend(point['gates'])
    inverse = [[k, w, m, p, -a if k == 2 else a] for k, w, m, p, a in reversed(gates)]
    REFERENCE['equal_states'](REFERENCE['evolve'](evolved, inverse), {0: 1 + 0j})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler-root', type=Path, default=ROOT)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--levels', nargs='+', default=['O2'])
    parser.add_argument('--types', nargs='+', choices=TYPES, default=list(TYPES))
    parser.add_argument('--profiles', nargs='+', choices=PROFILES, default=list(PROFILES))
    parser.add_argument('--sanitized', action='store_true')
    args = parser.parse_args()
    target, output = args.compiler_root.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    flags = (
        ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
        if args.sanitized
        else []
    )
    env = dict(os.environ, CC='gcc-11', CKET_SIMULATOR='sparse', CKET_SEED='7')
    if args.sanitized:
        env.update(CC=str(target / 'build/gcc-wrapper'), CKET_REAL_GCC='gcc-11')
    records = []

    def run(label, command, environment=env):
        result = subprocess.run(
            command,
            cwd=ROOT,
            env=environment,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=180,
        )
        (output / (label + '.log')).write_text(result.stdout)
        records.append(dict(command=command, exit_code=result.returncode, log=label + '.log'))
        (output / 'commands.json').write_text(json.dumps(records, indent=2) + '\n')
        assert result.returncode == 0, result.stdout[-6000:]
        assert not any(
            s in result.stdout
            for s in ('AddressSanitizer', 'runtime error:', 'internal compiler error:')
        )
        return result.stdout

    trace = output / 'trace.o'
    run(
        'trace',
        [
            'gcc-11',
            '-std=c11',
            '-O1',
            *flags,
            '-c',
            str(ROOT / 'tests/ownership_trace.c'),
            '-o',
            str(trace),
        ],
    )
    links = ['-Wl,' + str(trace), '-Wl,--wrap=qc_create_sparse', '-Wl,--wrap=__cket_apply']
    programs = states = inverses = 0
    for type_name, producer, profile in itertools.product(
        args.types, ('root', 'derived'), args.profiles
    ):
        path = output / (type_name + '-' + producer + '-' + profile + '.c')
        path.write_text(source(type_name, producer, profile))
        counts = (
            (1,)
            if profile == 'static_copy'
            else (0, 1, 2) if profile in ('fill', 'alias_fill') else (0, 1)
        )
        for level in args.levels:
            binary = output / (path.stem + '-' + level)
            run(
                binary.name + '-compile',
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
            programs += 1
            for policy, count in itertools.product((0, 1), counts):
                log = run(
                    binary.name + f'-tables{policy}-n{count}',
                    [str(binary), str(count)],
                    dict(env, OWNERSHIP_TABLES=str(policy)),
                )
                points = [json.loads(line) for line in log.splitlines() if line.startswith('{')]
                check(type_name, producer, profile, count, points)
                states += len(points)
                inverses += 1
        print('PASS', path.stem, flush=True)
    summary = dict(
        status='passed',
        public_density_comparisons=states,
        joint_gate_state_comparisons=states,
        complete_gate_inverses=inverses,
        compiled_programs=programs,
        levels=args.levels,
        types=args.types,
        profiles=args.profiles,
        synthesis_policies=[0, 1],
        sanitized=args.sanitized,
        scope='Finite signed/unsigned 16/32-bit fixed-word patches with unknown roots and derived producers, '
        'CPU lengths 0/1/2, static copies, open/nested controls, aliasing fills, intra-word overlapping moves, '
        'heap-to-fixed transfers and scoped old-value phases after the CPU length changes. '
        'Independent public reduced-density formulas and physical gate evolution/inversion. '
        'No arbitrary-state, all-library, full C11 or QPU qualification.',
    )
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary), flush=True)


if __name__ == '__main__':
    main()
