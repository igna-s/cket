#!/usr/bin/env python3
"""Compare complete native states for heap identities, loops and phase consumers."""
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
TYPES = ('uint8_t', 'int8_t', 'uint16_t', 'int16_t', 'uint32_t', 'int32_t', 'uint64_t', 'int64_t')
PROFILES = {
    'assignment': 'p[0]=p[0];',
    'fill': 'memset((void*)p,p[0],1);',
    'move': 'memmove((void*)p,(const void*)p,sizeof *p);',
    'coherent_assignment': 'if(flag)p[0]=p[0];',
    'coherent_fill': 'if(flag)memset((void*)p,p[0],1);',
    'assignment_value': 'if((p[0]=p[0])==1)phase(.17);',
    'cpu_fill_once': 'memset((void*)p,(++calls,0),1);',
    'cpu_fill_empty': 'memset((void*)p,(++calls,7),empty);',
}


def source(type_name, profile):
    return f'''#include <quantum.h>
#include <stdlib.h>
#include <string.h>
void ownership_checkpoint(const char*);
typedef {type_name} word;
int main(int argc,char **argv){{
unsigned rounds=argc>1?(unsigned)strtoul(argv[1],0,10):8;if(rounds>64)return 2;
size_t empty=argc>2?(size_t)strtoul(argv[2],0,10):0;if(empty)return 2;
quant word *p=calloc(1,sizeof *p);if(!p)return 2;
quant bool y=superposition(0,1),flag=superposition(0,1);p[0]=(word){('0' if profile == 'cpu_fill_once' else 'y')};
if(y)phase(.73);if(flag)phase(.31);ownership_checkpoint("before");
unsigned calls=0;
for(unsigned i=0;i<rounds;++i){{{PROFILES[profile]}}}
ownership_checkpoint("after");free((void*)p);measure(y);measure(flag);
return calls!={('rounds' if profile.startswith('cpu_fill_') else '0')};
}}
'''


def check(profile, rounds, points):
    assert [point['tag'] for point in points] == ['before', 'after']
    state, gates, owners = {0: 1 + 0j}, [], None
    for point in points:
        assert point['measurements'] == point['resets'] == 0
        names = {r['name']: r for r in point['registers'] if r['name'] and not r['history']}
        assert len(point['heaps']) == 1 and len(point['heaps'][0]['ids']) == 1
        identity = point['heaps'][0]['ids'][0]
        heap = next(r for r in point['registers'] if r['id'] == identity)
        current = (identity, heap['wires'], names['y']['wires'], names['flag']['wires'])
        if owners is None:
            owners = current
        else:
            assert current == owners, 'an identity changed a public owner or its wires'
        expected = {}
        for bit, flag in itertools.product((0, 1), repeat=2):
            payload = 0 if profile == 'cpu_fill_once' else bit
            basis = (
                (payload << heap['wires'][0])
                | (bit << names['y']['wires'][0])
                | (flag << names['flag']['wires'][0])
            )
            angle = 0.73 * bit + 0.31 * flag
            if point['tag'] == 'after' and profile == 'assignment_value':
                angle += 0.17 * rounds * bit
            expected[basis] = cmath.exp(1j * angle) / 2
        actual = {int(b, 0): complex(real, imag) for b, real, imag in point['state']}
        REFERENCE['equal_states'](actual, expected)
        state = REFERENCE['evolve'](state, point['gates'])
        REFERENCE['equal_states'](actual, state)
        gates.extend(point['gates'])
    inverse = [
        [kind, wire, mask, pattern, -angle if kind == 2 else angle]
        for kind, wire, mask, pattern, angle in reversed(gates)
    ]
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
    records = []

    def run(label, command, environment=env, expected=0, diagnostic=None):
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
        records.append(
            dict(
                command=command,
                exit_code=result.returncode,
                expected_exit_code=expected,
                log=label + '.log',
            )
        )
        (output / 'commands.json').write_text(json.dumps(records, indent=2) + '\n')
        assert result.returncode == expected and (
            not diagnostic or diagnostic in result.stdout
        ), result.stdout[-6000:]
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
    excluded = []
    for type_name, profile in itertools.product(TYPES, PROFILES):
        if '64' in type_name and profile == 'assignment_value':
            excluded.append(
                type_name + ': 64-bit returned capture plus live owners exceeds 128 wires'
            )
            continue
        path = output / (type_name + '-' + profile + '.c')
        path.write_text(source(type_name, profile))
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
            programs += 1
            for policy, rounds in itertools.product((0, 1), (0, 1, 8)):
                log = run(
                    label + f'-tables{policy}-n{rounds}',
                    [str(binary), str(rounds)],
                    dict(env, OWNERSHIP_TABLES=str(policy)),
                )
                points = [json.loads(line) for line in log.splitlines() if line.startswith('{')]
                states += check(profile, rounds, points)
                inverses += 1
        print('PASS', type_name, profile, flush=True)
    boundary = output / 'unsupported-mixed-fill.c'
    boundary.write_text(source('uint8_t', 'fill').replace('p[0],1);', '(++calls,p[0]),1);'))
    for level in args.levels:
        run(
            'unsupported-mixed-fill-' + level,
            [
                str(compiler / 'build/cket'),
                str(boundary),
                '-' + level,
                *flags,
                '-o',
                str(output / 'unsupported-mixed-fill'),
            ],
            expected=1,
            diagnostic='CPU effects in quantum expressions',
        )
    boundary = output / 'unsupported-coherent-cpu-fill.c'
    boundary.write_text(
        source('uint8_t', 'cpu_fill_once').replace(
            'memset((void*)p,(++calls,0),1);', 'if(flag)memset((void*)p,(++calls,0),1);'
        )
    )
    for level in args.levels:
        run(
            'unsupported-coherent-cpu-fill-' + level,
            [
                str(compiler / 'build/cket'),
                str(boundary),
                '-' + level,
                *flags,
                '-o',
                str(output / 'unsupported-coherent-cpu-fill'),
            ],
            expected=1,
            diagnostic='CPU effects in quantum expressions',
        )
    summary = dict(
        status='passed',
        full_state_checkpoints=states,
        complete_gate_inverses=inverses,
        compiled_programs=programs,
        types=list(TYPES),
        profiles=list(PROFILES),
        rounds=[0, 1, 8],
        levels=args.levels,
        synthesis_policies=[0, 1],
        sanitized=args.sanitized,
        excluded_resource_profiles=excluded,
        unsupported_mixed_cpu_quantum_fill_rejections=len(args.levels),
        unsupported_coherent_cpu_fill_rejections=len(args.levels),
        scope='Finite full complex states, surviving owners/wires and private-zero checks for checked heap identities. Pure CPU fill effects are checked on known-zero storage; mixed comma effects with a quantum heap fill remain rejected. Returned captures are checked through 32 bits; 64-bit captures with these live inputs exceed the representation budget. General coherent nonidentity writes and scoped borrow cleanup remain open.',
    )
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary), flush=True)


if __name__ == '__main__':
    main()
