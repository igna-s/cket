#!/usr/bin/env python3
"""Finite C results, phases, cleanup and inverses for structured CPU procedure exits."""
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
NATIVE = (
    'if(n==0)return;x[0]^=true;++*visits;',
    'if(n){if(n==2)return;x[0]^=true;}else{x[0]^=true;}x[0]^=true;++*visits;',
    'for(unsigned i=0;i<n;++i,++*visits){if(i==2)return;x[0]^=true;}x[0]^=true;',
    'unsigned i=0;while(i++<n){if(i==3)return;x[0]^=true;++*visits;}x[0]^=true;',
    'unsigned i=0;do{if(i++==n)return;x[0]^=true;++*visits;}while(i<4);x[0]^=true;',
    'if(n==1){++*visits;return;}x[0]^=true;if(n==2)return;++*visits;x[0]^=true;',
    'x[0]^=true;{if(n==2)return;x[0]^=true;}++*visits;',
    'for(unsigned i=0;;++i,++*visits){if(i==n)return;x[0]^=true;}',
    'inner(x,n,visits);x[0]^=true;++*visits;if(n==2)return;x[0]^=true;',
    'if((*visits)++==n)return;x[0]^=true;',
    'if(n)x[0]^=true;else x[0]^=true;',
    'unsigned i=0;while((++*visits,i++<n)){if(i==2)return;x[0]^=true;}',
    'unsigned i=0;do{if(i++==n)return;x[0]^=true;}while((++*visits,i<4));x[0]^=true;',
    'for(unsigned i=0;i<4;++i){inner(x,i,visits);if(i==n)return;}x[0]^=true;',
    'for(unsigned i=0;i<4;++i,++*visits){if(i==n)break;x[0]^=true;}x[0]^=true;',
    'for(unsigned i=0;i<4;++i,++*visits){if(i==n)continue;x[0]^=true;++*visits;}x[0]^=true;',
    'unsigned i=0;while((++*visits,i++<4)){if(i==n)break;x[0]^=true;}x[0]^=true;',
    'unsigned i=0;do{++i;if(i==n)continue;x[0]^=true;++*visits;}'
    'while((++*visits,i<4));x[0]^=true;',
    'for(unsigned i=0;i<3;++i){for(unsigned j=0;j<4;++j){'
    'if(j==n)break;if(i==n)continue;x[0]^=true;++*visits;}++*visits;}',
    'for(unsigned i=0;i<4;++i){inner(x,i,visits);if(i==n)continue;'
    'if(i==2)break;x[0]^=true;}++*visits;',
    'for(unsigned i=0;i<4;++i){if(i==n)continue;unsigned snapshot=++*visits;'
    'x[0]^=snapshot&1;}x[0]^=true;',
    'for(unsigned i=0;i<4;++i){if(i==n){break;}x[0]^=true;++*visits;}x[0]^=true;',
    'switch(n){case 0:return;case 1:case 2:x[0]^=true;break;default:++*visits;break;}'
    'x[0]^=true;++*visits;',
    'for(unsigned i=0;i<4;++i){switch(i){case 1:continue;case 2:if(n==2)return;break;'
    'default:break;}x[0]^=true;++*visits;}x[0]^=true;',
)
PHASE = (
    'quant bool scratch=!x[0];if(scratch)phase(.17);if(n==0)return;if(scratch)phase(.23);',
    'for(unsigned i=0;i<4;++i){if(i==n)return;if(x[0])phase(.13);}if(x[0])phase(.29);',
    'if(n){if(n==2)return;if(x[0])phase(.11);}else{if(x[0])phase(.07);}if(!x[0])phase(.19);',
    'inner(x,n);if(x[0])phase(.23);if(n==2)return;if(!x[0])phase(.31);',
    'for(unsigned i=0;i<4;++i){if(i==n)return;x[0]^=true;if(x[0])phase(.13);}if(x[0])phase(.29);',
    'for(unsigned i=0;i<4;++i){if(i==n)continue;if(x[0])phase(.07*(i+1));}' 'if(!x[0])phase(.13);',
    'for(unsigned i=0;i<4;++i){if(i==n)break;if(x[0])phase(.09*(i+1));}' 'if(!x[0])phase(.17);',
    'unsigned i=0;while(i<4){++i;if(i==n)continue;if(i==3)break;'
    'if(!x[0])phase(.11*i);}if(x[0])phase(.19);',
    'unsigned i=0;do{++i;if(i==n)continue;if(i==3)break;'
    'if(x[0])phase(.05*i);}while(i<4);if(!x[0])phase(.23);',
    'for(unsigned i=0;i<3;++i){for(unsigned j=0;j<3;++j){'
    'if(j==n)break;if(i==n)continue;if(x[0])phase(.03*(i*3+j+1));}}'
    'if(!x[0])phase(.29);',
    'for(unsigned i=0;i<4;++i){switch(i==n?0:i==3?1:2){case 0:continue;case 1:return;'
    'default:break;}if(x[0])phase(.07*(i+1));}if(!x[0])phase(.19);',
)


def native_source(profile):
    body = NATIVE[profile]
    cpu = body.replace('inner(', 'cpu_inner(')
    initial = 'false' if profile == 10 else 'input'
    return (
        '#include <quantum.h>\n#include <stdlib.h>\n'
        'static void cpu_inner(bool*x,unsigned n,unsigned*v){if(!n)return;x[0]^=true;++*v;}\n'
        'static void inner(quant bool*x,unsigned n,unsigned*v){if(!n)return;x[0]^=true;++*v;}\n'
        'static void reference(bool*x,unsigned n,unsigned*visits){' + cpu + '}\n'
        'static void update(quant bool*x,unsigned n,unsigned*visits){' + body + '}\n'
        'int main(int argc,char**argv){if(argc!=3)return 2;'
        'unsigned n=(unsigned)strtoul(argv[1],0,10);bool input=argv[2][0]==\'1\';'
        'bool expected=' + initial + ';unsigned a=0,b=0;reference(&expected,n,&a);'
        'quant bool x=' + initial + ';update(&x,n,&b);x^=true;expected^=true;'
        'return measure(x)!=expected||a!=b;}\n'
    )


def phase_source(profile):
    return (
        '#include <quantum.h>\n#include <stdlib.h>\n'
        'void ownership_checkpoint(const char*);\n'
        'static void inner(quant const bool*x,unsigned n){if(!n)return;if(x[0])phase(.17);}\n'
        'static void update(quant '
        + ('' if profile == 4 else 'const ')
        + 'bool*x,unsigned n){'
        + PHASE[profile]
        + '}\n'
        'int main(int argc,char**argv){unsigned n=argc>1?(unsigned)strtoul(argv[1],0,10):0;'
        'quant bool x=superposition(0,1),spectator=superposition(0,1);'
        'if(x)phase(.37);if(spectator)phase(-.19);ownership_checkpoint("before");'
        'update(&x,n);if(x)phase(.41);ownership_checkpoint("after");'
        'measure(x);measure(spectator);return 0;}\n'
    )


def phase_expected(profile, n, x):
    if profile == 0:
        return (0.17 + 0.23 * (n != 0)) * (not x)
    if profile == 1:
        return (0.13 * min(n, 4) + 0.29 * (n >= 4)) * x
    if profile == 2:
        return 0 if n == 2 else (0.11 if n else 0.07) * x + 0.19 * (not x)
    if profile == 3:
        return (0.23 + 0.17 * (n != 0)) * x + 0.31 * (n != 2) * (not x)
    if profile == 4:
        angle = 0
        for _ in range(min(n, 4)):
            x ^= 1
            angle += 0.13 * x
        return angle + 0.29 * (n >= 4) * x
    if profile == 5:
        return sum(0.07 * (i + 1) for i in range(4) if i != n) * x + 0.13 * (not x)
    if profile == 6:
        return 0.09 * sum(range(1, min(n, 4) + 1)) * x + 0.17 * (not x)
    if profile in (7, 8):
        weight = 0
        for i in range(1, 5):
            if i == n:
                continue
            if i == 3:
                break
            weight += i
        return (
            0.11 * weight * (not x) + 0.19 * x
            if profile == 7
            else 0.05 * weight * x + 0.23 * (not x)
        )
    if profile == 10:
        angle = 0
        for i in range(4):
            if i == n:
                continue
            if i == 3:
                return angle
            angle += 0.07 * (i + 1) * x
        return angle + 0.19 * (not x)
    angle = 0.29 * (not x)
    for i in range(3):
        for j in range(3):
            if j == n:
                break
            if i == n:
                continue
            angle += 0.03 * (i * 3 + j + 1) * x
    return angle


def check(points, profile, n):
    assert [p['tag'] for p in points] == ['before', 'after']
    state, gates = {0: 1 + 0j}, []
    for point in points:
        assert point['measurements'] == point['resets'] == 0
        names = {r['name']: r for r in point['registers'] if r['name']}
        xwire, spectator = names['x']['wires'][0], names['spectator']['wires'][0]
        assert xwire != spectator
        expected = {}
        for x in (0, 1):
            for other in (0, 1):
                angle = 0.37 * x - 0.19 * other
                current = x
                if point['tag'] == 'after':
                    if profile == 4:
                        current ^= min(n, 4) & 1
                    angle += phase_expected(profile, n, x) + 0.41 * current
                expected[(current << xwire) | (other << spectator)] = 0.5 * cmath.exp(1j * angle)
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
        commands.append(dict(label=label, command=command, exit_code=result.returncode))
        (output / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
        if diagnostic:
            assert result.returncode == 1 and diagnostic in result.stdout, result.stdout
        else:
            assert result.returncode == 0, result.stdout
        return result.stdout

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
    native = checkpoints = inverses = rejected = 0
    negative = (
        (
            'void update(quant const bool x){if(x)return;phase(.1);}',
            'quant bool x=false;update(x);',
            'unmeasured quantum control',
        ),
        (
            'void update(quant const bool*x){if(x[0])return;phase(.1);}',
            'quant bool x=false;update(&x);',
            'unmeasured quantum control',
        ),
        (
            'void update(quant bool x,unsigned n){switch(n){case 0:x^=true;default:return;}x^=true;}',
            'quant bool x=false;update(x,0);',
            'early or conditional returns in quantum procedures',
        ),
        (
            'quant bool update(quant const bool x,unsigned n){quant bool t=!x;switch(n){case 0:t^=true;default:return t;}return t;}',
            'volatile unsigned n=0;quant bool x=false;quant bool y=update(x,n);measure(y);measure(x);',
            'early or conditional returns in quantum procedures',
        ),
    )
    for level in args.levels:
        for profile in range(len(NATIVE)):
            path, binary = output / f'native-{profile}.c', output / f'native-{profile}-{level}'
            path.write_text(native_source(profile))
            run(
                f'native-{profile}-{level}-compile',
                [str(target / 'build/cket'), str(path), '-' + level, *flags, '-o', str(binary)],
            )
            for n in range(5):
                for initial in (0, 1):
                    run(
                        f'native-{profile}-{level}-n{n}-input{initial}',
                        [str(binary), str(n), str(initial)],
                    )
                    native += 1
        for profile in range(len(PHASE)):
            path, binary = output / f'phase-{profile}.c', output / f'phase-{profile}-{level}'
            path.write_text(phase_source(profile))
            run(
                f'phase-{profile}-{level}-compile',
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
                        f'phase-{profile}-{level}-n{n}-tables{tables}',
                        [str(binary), str(n)],
                        dict(env, OWNERSHIP_TABLES=str(tables)),
                    )
                    points = [
                        json.loads(line) for line in text.splitlines() if line.startswith('{')
                    ]
                    check(points, profile, n)
                    checkpoints += len(points)
                    inverses += 1
        for i, (definition, call, diagnostic) in enumerate(negative):
            path = output / f'rejected-{i}.c'
            path.write_text(
                '#include <quantum.h>\n' + definition + '\nint main(void){' + call + 'return 0;}\n'
            )
            run(
                f'rejected-{i}-{level}',
                [
                    str(target / 'build/cket'),
                    str(path),
                    '-' + level,
                    '--emit-ir',
                    '-o',
                    str(output / 'rejected.json'),
                ],
                diagnostic=diagnostic,
            )
            rejected += 1
    summary = dict(
        status='passed',
        levels=args.levels,
        sanitized=args.sanitized,
        native_executions=native,
        full_state_checkpoints=checkpoints,
        complete_gate_inverses=inverses,
        compile_rejections=rejected,
        native_profiles=len(NATIVE),
        phase_profiles=len(PHASE),
        scope='Structured CPU returns in void procedures; finite Boolean C results, CPU effects, four-label complex states and private cleanup. General CFG, owned early results and quantum-controlled exits remain pending.',
    )
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary))
    print(
        'Structured CPU procedure returns passed; finite native/state/inverse scopes recorded in summary.json.'
    )


if __name__ == '__main__':
    main()
