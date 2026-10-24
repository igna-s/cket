#!/usr/bin/env python3
"""Finite native heap byte contracts, complex states and independent inverses."""
from __future__ import annotations

import argparse
import cmath
import hashlib
import json
import math
import os
from pathlib import Path
import runpy
import subprocess

ROOT = Path(__file__).resolve().parent.parent
REFERENCE = runpy.run_path(str(ROOT / 'scripts/validate-ownership.py'))
HEADER = '#include <quantum.h>\n#include <stdlib.h>\n#include <string.h>\nvoid ownership_checkpoint(const char *);\n'

# Each formula specifies ordinary integer values as functions of the input bit.
# The expected amplitudes do not consult engine value/clean certificates.
PROFILES = {
    'copy': (
        'uint8_t',
        [lambda c: 9 if c else 3, lambda c: 7],
        'uint8_t',
        [lambda c: 11, lambda c: 13],
        'memcpy((void*)q,(const void*)p,n);',
        [lambda c: 9 if c else 3, lambda c: 7],
        [lambda c: 9 if c else 3, lambda c: 7],
        2,
    ),
    'move_right': (
        'uint8_t',
        [lambda c: 9 if c else 3, lambda c: 5, lambda c: 7],
        None,
        [],
        'memmove((void*)(p+1),(const void*)p,n);',
        [lambda c: 9 if c else 3, lambda c: 9 if c else 3, lambda c: 5],
        [],
        2,
    ),
    'move_left': (
        'uint8_t',
        [lambda c: 7, lambda c: 9 if c else 3, lambda c: 5],
        None,
        [],
        'memmove((void*)p,(const void*)(p+1),n);',
        [lambda c: 9 if c else 3, lambda c: 5, lambda c: 5],
        [],
        2,
    ),
    'partial_words': (
        'uint16_t',
        [lambda c: 0x129A if c else 0x12BC],
        'uint16_t',
        [lambda c: 0xAAAA, lambda c: 0xBBBB],
        'memcpy((unsigned char*)q+1,(const void*)p,n);',
        [lambda c: 0x129A if c else 0x12BC],
        [lambda c: 0x9AAA if c else 0xBCAA, lambda c: 0xBB12],
        2,
    ),
    'mixed_widths': (
        'uint16_t',
        [lambda c: 0x129A if c else 0x12BC, lambda c: 0x5678],
        'uint8_t',
        [lambda c: 1, lambda c: 2, lambda c: 3],
        'memcpy((void*)q,(const void*)p,n);',
        [lambda c: 0x129A if c else 0x12BC, lambda c: 0x5678],
        [lambda c: 0x9A if c else 0xBC, lambda c: 0x12, lambda c: 0x78],
        3,
    ),
    'fill_alias': (
        'uint8_t',
        [lambda c: 5, lambda c: 9 if c else 3],
        None,
        [],
        'memset((void*)p,p[1],n);',
        [lambda c: 9 if c else 3, lambda c: 9 if c else 3],
        [],
        2,
    ),
    'controlled_copy': (
        'uint8_t',
        [lambda c: 9 if c else 3],
        'uint8_t',
        [lambda c: 5],
        'if(coin)memcpy((void*)q,(const void*)p,n);',
        [lambda c: 9 if c else 3],
        [lambda c: 9 if c else 5],
        1,
    ),
    'controlled_fill': (
        'uint16_t',
        [lambda c: 0x1234],
        None,
        [],
        'if(coin)memset((unsigned char*)p+1,coin?0xAB:0xCD,n);',
        [lambda c: 0xAB34 if c else 0x1234],
        [],
        1,
    ),
}

VALID = {
    'fixed_source': 'int main(void){quant uint8_t *p=calloc(2,1);if(!p)return 2;quant uint8_t v=7;memcpy((void*)p,&v,1);unsigned a=measure(p[0]),b=measure(v);free((void*)p);return a!=7||b!=7;}',
    'borrowed_function': '''void copy_bytes(quant uint8_t *out,quant const uint8_t *in,size_t n){memcpy((void*)out,(const void*)in,n);}int main(void){quant uint8_t *p=calloc(3,1),*q=calloc(3,1);if(!p||!q)return 2;p[0]=3;p[1]=9;p[2]=5;copy_bytes(q+1,p+1,2);unsigned a=measure(q[0]),b=measure(q[1]),c=measure(q[2]);free((void*)p);free((void*)q);return a!=0||b!=9||c!=5;}''',
    'return_comparison': '''int main(void){quant uint8_t *p=calloc(2,1),*q=calloc(2,1);if(!p||!q)return 2;p[0]=3;p[1]=9;if(memcpy((void*)q,(const void*)p,2)!=(void*)q)return 3;if(memset((void*)(q+1),7,1)!=(void*)(q+1))return 4;unsigned a=measure(q[0]),b=measure(q[1]);free((void*)p);free((void*)q);return a!=3||b!=7;}''',
    'return_alias': '''int main(void){quant uint8_t *p=calloc(2,1),*q=calloc(2,1);if(!p||!q)return 2;p[0]=3;p[1]=9;void *v=memcpy((void*)q,(const void*)p,2);unsigned char *b=v;memmove(b+1,b,1);v=memset(b,7,1);unsigned x=measure(q[0]),y=measure(q[1]);int bad=v!=(void*)q;free((void*)p);free((void*)q);return bad||x!=7||y!=3;}''',
    'runtime_loop': '''int main(void){quant uint8_t *p=calloc(2,1),*q=calloc(2,1);if(!p||!q)return 2;p[0]=3;p[1]=9;size_t rounds=3;for(size_t i=0;i<rounds;++i){size_t n=i%2+1;memcpy((void*)q,(const void*)p,n);memset((void*)p,(int)i+4,1);}unsigned x=measure(q[0]),y=measure(q[1]);free((void*)p);free((void*)q);return x!=5||y!=9;}''',
    'one_past_zero': '''int main(void){quant uint8_t *p=calloc(2,1);if(!p)return 2;p[0]=7;memcpy((void*)(p+2),(const void*)(p+2),0);memmove((void*)(p+2),(const void*)p,0);memset((void*)(p+2),8,0);unsigned x=measure(p[0]);free((void*)p);return x!=7;}''',
    'signed_partial': '''int main(void){quant int16_t *p=calloc(1,2);quant uint8_t *q=calloc(2,1);if(!p||!q)return 2;p[0]=-2;memcpy((void*)q,(const void*)p,2);memset((unsigned char*)p+1,0x12,1);int x=measure(p[0]);unsigned a=measure(q[0]),b=measure(q[1]);free((void*)p);free((void*)q);return x!=0x12FE||a!=254||b!=255;}''',
    'same_view_move': '''int main(void){quant uint16_t *p=calloc(2,2);if(!p)return 2;p[0]=1234;p[1]=5678;size_t n=4;memmove((void*)p,(const void*)p,n);unsigned a=measure(p[0]),b=measure(p[1]);free((void*)p);return a!=1234||b!=5678;}''',
    'inactive_null': '''int main(void){quant uint8_t *p=NULL;quant bool b=0;if(b)memset((void*)p,8,1);measure(b);return 0;}''',
}
INVALID = {
    'borrowed_return_free': (
        'void bad(quant uint8_t *p){void *v=memset((void*)p,7,1);free(v);}int main(void){quant uint8_t *p=calloc(1,1);bad(p);}',
        'cannot be freed',
    ),
    'copy_overlap': (
        'int main(void){quant uint8_t *p=calloc(3,1);memcpy((void*)(p+1),(const void*)p,2);}',
        'overlap',
    ),
    'copy_identical': (
        'int main(void){quant uint8_t *p=calloc(1,1);memcpy((void*)p,(const void*)p,1);}',
        'overlap',
    ),
    'source_bounds': (
        'int main(void){quant uint8_t *p=calloc(1,1),*q=calloc(2,1);size_t n=2;memcpy((void*)q,(const void*)p,n);}',
        'outside live storage',
    ),
    'destination_bounds': (
        'int main(void){quant uint8_t *p=calloc(2,1);size_t n=2;memset((void*)(p+1),7,n);}',
        'outside live storage',
    ),
    'one_past_nonzero': (
        'int main(void){quant uint8_t *p=calloc(2,1);memset((void*)(p+2),7,1);}',
        'outside live storage',
    ),
    'length_wrap': (
        'int main(void){quant uint8_t *p=calloc(2,1);size_t n=SIZE_MAX;memset((void*)p,7,n);}',
        'outside live storage',
    ),
    'boolean_layout': (
        'int main(void){quant bool *p=calloc(2,sizeof *p);memset((void*)p,0,1);}',
        'whole-byte',
    ),
    'readonly_cast': (
        'int main(void){quant uint8_t *p=calloc(2,1);const uint8_t *v=(const uint8_t*)p;memset((void*)v,0,1);}',
        'mutable destination',
    ),
    'quantum_length': (
        'int main(void){quant uint8_t *p=calloc(2,1);quant uint8_t n=1;memset((void*)p,7,n);}',
        'pure classical',
    ),
    'empty_fill_domain': (
        'int main(void){quant uint8_t *p=calloc(1,1);quant uint8_t x=0;memset((void*)p,1/x,0);measure(x);free((void*)p);}',
        'division',
    ),
}


def profile_source(profile):
    ptype, p, qtype, q, operation, after_p, after_q, count = PROFILES[profile]
    declarations = f'quant {ptype} *p=calloc({len(p)},sizeof *p);if(!p)return 2;'
    if qtype:
        declarations += f'quant {qtype} *q=calloc({len(q)},sizeof *q);if(!q)return 2;'
    preparation = ''.join(
        f'{name}[{i}]=coin?{value(1)}:{value(0)};'
        for name, values in [('p', p), ('q', q)]
        for i, value in enumerate(values)
    )
    disposal = 'free((void*)p);' + ('free((void*)q);' if q else '')
    condition = (p[0](1) & 255) ^ 1
    return (
        HEADER
        + f'''int main(void){{{declarations}
        quant bool coin=superposition(0,1);{preparation}
        if(coin)phase(.31);size_t n={count};
        ownership_checkpoint("prepared");
        quant uint8_t saved=(uint8_t)p[0]^1;
        {operation}ownership_checkpoint("written");
        {disposal}ownership_checkpoint("freed");
        if(saved=={condition})phase(.17);
        ownership_checkpoint("inverse");measure(coin);return 0;}}'''
    )


def expected(profile, point, context):
    _, p, _, q, _, after_p, after_q, _ = PROFILES[profile]
    registers = {r['id']: r for r in point['registers']}
    named = {r['name']: r for r in point['registers'] if r['name'] is not None}
    heaps = sorted(point['heaps'], key=lambda h: h['handle'])
    if point['tag'] == 'prepared':
        context['initial'] = dict(zip(heaps[0]['ids'], p))
        if q:
            context['initial'].update(zip(heaps[1]['ids'], q))
    elif point['tag'] == 'written':
        context['written'] = dict(zip(heaps[0]['ids'], after_p))
        if q:
            context['written'].update(zip(heaps[1]['ids'], after_q))
    result = {}
    for coin in (0, 1):
        values = {identity: value(coin) for identity, value in context['initial'].items()}
        if point['tag'] != 'prepared':
            values.update({identity: value(coin) for identity, value in context['written'].items()})
        values[named['coin']['id']] = coin
        if 'saved' in named and point['tag'] in ('written', 'freed'):
            values[named['saved']['id']] = (p[0](coin) & 255) ^ 1
        basis = 0
        for identity, register in registers.items():
            value = values.get(identity, 0)
            for bit, wire in enumerate(register['wires']):
                basis |= ((value >> bit) & 1) << wire
        phase = 0.31 if coin else 0
        if point['tag'] == 'inverse' and (p[0](coin) & 255) == (p[0](1) & 255):
            phase += 0.17
        result[basis] = cmath.exp(1j * phase) / math.sqrt(2)
    return result


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
    env = dict(os.environ, CKET_SIMULATOR='sparse', CKET_SEED='7')
    records = []

    def run(label, command, environment=env):
        completed = subprocess.run(
            command,
            cwd=compiler,
            env=environment,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=120,
        )
        (output / (label + '.log')).write_text(completed.stdout)
        records.append(dict(command=command, exit_code=completed.returncode, log=label + '.log'))
        (output / 'commands.json').write_text(json.dumps(records, indent=2) + '\n')
        return completed

    if args.sanitized:
        for component in (
            'cket_plugin.so',
            'json.o',
            'quantum.o',
            'verify.o',
            'simulator.o',
            'cket_runtime.o',
            'cket',
        ):
            inspected = run(
                'instrumentation-' + component, ['nm', '-u', str(compiler / 'build' / component)]
            )
            assert inspected.returncode == 0 and '__asan_init' in inspected.stdout, component
            assert '__ubsan_handle' in inspected.stdout, component

    adapter = output / 'trace.o'
    compiled = run(
        'trace-compile',
        [
            'gcc-11',
            '-std=c11',
            '-O1',
            *flags,
            '-c',
            str(ROOT / 'tests/ownership_trace.c'),
            '-o',
            str(adapter),
        ],
    )
    assert compiled.returncode == 0, compiled.stdout
    links = ['-Wl,' + str(adapter), '-Wl,--wrap=qc_create_sparse', '-Wl,--wrap=__cket_apply']
    points = inverses = 0
    for level in args.levels:
        for name in PROFILES:
            source, binary = output / (name + '.c'), output / (name + level)
            source.write_text(profile_source(name))
            compiled = run(
                name + level + '-compile',
                [
                    str(compiler / 'build/cket'),
                    str(source),
                    '-' + level,
                    *flags,
                    *links,
                    '-o',
                    str(binary),
                ],
            )
            assert compiled.returncode == 0, compiled.stdout
            for tables in (0, 1):
                completed = run(
                    name + level + f'-tables{tables}',
                    [str(binary)],
                    dict(env, OWNERSHIP_TABLES=str(tables)),
                )
                assert completed.returncode == 0, completed.stdout
                snapshots = [
                    json.loads(line)
                    for line in completed.stdout.splitlines()
                    if line.startswith('{')
                ]
                assert [s['tag'] for s in snapshots] == ['prepared', 'written', 'freed', 'inverse']
                state, context, gates = {0: 1 + 0j}, {}, []
                for point in snapshots:
                    assert point['measurements'] == point['resets'] == 0
                    gates.extend(point['gates'])
                    state = REFERENCE['evolve'](state, point['gates'])
                    native = {int(b, 0): complex(r, i) for b, r, i in point['state']}
                    REFERENCE['equal_states'](native, state)
                    REFERENCE['equal_states'](native, expected(name, point, context))
                    points += 1
                inverse = [[k, w, m, p, -a if k == 2 else a] for k, w, m, p, a in reversed(gates)]
                REFERENCE['equal_states'](REFERENCE['evolve'](state, inverse), {0: 1 + 0j})
                inverses += 1
                print(f'PASS {name} {level} tables={tables}', flush=True)
        for name, body in VALID.items():
            source, binary = output / (name + '.c'), output / (name + level)
            source.write_text(HEADER + body)
            compiled = run(
                name + level + '-compile',
                [str(compiler / 'build/cket'), str(source), '-' + level, *flags, '-o', str(binary)],
            )
            assert compiled.returncode == 0, compiled.stdout
            completed = run(name + level + '-run', [str(binary)])
            assert completed.returncode == 0, completed.stdout
        for name, (body, diagnostic) in INVALID.items():
            source, binary = output / (name + '.c'), output / (name + level)
            source.write_text(HEADER + body)
            completed = run(
                name + level + '-compile',
                [str(compiler / 'build/cket'), str(source), '-' + level, *flags, '-o', str(binary)],
            )
            if completed.returncode == 0:
                completed = run(name + level + '-run', [str(binary)])
            assert completed.returncode > 0 and diagnostic in completed.stdout, completed.stdout
    summary = dict(
        status='passed',
        levels=args.levels,
        synthesis_policies=[0, 1],
        full_state_checkpoints=points,
        complete_gate_inverses=inverses,
        native_programs=len(VALID) * len(args.levels),
        semantic_rejections=len(INVALID) * len(args.levels),
        sanitized=args.sanitized,
        plugin_sha256=hashlib.sha256((compiler / 'build/cket_plugin.so').read_bytes()).hexdigest(),
        scope='Finite native heap-to-heap copies, overlapping moves, partial/mixed words, fill capture, coherent controls, saved inverses and disposal; no physical-QPU or complete-libc claim.',
    )
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary), flush=True)


if __name__ == '__main__':
    main()
