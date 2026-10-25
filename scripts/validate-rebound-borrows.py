#!/usr/bin/env python3
"""Compare complete scoped-borrow states after possible-active coherent writes."""
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
TYPES = ('bool', 'uint8_t', 'int8_t', 'uint16_t', 'int16_t', 'uint32_t', 'int32_t')
PROFILES = (
    'assignment',
    'copy',
    'fill',
    'open',
    'nested',
    'chain',
    'move',
    'arithmetic',
    'function',
    'negative',
)


def source(type_name, profile):
    predicate = '!c' if profile == 'open' else 'c'
    write = (
        'memcpy((void*)&y,(const void*)&x,n);'
        if profile == 'copy'
        else 'memset((void*)&y,x,n);' if profile == 'fill' else 'y=(word)x;'
    )
    if profile == 'nested':
        write = 'if(d){' + write + '}'
    declaration = 'quant word saved=y+1;' if profile == 'arithmetic' else 'quant word saved=y^1;'
    consumer = 'if(saved&1)phase(.47);' if profile == 'arithmetic' else 'if(saved)phase(.47);'
    if profile == 'chain':
        declaration += 'quant uint8_t chain=(uint8_t)(saved^1);'
        consumer = 'if(chain)phase(.47);'
    if profile == 'move':
        consumer = '{quant word moved=saved;if(moved)phase(.47);ownership_checkpoint("borrowed");if(moved&1)phase(.13);}'
    else:
        consumer += (
            'ownership_checkpoint("borrowed");if('
            + ('chain' if profile == 'chain' else 'saved')
            + '&1)phase(.13);'
        )
    helper = ''
    body = '{' + declaration + 'if(' + predicate + '){' + write + '}' + consumer + '}'
    if profile == 'function':
        helper = 'static void update(quant word *p,quant const uint8_t x,quant const bool c){quant word saved=*p^1;if(c)*p=(word)x;if(saved)phase(.47);ownership_checkpoint("borrowed");if(saved&1)phase(.13);}\n'
        body = 'update(&y,x,c);'
    return f'''#include <quantum.h>
#include <stdlib.h>
#include <string.h>
void ownership_checkpoint(const char*);
typedef {type_name} word;
{helper}
int main(int argc,char **argv){{
size_t n=argc>1?strtoul(argv[1],0,10):1;if(n>1)return 2;
quant uint8_t x=superposition(0,1);quant word y=superposition(0,{1 if type_name=='bool' or profile=='negative' else 3});{'y=~y;' if profile=='negative' else ''}
quant bool c=superposition(0,1),d=superposition(0,1);
if(x)phase(.11);if(y&1)phase(.73);if(y&2)phase(.29);if(c)phase(.31);if(d)phase(.19);
ownership_checkpoint("before");{body}ownership_checkpoint("after");
measure(y);measure(x);measure(c);measure(d);return 0;}}
'''


def check(type_name, profile, count, points):
    assert [p['tag'] for p in points] == ['before', 'borrowed', 'after']
    old_values = (-2, -1) if profile == 'negative' else range(2 if type_name == 'bool' else 4)
    rows = list(itertools.product((0, 1), old_values, (0, 1), (0, 1)))
    state, gates = {0: 1 + 0j}, []
    public = None
    for point in points:
        assert point['measurements'] == point['resets'] == 0
        names = {r['name']: r for r in point['registers'] if r['name'] and not r['history']}
        owners = {name: (names[name]['id'], names[name]['wires']) for name in ('x', 'y', 'c', 'd')}
        if public is None:
            public = owners
        assert owners == public, 'controlled overwrite changed a stable public owner'
        extras = [
            r for r in point['registers'] if r['id'] not in {owner[0] for owner in owners.values()}
        ]
        writing = profile not in ('copy', 'fill') or count != 0
        histories = [r for r in extras if r['history']]
        borrows = [r for r in extras if not r['history']]
        if point['tag'] != 'before' and writing:
            assert len(histories) == 1, 'redundant decision or root history'
            history = histories[0]
            assert len(history['wires']) == len(names['y']['wires'])
        else:
            assert not histories, 'unexpected root history'
            history = None
        if point['tag'] == 'borrowed':
            assert len(borrows) == (
                2 if profile == 'chain' else 1
            ), 'unexpected live private decision'
            borrows.sort(key=lambda r: r['id'])
        else:
            assert not borrows, 'scoped borrow did not clean'
        expected = {}
        for x, y, c, d in rows:
            active = (
                not c if profile == 'open' else c and d if profile == 'nested' else c
            ) and writing
            current = x if point['tag'] != 'before' and active else y
            values = [(names['x'], x), (names['y'], current), (names['c'], c), (names['d'], d)]
            if history:
                values.append((history, y if active else 0))
            saved = (
                (bool(y + 1) if type_name == 'bool' else y + 1)
                if profile == 'arithmetic'
                else y ^ 1
            )
            if borrows:
                values.append((borrows[0], saved))
                if profile == 'chain':
                    values.append((borrows[1], y))
            basis = 0
            for register, value in values:
                basis |= sum(
                    ((value >> bit) & 1) << wire for bit, wire in enumerate(register['wires'])
                )
            angle = 0.11 * x + 0.73 * (y & 1) + 0.29 * ((y >> 1) & 1) + 0.31 * c + 0.19 * d
            if point['tag'] != 'before':
                phase = (
                    bool(y)
                    if profile == 'chain'
                    else (
                        (1 if type_name == 'bool' else (y + 1) & 1)
                        if profile == 'arithmetic'
                        else bool(y ^ 1)
                    )
                )
                angle += 0.47 * phase
            if point['tag'] == 'after':
                angle += 0.13 * ((y if profile == 'chain' else saved) & 1)
            expected[basis] = cmath.exp(1j * angle) / math.sqrt(len(rows))
        actual = {int(b, 0): complex(r, i) for b, r, i in point['state']}
        REFERENCE['equal_states'](actual, expected)
        state = REFERENCE['evolve'](state, point['gates'])
        REFERENCE['equal_states'](actual, state)
        gates.extend(point['gates'])
    inverse = [[k, w, m, p, -a if k == 2 else a] for k, w, m, p, a in reversed(gates)]
    REFERENCE['equal_states'](REFERENCE['evolve'](state, inverse), {0: 1 + 0j})


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
    records = []

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
        records.append(dict(command=command, exit_code=result.returncode, log=label + '.log'))
        (output / 'commands.json').write_text(json.dumps(records, indent=2) + '\n')
        assert result.returncode == 0, result.stdout[-8000:]
        assert not any(
            token in result.stdout
            for token in ('AddressSanitizer', 'runtime error:', 'internal compiler error:')
        ), result.stdout[-8000:]
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
    states = inverses = programs = 0
    for type_name, profile in itertools.product(TYPES, PROFILES):
        if type_name == 'bool' and profile in ('copy', 'fill'):
            continue
        if profile == 'negative' and type_name not in ('int8_t', 'int16_t', 'int32_t'):
            continue
        path = output / (type_name + '-' + profile + '.c')
        path.write_text(source(type_name, profile))
        for level in args.levels:
            binary = output / (path.stem + '-' + level)
            run(
                binary.name + '-compile',
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
            programs += 1
            for policy, count in itertools.product(
                (0, 1), (0, 1) if profile in ('copy', 'fill') else (1,)
            ):
                log = run(
                    binary.name + f'-tables{policy}-n{count}',
                    [str(binary), str(count)],
                    dict(env, OWNERSHIP_TABLES=str(policy)),
                )
                points = [json.loads(line) for line in log.splitlines() if line.startswith('{')]
                check(type_name, profile, count, points)
                states += len(points)
                inverses += 1
        print('PASS', path.stem, flush=True)
    summary = dict(
        status='passed',
        full_state_checkpoints=states,
        complete_gate_inverses=inverses,
        compiled_programs=programs,
        levels=args.levels,
        synthesis_policies=[0, 1],
        sanitized=args.sanitized,
        types=list(TYPES),
        profiles=list(PROFILES),
        signed_negative_values=[-2, -1],
        scope='Finite full complex states before, during and after scoped borrow cleanup across single controlled overwrites of root owners. Bool and signed/unsigned 8/16/32-bit assignments; one-byte copy/fill into integer words; open/nested controls, chained/moved/arithmetic borrows and a same-unit function. Includes signed -2/-1 preparations and mixed-sign overwrite histories. Independent input phases, stable public owners/wires, only required old-root history and complete gate reversal. Bool byte operations and 64-bit controlled overwrite profiles are not qualified. Repeated writes, derived destination cleanup, correlated range proofs and nonidentity coherent heap copies require separate contracts/evidence.',
    )
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary), flush=True)


if __name__ == '__main__':
    main()
