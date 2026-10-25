#!/usr/bin/env python3
"""Check inactive branch coherence and the complete controlled-store isometry."""
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
PROFILES = ('copy', 'assignment', 'borrowed', 'borrowed_copy', 'self_control')


def source(profile, coherent):
    control = 'superposition(0,1)' if coherent else '0'
    saved = 'quant uint8_t saved=y^1;' if profile in ('borrowed', 'borrowed_copy') else ''
    predicate = '(y&2)' if profile == 'self_control' else 'control'
    operation = (
        'memcpy((void*)&y,(const void*)&x,n);' if profile in ('copy', 'borrowed_copy') else 'y=x^0;'
    )
    finish = 'measure(saved);' if saved else ''
    return f'''#include <quantum.h>
#include <stdlib.h>
#include <string.h>
void ownership_checkpoint(const char*);
int main(int argc,char **argv){{
size_t n=argc>1?strtoul(argv[1],0,10):1;
quant uint8_t x=superposition(0,1),y=superposition(0,3);
quant bool control={control};
if(x)phase(.31);if(y&1)phase(.73);if(y&2)phase(.29);if(control)phase(.19);
{saved}
ownership_checkpoint("before");
if({predicate}){{{operation}}}
ownership_checkpoint("after");
{finish}measure(y);measure(x);measure(control);return 0;
}}
'''


def check(profile, coherent, count, points):
    assert [p['tag'] for p in points] == ['before', 'after']
    rows = list(itertools.product(range(2), range(4), range(2) if coherent else range(1)))
    amplitudes = [
        cmath.exp(1j * (0.31 * x + 0.73 * (y & 1) + 0.29 * (y >> 1) + 0.19 * c))
        / math.sqrt(16 if coherent else 8)
        for x, y, c in rows
    ]
    state, gates, names_before = {0: 1 + 0j}, [], None
    for point in points:
        assert point['measurements'] == point['resets'] == 0
        names = {r['name']: r for r in point['registers'] if r['name'] and not r['history']}
        values = {}

        def assign(register, content):
            for bit, wire in enumerate(register['wires']):
                values[wire] = [(value >> bit) & 1 for value in content]

        active = [
            (bool(y & 2) if profile == 'self_control' else bool(c))
            and (bool(count) if profile in ('copy', 'borrowed_copy') else True)
            for x, y, c in rows
        ]
        assign(names['x'], [x for x, y, c in rows])
        assign(names['control'], [c for x, y, c in rows])
        assign(
            names['y'],
            [
                x if enabled and point['tag'] == 'after' else y
                for (x, y, c), enabled in zip(rows, active)
            ],
        )
        if profile in ('borrowed', 'borrowed_copy'):
            assign(names['saved'], [y ^ 1 for x, y, c in rows])
        if names_before is None:
            names_before = names
        else:
            assert all(names[name]['wires'] == names_before[name]['wires'] for name in names)
        for register in point['registers']:
            if set(register['wires']) <= values.keys():
                continue
            assert register['history'], 'unexpected live private workspace'
            if len(register['wires']) == 8:
                assign(register, [y if enabled else 0 for (x, y, c), enabled in zip(rows, active)])
            else:
                assert profile == 'self_control' and len(register['wires']) == 1
                assign(register, [int(bool(y & 2)) for x, y, c in rows])
        expected = {}
        for row, amplitude in enumerate(amplitudes):
            basis = sum(content[row] << wire for wire, content in values.items())
            expected[basis] = expected.get(basis, 0j) + amplitude
        actual = {int(b, 0): complex(real, imag) for b, real, imag in point['state']}
        REFERENCE['equal_states'](actual, expected)
        state = REFERENCE['evolve'](state, point['gates'])
        REFERENCE['equal_states'](actual, state)
        assert abs(sum(abs(a) ** 2 for a in actual.values()) - 1) < 1e-10
        gates.extend(point['gates'])
    inverse = [[k, w, m, p, -angle if k == 2 else angle] for k, w, m, p, angle in reversed(gates)]
    REFERENCE['equal_states'](REFERENCE['evolve'](state, inverse), {0: 1 + 0j})
    return len(points)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler-root', type=Path, default=ROOT)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--levels', nargs='+', default=['O2'])
    parser.add_argument('--sanitized', action='store_true')
    args = parser.parse_args()
    compiler, output = args.compiler_root.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
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
        result = subprocess.run(
            command,
            cwd=ROOT,
            env=environment,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=120,
        )
        (output / (label + '.log')).write_text(result.stdout)
        commands.append(dict(command=command, exit_code=result.returncode, log=label + '.log'))
        (output / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
        assert result.returncode == 0, result.stdout[-5000:]
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
    total = inverses = 0
    for profile in PROFILES:
        for coherent in (False, True):
            path = output / (profile + ('-coherent' if coherent else '-off') + '.c')
            path.write_text(source(profile, coherent))
            for level in args.levels:
                label = path.stem + '-' + level
                binary = output / label
                run(
                    label + '-compile',
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
                for policy, count in itertools.product((0, 1), (0, 1)):
                    log = run(
                        label + f'-tables{policy}-n{count}',
                        [str(binary), str(count)],
                        dict(env, OWNERSHIP_TABLES=str(policy)),
                    )
                    points = [json.loads(line) for line in log.splitlines() if line.startswith('{')]
                    total += check(profile, coherent, count, points)
                    inverses += 1
                print('PASS', label, flush=True)
    summary = dict(
        status='passed',
        full_state_checkpoints=total,
        complete_gate_inverses=inverses,
        profiles=list(PROFILES),
        levels=args.levels,
        synthesis_policies=[0, 1],
        sanitized=args.sanitized,
        scope='Finite full-state formulas for controlled fixed-storage writes, including inactive branches, retained prior values, borrowed results and a changed condition source. No heap/general-ABI qualification.',
    )
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary), flush=True)


if __name__ == '__main__':
    main()
