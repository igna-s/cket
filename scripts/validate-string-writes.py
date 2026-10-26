#!/usr/bin/env python3
"""Finite string-write channels, scoped phases and independent gate inverses."""
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
PROFILES = (
    'copy',
    'copy_open',
    'copy_nested',
    'copy_borrow',
    'copy_empty',
    'bounded_one',
    'bounded_two',
    'concat_empty',
    'concat_zero',
    'concat',
    'concat_borrow',
)


def width(type_name):
    return int(''.join(c for c in type_name if c.isdigit()))


def supported(type_name, producer, profile):
    return not (
        (width(type_name) == 32 and (profile.endswith('borrow') or profile == 'concat'))
        or (producer == 'derived' and profile == 'concat_borrow')
    )


def source(type_name, producer, profile):
    negative = type_name.startswith('int') and not profile.startswith('concat')
    initial = (
        'quant word y=superposition(0,1);y^=(word)(a?256:0);' + ('y^=(word)-1;' if negative else '')
        if producer == 'root'
        else 'quant bool b=superposition(0,1);quant word y=(word)'
        + ('~' if negative else '')
        + '(((word)a<<8)|b);'
    )
    operation = {
        'copy_empty': 'strcpy((char*)&y,"");',
        'bounded_one': 'strncpy((char*)&y,(const char*)src,1);',
        'bounded_two': 'strncpy((char*)&y,(const char*)src,2);',
        'concat_empty': 'strcat((char*)&y,"");',
        'concat_zero': 'strncat((char*)&y,(const char*)src,0);',
        'concat': 'strcat((char*)&y,(const char*)src);',
        'concat_borrow': 'strncat((char*)&y,(const char*)src,1);',
    }.get(profile, 'strcpy((char*)&y,(const char*)src);')
    predicate = '!c' if profile == 'copy_open' else 'c&&d' if profile == 'copy_nested' else 'c'
    if profile.startswith('concat'):
        predicate = 'c&&!(y&255)'
    effect = f'if(enabled){{if({predicate}){{{operation}}}}}'
    if profile == 'concat_borrow':
        effect = (
            '{quant bool saved=(y&1)==0,high_saved=(y&256)!=0;'
            + effect
            + 'if(saved)phase(.47);if(high_saved)phase(.13);}'
        )
    elif profile.endswith('borrow'):
        effect = (
            '{quant word saved=y^1;' + effect + 'if(saved&1)phase(.47);if(saved&256)phase(.13);}'
        )
    return f'''#include <quantum.h>
#include <stdint.h>
#include <string.h>
void ownership_checkpoint(const char*);
typedef {type_name} word;
int main(int argc,char **argv){{
int enabled=argc>1&&argv[1][0]=='1';
quant bool a=superposition(0,1),x=superposition(0,1);
quant bool c=superposition(0,1),d=superposition(0,1);
quant uint8_t src[{1 if profile=='concat_borrow' else 2}]={{{'x^0' if profile=='concat_borrow' else 'x^0,0'}}};{initial}
if(a)phase(.11);if(y&1)phase(.23);if(x)phase(.41);if(c)phase(.17);if(d)phase(.29);
ownership_checkpoint("before");{effect}ownership_checkpoint("after");
measure(y);{'measure(b);' if producer=='derived' else ''}
measure(src[0]);{'' if profile=='concat_borrow' else 'measure(src[1]);'}measure(a);measure(x);measure(c);measure(d);return 0;}}
'''


def check(type_name, producer, profile, enabled, points):
    assert [p['tag'] for p in points] == ['before', 'after']
    bits = width(type_name)
    full = (1 << bits) - 1
    public = ['a'] + (['b'] if producer == 'derived' else []) + ['x', 'c', 'd', 'y', 'src[0]']
    if profile != 'concat_borrow':
        public.append('src[1]')
    evolved, gates, identities = {0: 1 + 0j}, [], None
    for point in points:
        assert point['measurements'] == point['resets'] == 0
        names = {r['name']: r for r in point['registers'] if r['name'] and not r['history']}
        registers = [names[name] for name in public]
        current_ids = [(r['id'], r['wires']) for r in registers]
        if identities is None:
            identities = current_ids
        assert current_ids == identities, 'string write changed a public owner identity'
        blocks = {}
        for a, b, x, c, d in itertools.product((0, 1), repeat=5):
            old = 256 * a + b
            if type_name.startswith('int') and not profile.startswith('concat'):
                old = (~old) & full
            active = bool(
                enabled
                and (
                    (c and not (old & 255))
                    if profile.startswith('concat')
                    else (
                        (not c)
                        if profile == 'copy_open'
                        else (c and d) if profile == 'copy_nested' else c
                    )
                )
            )
            count = 0
            incoming = x
            if profile == 'copy_empty':
                count, incoming = 1, 0
            elif profile == 'bounded_one':
                count = 1
            elif profile == 'bounded_two':
                count = 2
            elif profile in ('concat_empty', 'concat_zero'):
                count = 0
            elif profile.startswith('concat'):
                count = 2 if x else 0
            else:
                count = x + 1
            mask = (1 << (8 * count)) - 1 if count else 0
            changed = point['tag'] == 'after' and active and count
            value = ((old & ~mask) | incoming) & full if changed else old
            history = (old & mask if changed else 0,)
            if producer == 'derived' or point['tag'] == 'before':
                history = ()
            elif profile.startswith('concat'):
                # A decision about an unknown destination must remain available
                # where the selected update destroys that decision's input.
                history += (mask if changed else 0,)
            outputs = [a] + ([b] if producer == 'derived' else []) + [x, c, d, value, x]
            if profile != 'concat_borrow':
                outputs.append(0)
            angle = 0.11 * a + 0.23 * (old & 1) + 0.41 * x + 0.17 * c + 0.29 * d
            if profile.endswith('borrow') and point['tag'] == 'after':
                saved = old ^ 1
                angle += 0.47 * bool(saved & 1) + 0.13 * bool(saved & 256)
            local = shift = 0
            for register, value in zip(registers, outputs, strict=True):
                local |= (int(value) & ((1 << len(register['wires'])) - 1)) << shift
                shift += len(register['wires'])
            blocks.setdefault(history, {})[local] = cmath.exp(1j * angle) / math.sqrt(32)
        actual, expected = DENSITY['actual_density'](point, registers), DENSITY['density'](blocks)
        error = max(
            abs(actual.get(k, 0j) - expected.get(k, 0j)) for k in actual.keys() | expected.keys()
        )
        assert error < 1e-10, (type_name, producer, profile, enabled, point['tag'], error)
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
    parser.add_argument('--types', nargs='+', choices=TYPES, default=list(TYPES))
    parser.add_argument('--profiles', nargs='+', choices=PROFILES, default=list(PROFILES))
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
    env = dict(os.environ, CC='gcc-11', CKET_SIMULATOR='sparse', CKET_SEED='7')
    if args.sanitized:
        env.update(CC=str(target / 'build/gcc-wrapper'), CKET_REAL_GCC='gcc-11')
    records = []

    def run(label, command, environment=env, expected_status=0):
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
        assert result.returncode == expected_status, result.stdout[-6000:]
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
    exclusions = []
    for type_name, producer, profile in itertools.product(
        args.types, ('root', 'derived'), args.profiles
    ):
        if not supported(type_name, producer, profile):
            exclusions.append(
                dict(
                    type=type_name,
                    producer=producer,
                    profile=profile,
                    reason='This fixture exceeds the 128-wire/workspace budget; no positive qualification.',
                )
            )
            continue
        path = output / (type_name + '-' + producer + '-' + profile + '.c')
        path.write_text(source(type_name, producer, profile))
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
            for policy, enabled in itertools.product((0, 1), (0, 1)):
                log = run(
                    binary.name + f'-tables{policy}-cpu{enabled}',
                    [str(binary), str(enabled)],
                    dict(env, OWNERSHIP_TABLES=str(policy)),
                )
                points = [json.loads(line) for line in log.splitlines() if line.startswith('{')]
                check(type_name, producer, profile, enabled, points)
                states += len(points)
                inverses += 1
        print('PASS', path.stem, flush=True)
    preflights = 0
    for operation, active, level in itertools.product(('strcpy', 'strcat'), (0, 1), args.levels):
        path = output / f'preflight-{operation}-active{active}-{level}.c'
        path.write_text(
            '#include <quantum.h>\n#include <stdint.h>\n#include <string.h>\n'
            'void ownership_checkpoint(const char*);\n'
            'int main(int argc,char **argv){quant uint16_t y='
            + ('superposition(0,1)' if operation == 'strcpy' else '0')
            + f';quant bool active={active};char src[2]={{argc>1?argv[1][0]:0,1}};'
            + 'ownership_checkpoint("before");if(active){'
            + operation
            + '((char*)&y,src);}'
            + 'ownership_checkpoint("after");measure(y);measure(active);return 0;}\n'
        )
        binary = path.with_suffix('')
        run(
            binary.name + '-compile',
            [str(target / 'build/cket'), str(path), '-' + level, *flags, *links, '-o', str(binary)],
        )
        for policy in (0, 1):
            failing = dict(env, OWNERSHIP_TABLES=str(policy), OWNERSHIP_EXIT_CHECKPOINT='1')
            log = run(binary.name + f'-tables{policy}', [str(binary), '1'], failing, active)
            points = [json.loads(line) for line in log.splitlines() if line.startswith('{')]
            assert [p['tag'] for p in points] == (
                ['before', 'exit'] if active else ['before', 'after', 'exit']
            )
            before, after = points[:2]
            if active:
                expected = (
                    'C division/remainder by zero is undefined'
                    if policy
                    else 'expression has no structured synthesis; truth tables are disabled'
                )
                assert '|c> runtime: ' + expected in log, log[-2000:]
            densities = []
            for point in (before, after):
                assert point['measurements'] == point['resets'] == 0
                registers = [
                    next(r for r in point['registers'] if r['name'] == name)
                    for name in ('y', 'active')
                ]
                densities.append(DENSITY['actual_density'](point, registers))
            error = max(
                abs(densities[0].get(k, 0j) - densities[1].get(k, 0j))
                for k in densities[0].keys() | densities[1].keys()
            )
            assert error < 1e-10, ('string preflight mutated public state', error)
            preflights += 1
        print('PASS', binary.name, flush=True)
    summary = dict(
        status='passed',
        compiled_programs=programs,
        public_density_comparisons=states,
        joint_gate_state_comparisons=states,
        complete_gate_inverses=inverses,
        types=args.types,
        profiles=args.profiles,
        levels=args.levels,
        synthesis_policies=[0, 1],
        cpu_branches=[0, 1],
        sanitized=args.sanitized,
        exclusions=exclusions,
        runtime_domain_preflights=preflights,
        scope='Finite fixed-word strcpy/strncpy/strcat/strncat channels with quantum terminators, padding, empty appends, '
        'open/nested controls, CPU branches and scoped old-value phases. Root histories include selected old bits '
        'and necessary destination-dependent masks; certified producers clean their histories. '
        'Signed/unsigned 16/32-bit copies and empty appends; nonempty concatenations and scoped borrowers through 16 bits. '
        'Copy borrowers keep a full old word; root concatenation borrowers keep two Boolean bit views. '
        'Excluded larger fixtures exceed the 128-wire/workspace budget. '
        'No all-string, arbitrary-state, general-lifetime, full C11 or QPU qualification.',
    )
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary), flush=True)


if __name__ == '__main__':
    main()
