#!/usr/bin/env python3
"""Verify finite fixed/heap transfers with complex states and saved inverses."""
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
HEADER = '#include <quantum.h>\n#include <stdlib.h>\n#include <string.h>\nvoid ownership_checkpoint(const char*);\nunsigned heap_transfer_read_count(void);\n'
# Scalar pairs give the expected values for input coin=0 and coin=1.
PROFILES = {
    'fixed_to_heap': dict(
        decl='quant uint8_t fixed[2]={0,5};fixed[0]=coin?9:3;',
        initial={'fixed[0]': (3, 9), 'fixed[1]': (5, 5)},
        heap=[(7, 7), (11, 11)],
        operation='memcpy((void*)p,(const void*)fixed,n);fixed[0]=13;',
        after={'fixed[0]': (13, 13), 'fixed[1]': (5, 5)},
        written=[(3, 9), (5, 5)],
        saved='p[0]^1',
        value=(2, 8),
        n=2,
        finish='measure(fixed[0]);measure(fixed[1]);',
    ),
    'heap_to_fixed': dict(
        decl='quant uint8_t fixed[2]={7,11};',
        initial={'fixed[0]': (7, 7), 'fixed[1]': (11, 11)},
        heap=[(3, 9), (5, 5)],
        operation='memcpy((void*)fixed,(const void*)p,n);p[0]=13;',
        after={'fixed[0]': (3, 9), 'fixed[1]': (5, 5)},
        written=[(13, 13), (5, 5)],
        saved='fixed[0]^1',
        value=(2, 8),
        n=2,
        finish='measure(fixed[0]);measure(fixed[1]);',
    ),
    'partial_word': dict(
        decl='quant uint16_t fixed[1]={0xAAAA};',
        initial={'fixed[0]': (0xAAAA, 0xAAAA)},
        heap=[(3, 9)],
        operation='memmove((unsigned char*)fixed+1,(const void*)p,n);',
        after={'fixed[0]': (0x03AA, 0x09AA)},
        written=[(3, 9)],
        saved='(uint8_t)(fixed[0]>>8)^1',
        value=(2, 8),
        n=1,
        finish='measure(fixed[0]);',
    ),
    'controlled_to_fixed': dict(
        decl='quant uint8_t fixed[1]={7};',
        initial={'fixed[0]': (7, 7)},
        heap=[(3, 9)],
        operation='if(coin)memcpy((void*)fixed,(const void*)p,n);',
        after={'fixed[0]': (7, 9)},
        written=[(3, 9)],
        saved='fixed[0]^1',
        value=(6, 8),
        n=1,
        finish='measure(fixed[0]);',
    ),
    'controlled_from_fixed': dict(
        decl='quant uint8_t fixed[1]={0};fixed[0]=coin?9:3;',
        initial={'fixed[0]': (3, 9)},
        heap=[(7, 7)],
        operation='if(coin)memcpy((void*)p,(const void*)fixed,n);',
        after={'fixed[0]': (3, 9)},
        written=[(7, 9)],
        saved='p[0]^1',
        value=(6, 8),
        n=1,
        finish='measure(fixed[0]);',
    ),
    'cpu_snapshot': dict(
        decl='unsigned char fixed[2]={3,5};',
        initial={},
        heap=[(7, 7), (11, 11)],
        operation='memcpy((void*)p,fixed,n);fixed[0]=99;',
        after={},
        written=[(3, 3), (5, 5)],
        saved='p[0]^1',
        value=(2, 2),
        n=2,
        finish='',
    ),
    'empty_identity': dict(
        decl='quant uint8_t fixed[1]={0};fixed[0]=coin?9:3;',
        initial={'fixed[0]': (3, 9)},
        heap=[(5, 5)],
        operation='memcpy((void*)fixed,(const void*)(p+1),n);',
        after={'fixed[0]': (3, 9)},
        written=[(5, 5)],
        saved='fixed[0]^1',
        value=(2, 8),
        n=0,
        finish='measure(fixed[0]);',
    ),
}
VALID = {
    'fixed_cpu_loop': '''int main(int argc,char **argv){quant uint8_t *p=calloc(1,1);if(!p)return 2;p[0]=3;quant uint8_t out=7;for(int i=0;i<argc;++i)memcpy((void*)&out,(const void*)p,1);unsigned result=measure(out);free((void*)p);return result!=3||argv==0;}''',
    'cpu_loop': '''int main(void){quant uint8_t *p=calloc(2,1);if(!p)return 2;unsigned char input[2]={3,5};size_t rounds=3;for(size_t i=0;i<rounds;++i){input[0]=(unsigned char)(i+3);memcpy((void*)p,input,2);}unsigned reads=heap_transfer_read_count(),a=measure(p[0]),b=measure(p[1]);free((void*)p);return reads!=6||a!=5||b!=5;}''',
    'cpu_reads': '''int main(void){quant uint8_t *p=calloc(4,1);if(!p)return 2;unsigned char input[4]={3,5,7,9};size_t n=2;memcpy((void*)p,input,n);quant bool inactive=0;if(inactive)memcpy((void*)p,input,n);unsigned reads=heap_transfer_read_count();unsigned a=measure(p[0]),b=measure(p[1]),c=measure(p[2]);measure(inactive);free((void*)p);return reads!=2||a!=3||b!=5||c!=0;}''',
    'returned_fixed_alias': '''int main(void){quant uint8_t *p=calloc(2,1);if(!p)return 2;p[0]=3;p[1]=9;quant uint8_t out[2]={0,0};void *alias=memcpy((void*)out,(const void*)p,2);memset(alias,7,1);unsigned a=measure(out[0]),b=measure(out[1]);free((void*)p);return a!=7||b!=9;}''',
    'scalar_round_trip': '''int main(void){quant uint8_t *p=calloc(2,1);if(!p)return 2;quant uint16_t a=0x1234,b=0;size_t n=2;memcpy((void*)p,(const void*)&a,n);memcpy((void*)&b,(const void*)p,n);unsigned x=measure(a),y=measure(b);free((void*)p);return x!=0x1234||y!=0x1234;}''',
    'runtime_prefix': '''int main(void){quant uint8_t *p=calloc(2,1);if(!p)return 2;p[0]=3;p[1]=9;quant uint8_t out[2]={7,11};size_t n=1;memcpy((void*)out,(const void*)p,n);unsigned a=measure(out[0]),b=measure(out[1]);free((void*)p);return a!=3||b!=11;}''',
}
INVALID = {
    'fixed_source_bounds': (
        'int main(void){quant uint8_t *p=calloc(2,1);quant uint8_t a=7;size_t n=2;memcpy((void*)p,(const void*)&a,n);measure(a);free((void*)p);}',
        'fixed source bounds',
    ),
    'fixed_destination_bounds': (
        'int main(void){quant uint8_t *p=calloc(2,1);quant uint8_t a=7;size_t n=2;memcpy((void*)&a,(const void*)p,n);measure(a);free((void*)p);}',
        'fixed storage bounds',
    ),
    'heap_source_bounds': (
        'int main(void){quant uint8_t *p=calloc(1,1);quant uint8_t out[2]={7,9};size_t n=2;memcpy((void*)out,(const void*)p,n);measure(out[0]);measure(out[1]);free((void*)p);}',
        'outside live storage',
    ),
    'cpu_destination': (
        'int main(void){quant uint8_t *p=calloc(1,1);unsigned char out=7;memcpy(&out,(const void*)p,1);free((void*)p);}',
        'unmeasured data into CPU',
    ),
    'unknown_cpu_source': (
        'void f(quant uint8_t *p,const void *unknown){memcpy((void*)p,unknown,1);}int main(void){quant uint8_t *p=calloc(1,1);f(p,(void*)0);free((void*)p);}',
        'checked fixed quantum buffer or known CPU',
    ),
    'volatile_cpu_source': (
        'int main(void){quant uint8_t *p=calloc(1,1);volatile unsigned char a=7;memcpy((void*)p,(const void*)&a,1);free((void*)p);}',
        'volatile/atomic',
    ),
}


def source_for(profile):
    p = PROFILES[profile]
    prepare = ''.join(f'p[{i}]=coin?{v[1]}:{v[0]};' for i, v in enumerate(p['heap']))
    return (
        HEADER
        + f'''int main(void){{quant uint8_t *p=calloc({len(p['heap'])},1);if(!p)return 2;
        quant bool coin=superposition(0,1);{p['decl']}{prepare}
        if(coin)phase(.31);size_t n={p['n']};ownership_checkpoint("prepared");
        {p['operation']}quant uint8_t saved={p['saved']};ownership_checkpoint("written");
        free((void*)p);ownership_checkpoint("freed");
        if(saved=={p['value'][1]})phase(.17);ownership_checkpoint("inverse");
        {p['finish']}measure(coin);return 0;}}'''
    )


def named(point):
    return {r['name']: r for r in point['registers'] if r['name'] is not None and not r['history']}


def expected(profile, point, context):
    p = PROFILES[profile]
    names = named(point)
    registers = {r['id']: r for r in point['registers']}
    if point['tag'] == 'prepared':
        context['initial'] = {names[name]['id']: value for name, value in p['initial'].items()}
        context['initial'].update(zip(point['heaps'][0]['ids'], p['heap']))
        context['wires'] = {name: names[name]['wires'] for name in p['initial']}
    if point['tag'] == 'written':
        context['written'] = dict(zip(point['heaps'][0]['ids'], p['written']))
        if profile == 'empty_identity':
            assert (
                names['fixed[0]']['wires'] == context['wires']['fixed[0]']
            ), 'zero byte copy changed the public quantum wires'
        if profile == 'controlled_to_fixed':
            assert not [
                r for r in point['registers'] if r['id'] < 4096 and r['name'] is None
            ], 'controlled transfer retained an unnamed guard register'
    histories = [
        r['id']
        for r in point['registers']
        if r['history']
        and r['id'] not in context['initial']
        and r['id'] not in context.get('written', {})
    ]
    if profile == 'controlled_from_fixed' and point['tag'] != 'prepared':
        assert len(histories) == 1
    else:
        assert not histories
    answer = {}
    for coin in (0, 1):
        values = {identity: value[coin] for identity, value in context['initial'].items()}
        if point['tag'] != 'prepared':
            values.update({identity: value[coin] for identity, value in context['written'].items()})
            values.update({names[name]['id']: value[coin] for name, value in p['after'].items()})
            if profile == 'controlled_from_fixed':
                values[histories[0]] = p['heap'][0][0] if coin else 0
        values[names['coin']['id']] = coin
        if point['tag'] in ('written', 'freed'):
            values[names['saved']['id']] = p['value'][coin]
        basis = 0
        for identity, r in registers.items():
            for bit, wire in enumerate(r['wires']):
                basis |= ((values.get(identity, 0) >> bit) & 1) << wire
        phase = (0.31 if coin else 0) + (
            0.17 if point['tag'] == 'inverse' and p['value'][coin] == p['value'][1] else 0
        )
        answer[basis] = cmath.exp(1j * phase) / math.sqrt(2)
    return answer


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler-root', type=Path, default=ROOT)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--levels', nargs='+', default=['O2'])
    parser.add_argument('--sanitized', action='store_true')
    args = parser.parse_args()
    compiler = args.compiler_root.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    levels = [level.lstrip('-') for level in args.levels]
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
            cwd=compiler,
            env=environment,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=120,
        )
        (output / (label + '.log')).write_text(result.stdout)
        records.append(dict(command=command, exit_code=result.returncode, log=label + '.log'))
        (output / 'commands.json').write_text(json.dumps(records, indent=2) + '\n')
        return result

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
            result = run(
                'instrumentation-' + component, ['nm', '-u', str(compiler / 'build' / component)]
            )
            assert (
                result.returncode == 0
                and '__asan_init' in result.stdout
                and '__ubsan_handle' in result.stdout
            ), component
    adapter = output / 'trace.o'
    result = run(
        'trace-compile',
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
    assert result.returncode == 0, result.stdout
    links = [
        '-Wl,' + str(adapter),
        '-Wl,--wrap=qc_create_sparse',
        '-Wl,--wrap=__cket_apply',
        '-Wl,--wrap=qc_apply',
        '-Wl,--wrap=qc_apply_bound',
        '-Wl,--wrap=__cket_read_byte',
    ]
    points = inverses = 0
    for level in levels:
        for name in PROFILES:
            source = output / (name + '.c')
            binary = output / (name + level)
            source.write_text(source_for(name))
            result = run(
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
            assert result.returncode == 0, result.stdout
            for tables in (0, 1):
                result = run(
                    name + level + f'-tables{tables}',
                    [str(binary)],
                    dict(env, OWNERSHIP_TABLES=str(tables)),
                )
                assert result.returncode == 0, result.stdout
                snapshots = [
                    json.loads(line) for line in result.stdout.splitlines() if line.startswith('{')
                ]
                assert [p['tag'] for p in snapshots] == ['prepared', 'written', 'freed', 'inverse']
                state = {0: 1 + 0j}
                context = {}
                gates = []
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
            source = output / (name + '.c')
            binary = output / (name + level)
            source.write_text(HEADER + body)
            result = run(
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
            assert result.returncode == 0, result.stdout
            result = run(name + level + '-run', [str(binary)])
            assert result.returncode == 0, result.stdout
        for name, (body, diagnostic) in INVALID.items():
            source = output / (name + '.c')
            binary = output / (name + level)
            source.write_text(HEADER + body)
            result = run(
                name + level + '-compile',
                [str(compiler / 'build/cket'), str(source), '-' + level, *flags, '-o', str(binary)],
            )
            if result.returncode == 0:
                result = run(name + level + '-run', [str(binary)])
            assert result.returncode > 0 and diagnostic in result.stdout, result.stdout
    summary = dict(
        status='passed',
        levels=levels,
        synthesis_policies=[0, 1],
        full_state_checkpoints=points,
        complete_gate_inverses=inverses,
        native_programs=len(VALID) * len(levels),
        semantic_rejections=len(INVALID) * len(levels),
        sanitized=args.sanitized,
        plugin_sha256=hashlib.sha256((compiler / 'build/cket_plugin.so').read_bytes()).hexdigest(),
        scope='Finite fixed/heap and known CPU source transfers, complete complex joint-state formulas, saved inverses after free, zero-length wire identity and counted CPU reads; no complete C11/ABI or QPU claim.',
    )
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary), flush=True)


if __name__ == '__main__':
    main()
