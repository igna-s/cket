#!/usr/bin/env python3
"""Finite coherent caller, CPU exit, inactive operand and gate-inverse contracts."""
from __future__ import annotations

import argparse
import cmath
import json
import os
from pathlib import Path
import runpy
import subprocess

ROOT = Path(__file__).resolve().parent.parent
REFERENCE = runpy.run_path(str(ROOT / 'scripts/validate-ownership.py'))
PROFILES = (
    'if(!n)return;x[0]^=true;if(x[0])phase(.17);if(n==2)return;x[0]^=true;phase(.23);',
    'if(n){if(n==2)return;if(x[0])phase(.11);}else{if(x[0])phase(.07);}if(!x[0])phase(.19);',
    'inner(x,n);if(x[0])phase(.23);if(n==2)return;if(!x[0])phase(.31);',
    'if(n==3){if(x[0])phase(.11);}if(!n)return;if(x[0])phase(.17);',
    'rotate(x,n*.13);if(n==2)return;if(x[0])phase(.17);',
    'unsigned amount=n+1;int direction=n==2?-1:1;if(!n)return;'
    'if(amount&1)x[0]^=true;if(x[0])phase(.09*direction);if(n==3)return;'
    'if((amount>>1)&1)x[0]^=true;phase(.13);',
    'unsigned local=n+1;if(n==2)return;'
    '{unsigned local=n+2;if(x[0])phase(.03*local);}if(!x[0])phase(.05*local);',
    'bool stop=n==2;unsigned shift=n%2;if(stop)return;'
    'if((5u>>shift)&1)x[0]^=true;if(x[0])phase(.17);',
)


def source(profile):
    return (
        '#include <quantum.h>\n#include <stdlib.h>\n'
        'void ownership_checkpoint(const char*);\n'
        'static void inner(quant const bool*x,unsigned n){if(!n)return;if(x[0])phase(.17);}\n'
        'static void rotate(quant const bool*x,double theta){if(!theta)return;if(x[0])phase(theta);}\n'
        'static void update(quant '
        + ('' if profile in (0, 5, 7) else 'const ')
        + 'bool*x,unsigned n){'
        + PROFILES[profile]
        + '}\n'
        'int main(int argc,char**argv){unsigned n=argc>1?(unsigned)strtoul(argv[1],0,10):0;'
        'quant bool x=superposition(0,1),control=superposition(0,1);'
        'if(x)phase(.37);if(control)phase(-.19);ownership_checkpoint("before");'
        'if(control)update(&x,n);if(x)phase(.41);ownership_checkpoint("after");'
        'measure(x);measure(control);return 0;}\n'
    )


def effect(profile, n, value):
    if profile == 0:
        return (value ^ (n == 2), 0.17 * (not value) + (0.23 if n != 2 else 0)) if n else (value, 0)
    if profile == 1:
        return value, 0 if n == 2 else (0.11 if n else 0.07) * value + 0.19 * (not value)
    if profile == 2:
        return value, (0.23 + 0.17 * (n != 0)) * value + 0.31 * (n != 2) * (not value)
    if profile == 3:
        return value, (0.11 * (n == 3) + 0.17 * (n != 0)) * value
    if profile == 4:
        return value, (n * 0.13 + 0.17 * (n != 2)) * value
    if profile == 5:
        if not n:
            return value, 0
        middle = value ^ ((n + 1) & 1)
        angle = 0.09 * (-1 if n == 2 else 1) * middle
        return (middle, angle) if n == 3 else (middle ^ (((n + 1) >> 1) & 1), angle + 0.13)
    if profile == 6:
        return value, 0 if n == 2 else 0.03 * (n + 2) * value + 0.05 * (n + 1) * (not value)
    current = value if n == 2 else value ^ ((5 >> (n % 2)) & 1)
    return current, 0 if n == 2 else 0.17 * current


def check(points, profile, n):
    assert [p['tag'] for p in points] == ['before', 'after']
    state, gates = {0: 1 + 0j}, []
    for point in points:
        assert point['measurements'] == point['resets'] == 0
        names = {r['name']: r for r in point['registers'] if r['name']}
        xwire, control = names['x']['wires'][0], names['control']['wires'][0]
        assert xwire != control
        expected = {}
        for initial in (0, 1):
            for enabled in (0, 1):
                current, angle = initial, 0.37 * initial - 0.19 * enabled
                if point['tag'] == 'after':
                    if enabled:
                        current, extra = effect(profile, n, initial)
                        angle += extra
                    angle += 0.41 * current
                expected[(current << xwire) | (enabled << control)] = 0.5 * cmath.exp(1j * angle)
        actual = {int(b, 0): complex(real, imag) for b, real, imag in point['state']}
        state = REFERENCE['evolve'](state, point['gates'])
        REFERENCE['equal_states'](actual, state)
        REFERENCE['equal_states'](actual, expected)
        gates.extend(point['gates'])
    inverse = [
        [k, w, mask, pattern, -angle if k == 2 else angle]
        for k, w, mask, pattern, angle in reversed(gates)
    ]
    REFERENCE['equal_states'](REFERENCE['evolve'](state, inverse), {0: 1 + 0j})


def boundaries():
    return (
        (
            'inactive-argument',
            'void update(quant bool x,int n){x^=n;if(n)return;x^=true;}',
            'int k=argc>1?argv[1][0]-\'0\':0;quant bool c=false,x=false;'
            'if(c)update(x,10/k);return measure(x)||measure(c);',
            None,
        ),
        (
            'inactive-predicate',
            'void update(quant bool x,int n){if(10/n)return;x^=true;}',
            'int n=argc>1?argv[1][0]-\'0\':0;quant bool c=false,x=false;'
            'if(c)update(x,n);return measure(x)||measure(c);',
            None,
        ),
        (
            'inactive-load',
            'void update(quant bool x,const int*p){if(*p)return;x^=true;}',
            'const int*p=0;quant bool c=false,x=false;if(c)update(x,p);'
            'return measure(x)||measure(c);',
            None,
        ),
        (
            'short-circuit',
            'void update(quant bool x,const int*p,int n){if(n&&*p)return;x^=true;}',
            'const int*p=0;int n=argc>1?argv[1][0]-\'0\':0;'
            'quant bool c=superposition(0,1),x=false;if(c)update(x,p,n);'
            'unsigned a=measure(x),b=measure(c);return a!=b;',
            None,
        ),
        (
            'inactive-index',
            'void update(quant bool x,int n){if(n)return;x^=true;}',
            'int data[1]={1};unsigned index=(unsigned)argc;quant bool c=false,x=false;'
            'if(c)update(x,data[index]);return measure(x)||measure(c);',
            None,
        ),
        (
            'inactive-local-division',
            'void update(quant bool x,int n){int local=10/n;if(local)return;x^=true;}',
            'int n=argc>1?argv[1][0]-\'0\':0;quant bool c=false,x=false;'
            'if(c)update(x,n);return measure(x)||measure(c);',
            None,
        ),
        (
            'inactive-local-load',
            'void update(quant bool x,const int*p){int local=*p;if(local)return;x^=true;}',
            'const int*p=0;quant bool c=false,x=false;if(c)update(x,p);'
            'return measure(x)||measure(c);',
            None,
        ),
        (
            'argument-effect',
            'void update(quant bool x,int n){if(n)return;x^=true;}',
            'int n=0;quant bool c=superposition(0,1),x=false;if(c)update(x,n++);'
            'measure(x);measure(c);return 0;',
            'classical function arguments under coherent control must be pure',
        ),
        (
            'public-effect',
            'static int visits;void update(quant bool x,int n){if(n)return;++visits;x^=true;}',
            'int n=argc;quant bool c=superposition(0,1),x=false;if(c)update(x,n);'
            'measure(x);measure(c);return 0;',
            'classical side effects cannot execute inside coherent quantum control',
        ),
        (
            'mutable-user-local',
            'void update(quant bool x,int n){int local=1;++local;if(n)return;x^=local;}',
            'int n=argc;quant bool c=superposition(0,1),x=false;if(c)update(x,n);'
            'measure(x);measure(c);return 0;',
            'classical storage cannot be created coherently',
        ),
        (
            'addressed-user-local',
            'void update(quant bool x,int n){int local=1;int*p=&local;if(n)return;x^=*p;}',
            'quant bool c=superposition(0,1),x=false;if(c)update(x,argc);'
            'measure(x);measure(c);return 0;',
            'classical storage cannot be created coherently',
        ),
        (
            'volatile-user-local',
            'void update(quant bool x,int n){volatile int local=1;if(n)return;x^=local;}',
            'quant bool c=superposition(0,1),x=false;if(c)update(x,argc);'
            'measure(x);measure(c);return 0;',
            'classical storage cannot be created coherently',
        ),
        (
            'quantum-user-local',
            'void update(quant bool x,int n){int local=x;if(n)return;x^=local;}',
            'quant bool c=superposition(0,1),x=false;if(c)update(x,argc);'
            'measure(x);measure(c);return 0;',
            'classical initialization cannot read a quantum value',
        ),
        (
            'quantum-exit',
            'void update(quant bool x,int n){if(x)return;x^=true;}',
            'quant bool c=superposition(0,1),x=false;if(c)update(x,argc);measure(x);'
            'measure(c);return 0;',
            'unmeasured quantum control',
        ),
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler-root', type=Path, default=ROOT)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--levels', nargs='+', default=['O0', 'O1', 'O2', 'O3', 'Os'])
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

    def run(label, command, environment=env, diagnostic=None):
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
        commands.append(
            dict(
                label=label,
                command=command,
                exit_code=z.returncode,
                expected_exit_code=1 if diagnostic else 0,
            )
        )
        (output / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
        assert z.returncode == (1 if diagnostic else 0), z.stdout
        assert not diagnostic or diagnostic in z.stdout, z.stdout
        return z.stdout

    adapter = output / 'trace.o'
    run(
        'trace-build',
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
        '-Wl,--wrap=' + n
        for n in (
            'qc_create_sparse',
            '__cket_apply',
            'qc_apply',
            'qc_apply_bound',
            '__cket_read_byte',
        )
    ]
    checkpoints = inverses = native = rejected = 0
    for level in args.levels:
        for profile in range(len(PROFILES)):
            path, binary = output / f'profile-{profile}.c', output / f'profile-{profile}-{level}'
            path.write_text(source(profile))
            run(
                f'profile-{profile}-{level}-compile',
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
            for n in range(5):
                for tables in (0, 1):
                    text = run(
                        f'profile-{profile}-{level}-n{n}-tables{tables}',
                        [str(binary), str(n)],
                        dict(env, OWNERSHIP_TABLES=str(tables)),
                    )
                    points = [json.loads(s) for s in text.splitlines() if s.startswith('{')]
                    check(points, profile, n)
                    checkpoints += len(points)
                    inverses += 1
        for name, definition, body, diagnostic in boundaries():
            path, binary = output / (name + '.c'), output / (name + '-' + level)
            path.write_text(
                '#include <quantum.h>\n'
                + definition
                + '\nint main(int argc,char**argv){'
                + body
                + '}\n'
            )
            run(
                name + '-' + level + '-compile',
                [str(target / 'build/cket'), str(path), '-' + level, *flags, '-o', str(binary)],
                diagnostic=diagnostic,
            )
            if diagnostic:
                rejected += 1
            else:
                run(name + '-' + level, [str(binary)])
                native += 1
    summary = dict(
        status='passed',
        levels=args.levels,
        sanitized=args.sanitized,
        phase_profiles=len(PROFILES),
        full_state_checkpoints=checkpoints,
        complete_gate_inverses=inverses,
        native_boundary_executions=native,
        compile_rejections=rejected,
        scope='Pure CPU early exits composed with coherent callers; four-label states, relative/global controlled phases, X, nested calls and private-zero cleanup. Arbitrary CFG, quantum-controlled exits, coherent loops and user local lifetimes remain pending.',
    )
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary))
    print(
        'Coherent caller CPU returns passed; finite state, inverse and inactive-operand scopes recorded in summary.json.'
    )


if __name__ == '__main__':
    main()
