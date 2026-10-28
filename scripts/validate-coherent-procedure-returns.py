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
    'int sum=(int)n;sum+=2;if(n&1)++sum;else sum-=1;if(sum==4)return;'
    'if(x[0])phase(.03*sum);--sum;if(!x[0])phase(.07*sum);',
    'for(unsigned i=0;i<n;++i){if(i==2)return;x[0]^=true;'
    'if(x[0])phase(.11*(i+1));}if(!x[0])phase(.17);',
    'unsigned k=n;while(k){if(k==2)return;if(x[0])phase(.09*k);--k;x[0]^=true;}'
    'if(!x[0])phase(.13);',
    'unsigned i=0;do{if(i==n)return;if(x[0])phase(.07*(i+1));++i;x[0]^=true;}'
    'while(i<3);if(!x[0])phase(.17);',
    'unsigned total=0;for(unsigned i=0;i<n;++i){unsigned limit=i+1;'
    'for(unsigned j=0;j<limit;++j){++total;if(x[0])phase(.03*total);}}'
    'if(!x[0])phase(.05*total);',
    'for(unsigned i=0;i<n;++i){if(x[0]){unsigned offset=i+1;++offset;phase(.04*offset);}}',
    'for(unsigned i=0;i<4;++i){if(i==n)continue;if(i==2)break;'
    'x[0]^=true;if(x[0])phase(.07*(i+1));}if(x[0])phase(.13);',
    'unsigned i=0;while(i<4){++i;if(i==n)continue;if(i==3)break;'
    'if(x[0])phase(.09*i);x[0]^=true;}if(!x[0])phase(.17);',
    'unsigned i=0;do{++i;if(i==n)continue;if(i==3)break;'
    'x[0]^=true;if(x[0])phase(.05*i);}while(i<4);if(x[0])phase(.19);',
    'for(unsigned i=0;i<3;++i){for(unsigned j=0;j<3;++j){'
    'if(j==n)break;if(i==n)continue;if(x[0])phase(.03*(i*3+j+1));}}'
    'if(!x[0])phase(.11);',
    'for(unsigned i=0;i<4;++i){inner(x,i);{if(i==n)continue;if(i==2)break;}'
    'if(x[0])phase(.13);}if(!x[0])phase(.17);',
    'for(unsigned i=0;i<4;++i){switch(i==n?0:i==2?1:2){case 0:continue;case 1:return;'
    'default:break;}x[0]^=true;if(x[0])phase(.07*(i+1));}if(x[0])phase(.13);',
    'if(n==1)return;switch(x[0]){case false:phase(.09*(n+1));break;'
    'default:phase(.11*(n+1));break;}if(n==2)return;if(!x[0])phase(.17);',
)


def source(profile):
    return (
        '#include <quantum.h>\n#include <stdlib.h>\n'
        'void ownership_checkpoint(const char*);\n'
        'static void inner(quant const bool*x,unsigned n){if(!n)return;if(x[0])phase(.17);}\n'
        'static void rotate(quant const bool*x,double theta){if(!theta)return;if(x[0])phase(theta);}\n'
        'static void update(quant '
        + ('' if profile in (0, 5, 7, 9, 10, 11, 14, 15, 16, 19) else 'const ')
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
    if profile == 7:
        current = value if n == 2 else value ^ ((5 >> (n % 2)) & 1)
        return current, 0 if n == 2 else 0.17 * current
    if profile == 8:
        total = n + (3 if n & 1 else 1)
        return value, 0 if total == 4 else 0.03 * total * value + 0.07 * (total - 1) * (not value)
    if profile == 9:
        current, angle = value, 0
        for i in range(n):
            if i == 2:
                return current, angle
            current ^= 1
            angle += 0.11 * (i + 1) * current
        return current, angle + 0.17 * (not current)
    if profile == 10:
        current, angle = value, 0
        for k in range(n, 0, -1):
            if k == 2:
                return current, angle
            angle += 0.09 * k * current
            current ^= 1
        return current, angle + 0.13 * (not current)
    if profile == 11:
        current, angle = value, 0
        for i in range(3):
            if i == n:
                return current, angle
            angle += 0.07 * (i + 1) * current
            current ^= 1
        return current, angle + 0.17 * (not current)
    if profile == 12:
        total = n * (n + 1) // 2
        return value, 0.03 * total * (total + 1) / 2 * value + 0.05 * total * (not value)
    if profile == 13:
        return value, 0.04 * n * (n + 3) / 2 * value
    if profile == 14:
        current, angle = value, 0
        for i in range(4):
            if i == n:
                continue
            if i == 2:
                break
            current ^= 1
            angle += 0.07 * (i + 1) * current
        return current, angle + 0.13 * current
    if profile in (15, 16):
        current, angle = value, 0
        for i in range(1, 5):
            if i == n:
                continue
            if i == 3:
                break
            if profile == 15:
                angle += 0.09 * i * current
                current ^= 1
            else:
                current ^= 1
                angle += 0.05 * i * current
        return current, angle + (0.17 * (not current) if profile == 15 else 0.19 * current)
    if profile == 17:
        angle = 0.11 * (not value)
        for i in range(3):
            for j in range(3):
                if j == n:
                    break
                if i == n:
                    continue
                angle += 0.03 * (i * 3 + j + 1) * value
        return value, angle
    if profile == 18:
        angle = 0.17 * (not value)
        for i in range(4):
            angle += 0.17 * (i != 0) * value
            if i == n:
                continue
            if i == 2:
                break
            angle += 0.13 * value
        return value, angle
    if profile == 19:
        current, angle = value, 0
        for i in range(4):
            if i == n:
                continue
            if i == 2:
                return current, angle
            current ^= 1
            angle += 0.07 * (i + 1) * current
        return current, angle + 0.13 * current
    if not value:
        return value, 0 if n == 1 else (0.09 * (n + 1) + (0 if n == 2 else 0.17))
    return value, 0 if n == 1 else 0.11 * (n + 1)


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
            None,
        ),
        (
            'quantum-break',
            'void update(quant bool x,int n){for(unsigned i=0;i<4;++i){if(x)break;x^=true;}}',
            'quant bool c=superposition(0,1),x=false;if(c)update(x,argc);'
            'measure(x);measure(c);return 0;',
            'a loop jump cannot depend on unmeasured quantum control',
        ),
        (
            'quantum-continue',
            'void update(quant bool x,int n){for(unsigned i=0;i<4;++i){if(x)continue;x^=true;}}',
            'quant bool c=superposition(0,1),x=false;if(c)update(x,argc);'
            'measure(x);measure(c);return 0;',
            'a loop jump cannot depend on unmeasured quantum control',
        ),
        (
            'switch-continue',
            'void update(quant bool x,int n){for(unsigned i=0;i<4;++i){'
            'switch(n){case 0:x^=true;default:continue;}x^=true;}}',
            'quant bool c=superposition(0,1),x=false;if(c)update(x,argc);'
            'measure(x);measure(c);return 0;',
            'loop continue through switch needs CFG lifetime analysis',
        ),
        (
            'fallthrough-switch',
            'void update(quant bool x,int n){switch(n){case 0:x^=true;default:x^=true;break;}}',
            'quant bool c=superposition(0,1),x=false;if(c)update(x,argc);'
            'measure(x);measure(c);return 0;',
            'switch with quantum effects requires structured cases ending in break, return or continue without fallthrough',
        ),
        (
            'coherent-quantum-switch-and-guard-values',
            'quant uint8_t guard_pick(quant uint8_t a,quant uint8_t b,quant uint8_t m){'
            'if(m==0)return a+1;if(m==1)return b^3;return a^b;}'
            'quant uint8_t switch_pick(quant uint8_t a,quant uint8_t b,quant uint8_t m){'
            'switch(m){case 0:return a+2;case 1 ... 2:return b^5;}return a+b;}',
            'quant uint8_t a=3,b=6,m0=0,m1=2,m2=3;'
            'quant uint8_t g0=guard_pick(a,b,m0),g1=guard_pick(a,b,m2);'
            'quant uint8_t s0=switch_pick(a,b,m0),s1=switch_pick(a,b,m1),s2=switch_pick(a,b,m2);'
            'quant uint8_t x=0;switch(m1){case 0:x^=1;break;case 2:x^=9;break;default:x^=3;break;}'
            'unsigned rg0=measure(g0),rg1=measure(g1),rs0=measure(s0),rs1=measure(s1),rs2=measure(s2),rx=measure(x);'
            'measure(a);measure(b);measure(m0);measure(m1);measure(m2);'
            'return rg0!=4||rg1!=5||rs0!=5||rs1!=3||rs2!=9||rx!=9;',
            None,
        ),
        (
            'inactive-runtime-memory',
            '#include <string.h>\nvoid update(quant uint8_t*a,size_t n){'
            'for(unsigned i=0;i<2;++i){if(i==1)break;memcpy((void*)(a+2),(const void*)a,n);}}',
            'size_t n=99;quant bool c=false;quant uint8_t a[4]={3,5,7,11};'
            'if(c)update(a,n);unsigned r0=measure(a[0]),r1=measure(a[1]),r2=measure(a[2]),r3=measure(a[3]),rc=measure(c);'
            'return r0!=3||r1!=5||r2!=7||r3!=11||rc!=0;',
            None,
        ),
        (
            'inactive-do-jump',
            'void update(quant bool x,int n){unsigned i=0;do{'
            'if(10/n)continue;++i;x^=true;}while(i<4);}',
            'int n=argc>1?argv[1][0]-\'0\':0;quant bool c=false,x=false;'
            'if(c)update(x,n);return measure(x)||measure(c);',
            None,
        ),
        (
            'indirect-cpu-effect',
            'static void cpu(void){}static void(*callback)(void)=cpu;'
            'void update(quant bool x,int n){callback();if(n)return;x^=true;}',
            'quant bool c=superposition(0,1),x=false;if(c)update(x,argc);'
            'measure(x);measure(c);return 0;',
            'classical calls and I/O cannot execute coherently',
        ),
        (
            'quantum-dependent-local-write',
            'void update(quant bool x,int n){int local=1;if(x)++local;'
            'if(n)return;phase(.1*local);}',
            'quant bool c=superposition(0,1),x=false;if(c)update(x,argc);'
            'measure(x);measure(c);return 0;',
            'classical storage cannot be created coherently',
        ),
        (
            'effectful-loop-condition',
            'void update(quant bool x,int n){int local=0;while(local++<n)x^=true;}',
            'quant bool c=superposition(0,1),x=false;if(c)update(x,argc);'
            'measure(x);measure(c);return 0;',
            'classical storage cannot be created coherently',
        ),
        (
            'inactive-for-local',
            'void update(quant bool x,int n){int limit=10/n;'
            'for(unsigned i=0;i<(unsigned)limit;++i)x^=true;}',
            'int n=argc>1?argv[1][0]-\'0\':0;quant bool c=false,x=false;'
            'if(c)update(x,n);return measure(x)||measure(c);',
            None,
        ),
        (
            'inactive-do-update',
            'void update(quant bool x,int n){int local=1;' 'do{local=10/n;x^=true;}while(local);}',
            'int n=argc>1?argv[1][0]-\'0\':0;quant bool c=false,x=false;'
            'if(c)update(x,n);return measure(x)||measure(c);',
            None,
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
        scope='Pure CPU returns, break/continue, private integer locals and CPU for/while/do loops composed with coherent callers; four-label states, controlled phases, X, nearest-loop jumps, nested calls/loops, private-zero cleanup and inverses. Private writes stay within their declaration quantum branch. Arbitrary CFG, switch-crossing continue, quantum-dependent jumps/iteration/local writes, escaping addresses and general lifetimes remain pending.',
    )
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary))
    print(
        'Coherent caller CPU returns passed; finite state, inverse and inactive-operand scopes recorded in summary.json.'
    )


if __name__ == '__main__':
    main()
