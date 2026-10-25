#!/usr/bin/env python3
"""Check scoped borrow cleanup after provably inactive fixed-storage writes."""
from __future__ import annotations

import argparse
import cmath
import itertools
import json
import os
from pathlib import Path
import runpy
import subprocess

ROOT = Path(__file__).resolve().parent.parent
REFERENCE = runpy.run_path(str(ROOT / 'scripts/validate-ownership.py'))


def source(type_name, operation, control):
    initial = (
        '1'
        if control == 'open_one'
        else '(bool)strtoul(argv[1],0,10)' if control == 'runtime_zero' else '0'
    )
    predicate = '!control' if control == 'open_one' else 'control'
    write = 'y=(word)x;' if operation == 'assignment' else 'memcpy((void*)&y,(const void*)&x,n);'
    # Partial one-byte copies keep the complete-state fixture below 128 wires.
    source_type = 'uint8_t' if operation == 'copy' else 'bool'
    return f'''#include <quantum.h>
#include <stdlib.h>
#include <string.h>
void ownership_checkpoint(const char*);
typedef {type_name} word;
int main(int argc,char **argv){{
if(argc<2)return 2;size_t n=1;
quant {source_type} x=superposition(0,1);quant word y=superposition(0,1);
quant bool control={initial};if(y)phase(.73);ownership_checkpoint("before");
{{quant word saved=y^1;if({predicate}){{{write}}}if(saved)phase(.47);}}
ownership_checkpoint("after");measure(y);measure(x);measure(control);return 0;}}
'''


def check(points, control):
    assert [p['tag'] for p in points] == ['before', 'after']
    state, gates = {0: 1 + 0j}, []
    owners = None
    for point in points:
        assert point['measurements'] == point['resets'] == 0
        names = {r['name']: r for r in point['registers'] if r['name'] and not r['history']}
        current = {
            name: (names[name]['id'], names[name]['wires']) for name in ('x', 'y', 'control')
        }
        if owners is None:
            owners = current
        assert current == owners
        expected = {}
        for x, y in itertools.product((0, 1), repeat=2):
            basis = (x << names['x']['wires'][0]) | (y << names['y']['wires'][0])
            if control == 'open_one':
                basis |= 1 << names['control']['wires'][0]
            angle = 0.73 * y + (0.47 * (1 - y) if point['tag'] == 'after' else 0)
            expected[basis] = cmath.exp(1j * angle) / 2
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
        assert result.returncode == 0, result.stdout[-6000:]
        assert not any(
            token in result.stdout
            for token in ('AddressSanitizer', 'runtime error:', 'internal compiler error:')
        ), result.stdout[-6000:]
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
    for type_name, operation, control in itertools.product(
        ('bool', 'uint8_t', 'uint16_t', 'uint32_t'),
        ('assignment', 'copy'),
        ('zero', 'open_one', 'runtime_zero'),
    ):
        if type_name == 'bool' and operation == 'copy':
            continue
        path = output / (type_name + '-' + operation + '-' + control + '.c')
        path.write_text(source(type_name, operation, control))
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
            for policy in (0, 1):
                log = run(
                    binary.name + f'-tables{policy}',
                    [str(binary), '0'],
                    dict(env, OWNERSHIP_TABLES=str(policy)),
                )
                check(
                    [json.loads(line) for line in log.splitlines() if line.startswith('{')], control
                )
                states += 2
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
        scope='Finite complete states and phase kickback after unobserved borrowed temporaries leave scope. Assignment: bool/8/16/32-bit; one-byte fixed copy into 8/16/32-bit owners. Guards are zero, open one or runtime CPU-prepared zero. The earlier full-word 32-bit copy fixture exceeded 128 wires; 64-bit scope profiles are not qualified. Possible-active coherent cleanup has separate directed evidence in validate-rebound-borrows.py. No general lifetime qualification.',
    )
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary), flush=True)


if __name__ == '__main__':
    main()
