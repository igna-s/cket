#!/usr/bin/env python3
"""Compare dynamically allocated native quantum buffers with independent states."""

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
HEADER = '#include <quantum.h>\n#include <stdlib.h>\n#include <limits.h>\nvoid ownership_checkpoint(const char*);\n'
PROFILES = {
    'grow_capture': '''int main(void) {
        size_t count=2,index=0;
        quant uint8_t *p=calloc(count,sizeof *p);
        if(!p)return 2;
        quant bool coin=superposition(0,1);
        p[0]=7;p[1]=5;if(coin)p[0]=3;
        if(coin)phase(.31);
        quant uint8_t selected=p[index]^1;
        index=1;
        p=realloc((void*)p,3*sizeof *p);
        p[2]=9;p[0]=11;
        ownership_checkpoint("retained");
        if(selected==2)phase(.17);
        ownership_checkpoint("inverse");
        free((void*)p);
        ownership_checkpoint("freed");
        measure(coin);return index!=1;
    }''',
    'shrink_saved': '''int main(void) {
        size_t count=3;
        quant uint8_t *p=malloc(count*sizeof *p);
        if(!p)return 2;
        quant bool coin=superposition(0,1);
        p[0]=1;p[1]=coin?2:6;p[2]=9;
        if(coin)phase(.31);
        quant uint8_t selected=p[1]^1;
        p=realloc((void*)p,sizeof *p);
        ownership_checkpoint("retained");
        if(selected==3)phase(.17);
        ownership_checkpoint("inverse");
        free((void*)p);
        ownership_checkpoint("freed");
        measure(coin);return 0;
    }''',
    'quantum_read': '''int main(void) {
        size_t count=2;
        quant uint8_t *p=calloc(count,sizeof *p);
        if(!p)return 2;
        p[0]=3;p[1]=9;
        quant bool coin=superposition(0,1);
        if(coin)phase(.31);
        quant uint8_t selected=p[coin]^1;
        ownership_checkpoint("retained");
        if(selected==8)phase(.17);
        ownership_checkpoint("inverse");
        free((void*)p);
        ownership_checkpoint("freed");
        measure(coin);return 0;
    }''',
}
EFFECT_PROFILES = {
    'payload_write': '''int main(void) {
        quant uint8_t *p=calloc(2,sizeof *p);
        if(!p)return 2;
        quant bool coin=superposition(0,1),pick=superposition(0,1);
        p[0]=3;p[1]=coin?9:5;
        if(coin)phase(.31);
        if(pick)phase(.13);
        ownership_checkpoint("prepared");
        quant uint8_t saved=p[pick]++;
        ownership_checkpoint("written");
        if(saved==9)phase(.17);
        ownership_checkpoint("inverse");
        free((void*)p);
        ownership_checkpoint("freed");
        measure(pick);measure(coin);return 0;
    }''',
    'lazy_update_value': '''int main(void) {
        quant uint8_t *p=calloc(2,sizeof *p);
        if(!p)return 2;
        p[0]=3;p[1]=9;
        quant bool coin=superposition(0,1);
        if(coin)phase(.31);
        ownership_checkpoint("prepared");
        quant uint8_t saved=coin?p[coin]++:5;
        ownership_checkpoint("written");
        if(saved==9)phase(.17);
        ownership_checkpoint("inverse");
        free((void*)p);
        ownership_checkpoint("freed");
        measure(coin);return 0;
    }''',
    'postfix_saved': '''int main(void) {
        quant uint8_t *p=calloc(2,sizeof *p);
        if(!p)return 2;
        p[0]=3;p[1]=9;
        quant bool coin=superposition(0,1);
        if(coin)phase(.31);
        ownership_checkpoint("prepared");
        quant uint8_t saved=p[coin]++;
        ownership_checkpoint("written");
        free((void*)p);
        ownership_checkpoint("retired");
        if(saved==9)phase(.17);
        ownership_checkpoint("inverse");
        measure(coin);return 0;
    }''',
    'assigned_value': '''int main(void) {
        quant uint8_t *p=calloc(2,sizeof *p);
        if(!p)return 2;
        p[0]=3;p[1]=9;
        quant bool coin=superposition(0,1);
        if(coin)phase(.31);
        ownership_checkpoint("prepared");
        quant uint8_t saved=(p[coin]=coin?7:5);
        ownership_checkpoint("written");
        if(saved==7)phase(.17);
        ownership_checkpoint("inverse");
        free((void*)p);
        ownership_checkpoint("freed");
        measure(coin);return 0;
    }''',
    'controlled_write': '''int main(void) {
        quant uint8_t *p=calloc(2,sizeof *p);
        if(!p)return 2;
        p[0]=3;p[1]=9;
        quant bool coin=superposition(0,1);
        if(coin)phase(.31);
        ownership_checkpoint("prepared");
        if(!coin)p[coin]=7;
        ownership_checkpoint("written");
        if(p[0]==7)phase(.17);
        ownership_checkpoint("inverse");
        free((void*)p);
        ownership_checkpoint("freed");
        measure(coin);return 0;
    }''',
}
PROFILES.update(EFFECT_PROFILES)

VALID = {
    'heap_loop_values': '''int main(void){quant uint8_t *p=calloc(1,sizeof *p);if(!p)return 2;quant uint8_t sum=0;size_t count=2;for(size_t i=0;i<count;++i)sum+=p[0]++;unsigned result=measure(sum),last=measure(p[0]);free((void*)p);return result!=1||last!=2;}''',
    'allocation_failure': '''int main(void){size_t n=2;quant uint8_t *p=calloc(n,sizeof *p);if(!p)return 2;p[0]=7;p[1]=9;quant uint8_t *q=realloc((void*)p,65*sizeof *p);unsigned a=measure(p[0]),b=measure(p[1]);free((void*)p);return q!=NULL||a!=7||b!=9;}''',
    'calloc_overflow': '''int main(void){size_t n=SIZE_MAX;quant uint8_t *p=calloc(n,2);return p!=NULL;}''',
    'null_realloc': '''int main(void){quant uint8_t *p=NULL;p=realloc((void*)p,2*sizeof *p);if(!p)return 2;p[0]=5;unsigned x=measure(p[0]);free((void*)p);return x!=5;}''',
    'signed_words': '''int main(void){size_t n=2;quant int16_t *p=malloc(n*sizeof *p);if(!p)return 2;p[0]=-7;p[1]=1234;quant int16_t selected=p[1]^1;int result=measure(selected),first=measure(p[0]),second=measure(p[1]);free((void*)p);return result!=1235||first!=-7||second!=1234;}''',
    'suffix_function': '''quant uint8_t pick(quant const uint8_t *p,size_t i){return p[i]^1;} int main(void){quant uint8_t *p=calloc(3,sizeof *p);if(!p)return 2;p[0]=2;p[1]=5;p[2]=9;quant uint8_t result=pick(p+1,1);unsigned x=measure(result);free((void*)p);return x!=8;}''',
    'repeated_lifetimes': '''int main(void){size_t rounds=30;for(size_t i=0;i<rounds;++i){quant uint8_t *p=calloc(2,sizeof *p);if(!p)return 2;p[0]=7;p[1]=9;free((void*)p);}return 0;}''',
    'inactive_bad_pointer': '''int main(void){quant uint8_t *p=NULL;quant bool b=0;if(b)p[0]=1;measure(b);return 0;}''',
    'narrow_observation': '''int main(void){quant uint8_t *p=calloc(1,sizeof *p);if(!p)return 2;p[0]=255;unsigned low=measure((uint8_t)(p[0]&3));unsigned full=measure(p[0]);free((void*)p);return low!=3||full!=255;}''',
}
INVALID = {
    'bounds': (
        'int main(void){quant uint8_t *p=calloc(2,sizeof *p);size_t i=2;quant uint8_t x=p[i]^1;measure(x);free((void*)p);}',
        ('division',),
    ),
    'write_bounds': (
        'int main(void){quant uint8_t *p=calloc(2,sizeof *p);size_t i=2;p[i]=1;free((void*)p);}',
        ('outside live storage',),
    ),
    'pointer_wrap': (
        'int main(void){quant uint16_t *p=calloc(2,sizeof *p);size_t i=(size_t)1<<63;quant uint16_t x=p[i]^1;measure(x);free((void*)p);}',
        ('division',),
    ),
    'after_free': (
        'int main(void){quant uint8_t *p=calloc(2,sizeof *p);free((void*)p);quant uint8_t x=p[0]^1;measure(x);}',
        ('division',),
    ),
    'readonly_store': (
        'void f(quant const uint8_t *p){((uint8_t*)p)[0]=1;} int main(void){quant uint8_t *p=calloc(2,sizeof *p);f(p);free((void*)p);}',
        ('mutable borrow',),
    ),
    'borrow_free': (
        'void f(quant uint8_t *p){free((void*)p);} int main(void){quant uint8_t *p=calloc(2,sizeof *p);f(p);free((void*)p);}',
        ('borrowed quantum heap',),
    ),
    'borrow_realloc': (
        'void f(quant uint8_t *p){quant uint8_t *q=realloc((void*)p,3*sizeof *p);(void)q;} int main(void){quant uint8_t *p=calloc(2,sizeof *p);f(p);free((void*)p);}',
        ('borrowed quantum heap',),
    ),
    'borrow_alias_free': (
        'void f(quant uint8_t *p){quant uint8_t *q=NULL;q=p;free((void*)q);} int main(void){quant uint8_t *p=calloc(2,sizeof *p);f(p);free((void*)p);}',
        ('borrowed quantum heap',),
    ),
    'overlapping_parameters': (
        'void f(quant uint8_t *a,quant uint8_t *b){a[0]=1;b[0]=2;} int main(void){quant uint8_t *p=calloc(2,sizeof *p);f(p,p);free((void*)p);}',
        ('disjoint',),
    ),
    'implicit_read': (
        'int main(void){quant uint8_t *p=calloc(2,sizeof *p);int x=p[0];free((void*)p);return x;}',
        ('classical initialization',),
    ),
    'struct_escape': (
        'int main(void){quant uint8_t *p=calloc(2,sizeof *p);struct S{uint8_t *p;}s={(uint8_t*)p};free((void*)p);return s.p!=0;}',
        ('escape',),
    ),
    'signed_overflow': (
        'int main(void){quant uint8_t *p=calloc(1,sizeof *p);p[0]=1;quant int y=INT_MAX;p[0]+=y;measure(y);free((void*)p);}',
        ('overflow', 'structured synthesis'),
    ),
}

VALID.update(
    {
        'heap_prefix': 'int main(void){quant uint8_t *p=calloc(2,sizeof *p);p[0]=3;p[1]=9;quant bool i=superposition(0,1);quant uint8_t x=++p[i];unsigned got=measure(x),j=measure(i),a=measure(p[0]),b=measure(p[1]);free((void*)p);return got!=(j?10u:4u)||a!=(j?3u:4u)||b!=(j?10u:9u);}',
        'heap_cpu_postfix': 'int main(void){quant uint8_t *p=calloc(2,sizeof *p);size_t i=1;p[0]=3;p[1]=9;quant uint8_t x=p[i]++;unsigned got=measure(x),a=measure(p[0]),b=measure(p[1]);free((void*)p);return got!=9||a!=3||b!=10;}',
        'heap_inactive_value': 'int main(void){quant uint8_t *p=NULL;quant bool b=0;quant uint8_t x=b?p[0]++:5;unsigned got=measure(x);measure(b);return got!=5;}',
        'heap_comma': 'int main(void){quant uint8_t *p=calloc(1,sizeof *p);p[0]=3;quant uint8_t x=(p[0]++,p[0]);unsigned a=measure(x),b=measure(p[0]);free((void*)p);return a!=4||b!=4;}',
        'heap_arithmetic': 'int main(void){quant uint8_t *p=calloc(1,sizeof *p);p[0]=3;int add=2;quant uint8_t x=add+p[0]++;unsigned a=measure(x),b=measure(p[0]);free((void*)p);return a!=5||b!=4;}',
        'heap_function_store': 'void f(quant uint8_t *p,quant bool i){p[i]=7;}int main(void){quant uint8_t *p=calloc(2,sizeof *p);p[0]=3;p[1]=9;quant bool i=superposition(0,1);f(p,i);unsigned j=measure(i),a=measure(p[0]),b=measure(p[1]);free((void*)p);return a!=(j?3u:7u)||b!=(j?7u:9u);}',
        'heap_measure_postfix': 'int main(void){quant uint8_t *p=calloc(2,sizeof *p);p[0]=3;p[1]=9;quant bool i=superposition(0,1);unsigned got=measure(p[i]++),j=measure(i),a=measure(p[0]),b=measure(p[1]);free((void*)p);return got!=(j?9u:3u)||a!=(j?3u:4u)||b!=(j?10u:9u);}',
    }
)
INVALID.update(
    {
        'quantum_write_bounds': (
            'int main(void){quant uint8_t *p=calloc(2,sizeof *p);quant uint8_t i=superposition(0,3);p[i]=7;measure(i);free((void*)p);}',
            ('division',),
        ),
        'quantum_write_wrap': (
            'int main(void){quant uint16_t *p=calloc(2,sizeof *p);quant uint64_t i=(uint64_t)1<<63;p[i]=7;measure(i);free((void*)p);}',
            ('division',),
        ),
        'heap_unsequenced_index': (
            'int main(void){quant uint8_t *p=calloc(2,sizeof *p);quant uint8_t i=superposition(0,1);p[i]=i++;measure(i);free((void*)p);}',
            ('unsequenced',),
        ),
        'heap_unsequenced_payload': (
            'int main(void){quant uint8_t *p=calloc(2,sizeof *p);p[0]=1;quant uint8_t x=p[0]++ +p[0];measure(x);free((void*)p);}',
            ('unsequenced',),
        ),
    }
)


def payload_expected(point: dict, context: dict) -> dict[int, complex]:
    registers = {r['id']: r for r in point['registers']}
    named = {r['name']: r for r in point['registers'] if r['name'] is not None}
    current = point['heaps'][0]['ids'] if point['heaps'] else []
    if point['tag'] == 'prepared':
        context['initial'] = current
    elif point['tag'] == 'written':
        context['written'] = current
    answer = {}
    for coin in (0, 1):
        for pick in (0, 1):
            old = 9 if coin else 5
            terms = {named['coin']['id']: coin, named['pick']['id']: pick}
            prepared = point['tag'] == 'prepared'
            ids = context['initial'] if prepared else context['written']
            values = (3, old) if prepared else (3 if pick else 4, old + pick)
            for identity, value in zip(ids, values):
                if identity in registers:
                    terms[identity] = value
            for identity, value in zip(context['initial'], (3, old)):
                if identity in registers and identity not in terms:
                    terms[identity] = value
            if point['tag'] == 'written':
                saved = old if pick else 3
                terms[named['saved']['id']] = saved
                captures = [
                    r
                    for r in point['registers']
                    if r['name'] and r['name'].startswith('__cket_heap_value_')
                ]
                assert len(captures) == 1
                terms[captures[0]['id']] = saved
            basis = 0
            for identity, value in terms.items():
                for bit, wire in enumerate(registers[identity]['wires']):
                    basis |= ((value >> bit) & 1) << wire
            angle = (0.31 if coin else 0) + (0.13 if pick else 0)
            if point['tag'] in ('inverse', 'freed') and coin and pick:
                angle += 0.17
            answer[basis] = cmath.exp(1j * angle) * 0.5
    return answer


def effect_expected(profile: str, point: dict, context: dict) -> dict[int, complex]:
    if profile == 'payload_write':
        return payload_expected(point, context)
    registers = {r['id']: r for r in point['registers']}
    named = {r['name']: r for r in point['registers'] if r['name'] is not None}
    current = point['heaps'][0]['ids'] if point['heaps'] else []
    if point['tag'] == 'prepared':
        context['initial'] = dict(zip(current, (3, 9)))
    elif point['tag'] == 'written':
        context['written'] = current
    answer = {}
    for coin in (0, 1):
        terms = {named['coin']['id']: coin}
        prepared = point['tag'] == 'prepared'
        output_ids = current if prepared else context['written']
        if prepared:
            values = (3, 9)
        elif profile == 'postfix_saved':
            values = (3 if coin else 4, 10 if coin else 9)
        elif profile == 'assigned_value':
            values = (3 if coin else 5, 7 if coin else 9)
        elif profile == 'lazy_update_value':
            values = (3, 10 if coin else 9)
        else:
            values = (3 if coin else 7, 9)
        for identity, value in zip(output_ids, values):
            if identity in registers:
                terms[identity] = value
        for identity, value in context['initial'].items():
            if identity in registers and identity not in terms:
                terms[identity] = value
        retained = point['tag'] in ('written', 'retired')
        if retained and profile != 'controlled_write':
            saved = (
                (9 if coin else 3)
                if profile == 'postfix_saved'
                else (9 if coin else 5) if profile == 'lazy_update_value' else (7 if coin else 5)
            )
            terms[named['saved']['id']] = saved
            captures = [
                r
                for r in point['registers']
                if r['name'] and r['name'].startswith('__cket_heap_value_')
            ]
            assert len(captures) == 1
            terms[captures[0]['id']] = (
                (9 if coin else 0) if profile == 'lazy_update_value' else saved
            )
            if profile == 'lazy_update_value':
                guards = [r for r in point['registers'] if r['name'] is None and r['id'] < 4096]
                assert len(guards) == 1
                terms[guards[0]['id']] = coin
        basis = 0
        for identity, value in terms.items():
            for bit, wire in enumerate(registers[identity]['wires']):
                basis |= ((value >> bit) & 1) << wire
        phased = point['tag'] in ('inverse', 'freed')
        angle = (0.31 if coin else 0) + (
            0.17 if phased and (not coin if profile == 'controlled_write' else coin) else 0
        )
        answer[basis] = cmath.exp(1j * angle) / math.sqrt(2)
    return answer


def expected(profile: str, point: dict) -> dict[int, complex]:
    registers = {r['id']: r for r in point['registers']}
    named = {r['name']: r for r in point['registers'] if r['name'] is not None}
    heaps = point['heaps']
    current = [registers[i] for i in heaps[0]['ids']] if heaps else []
    histories = [r for r in point['registers'] if r['history'] and r['name'] is None]
    retained = point['tag'] == 'retained'
    answer = {}
    for coin in (0, 1):
        terms = [(named['coin'], coin)]
        if profile == 'quantum_read':
            terms.extend(zip(current, (3, 9)))
            if retained:
                terms.append((named['selected'], (9 if coin else 3) ^ 1))
            assert not histories
        else:
            terms.extend(zip(current, (11, 5, 9) if profile == 'grow_capture' else (1,)))
            unknown = [r for r in histories if not r['known']]
            assert len(unknown) == 1
            old = (3 if coin else 7) if profile == 'grow_capture' else (2 if coin else 6)
            terms.append((unknown[0], old))
            if retained:
                terms.append((named['selected'], old ^ 1))
            for r in histories:
                if r['known']:
                    terms.append((r, r['known_value']))
        basis = 0
        for register, value in terms:
            for bit, wire in enumerate(register['wires']):
                basis |= ((value >> bit) & 1) << wire
        angle = (0.31 + (0 if retained else 0.17)) if coin else 0
        answer[basis] = cmath.exp(1j * angle) / math.sqrt(2)
    return answer


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler-root', type=Path, default=ROOT)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--levels', nargs='+', default=['O2'])
    parser.add_argument('--sanitized', action='store_true')
    args = parser.parse_args()
    compiler = args.compiler_root.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    levels = ['-' + level.lstrip('-') for level in args.levels]
    flags = (
        ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
        if args.sanitized
        else []
    )
    records = []

    def run(command: list[str], label: str, env=None):
        result = subprocess.run(
            command,
            cwd=compiler,
            env=env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=120,
        )
        (output / (label + '.log')).write_text(result.stdout)
        records.append({'command': command, 'exit_code': result.returncode, 'log': label + '.log'})
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
                ['nm', '-u', str(compiler / 'build' / component)], 'instrumentation-' + component
            )
            assert result.returncode == 0 and '__asan_init' in result.stdout, component
            assert '__ubsan_handle' in result.stdout, component
    adapter = output / 'ownership_trace.o'
    result = run(
        [
            'gcc-11',
            '-std=c11',
            '-O1',
            '-Wall',
            '-Wextra',
            '-Werror',
            *flags,
            '-c',
            str(ROOT / 'tests/ownership_trace.c'),
            '-o',
            str(adapter),
        ],
        'adapter',
    )
    assert result.returncode == 0, result.stdout
    links = ['-Wl,' + str(adapter), '-Wl,--wrap=qc_create_sparse', '-Wl,--wrap=__cket_apply']
    points = 0
    for level in levels:
        for profile, body in PROFILES.items():
            source = output / (profile + '.c')
            source.write_text(HEADER + body)
            binary = output / (profile + level)
            result = run(
                [
                    str(compiler / 'build/cket'),
                    str(source),
                    level,
                    *flags,
                    *links,
                    '-o',
                    str(binary),
                ],
                profile + level + '-compile',
            )
            assert result.returncode == 0, result.stdout
            for tables in (0, 1):
                env = dict(
                    os.environ, CKET_SIMULATOR='sparse', CKET_SEED='7', OWNERSHIP_TABLES=str(tables)
                )
                result = run([str(binary)], profile + level + f'-tables{tables}', env)
                assert result.returncode == 0, result.stdout
                state = {0: 1 + 0j}
                snapshots = [
                    json.loads(line) for line in result.stdout.splitlines() if line.startswith('{')
                ]
                assert len(snapshots) == (4 if profile in EFFECT_PROFILES else 3)
                context = {}
                for point in snapshots:
                    assert not point['measurements'] and not point['resets']
                    state = REFERENCE['evolve'](state, point['gates'])
                    native = {
                        int(basis, 0): complex(real, imag) for basis, real, imag in point['state']
                    }
                    REFERENCE['equal_states'](native, state)
                    formula = (
                        effect_expected(profile, point, context)
                        if profile in EFFECT_PROFILES
                        else expected(profile, point)
                    )
                    REFERENCE['equal_states'](native, formula)
                    points += 1
                print(f'PASS {profile} {level} tables={tables}', flush=True)
        for name, body in VALID.items():
            source = output / (name + '.c')
            source.write_text(HEADER + body)
            binary = output / (name + level)
            result = run(
                [str(compiler / 'build/cket'), str(source), level, *flags, '-o', str(binary)],
                name + level + '-compile',
            )
            assert result.returncode == 0, result.stdout
            result = run(
                [str(binary)],
                name + level + '-run',
                dict(os.environ, CKET_SIMULATOR='sparse', CKET_SEED='7'),
            )
            assert result.returncode == 0, result.stdout
        for name, (body, diagnostics) in INVALID.items():
            source = output / (name + '.c')
            source.write_text(HEADER + body)
            binary = output / (name + level)
            result = run(
                [str(compiler / 'build/cket'), str(source), level, *flags, '-o', str(binary)],
                name + level + '-compile',
            )
            if result.returncode == 0:
                result = run(
                    [str(binary)],
                    name + level + '-run',
                    dict(os.environ, CKET_SIMULATOR='sparse', CKET_SEED='7'),
                )
            assert result.returncode > 0 and any(
                part in result.stdout.lower() for part in diagnostics
            ), result.stdout
    summary = {
        'status': 'passed',
        'full_state_checkpoints': points,
        'levels': levels,
        'synthesis_policies': [0, 1],
        'valid_programs': len(VALID) * len(levels),
        'semantic_rejections': len(INVALID) * len(levels),
        'sanitized': args.sanitized,
        'compiler_sha256': hashlib.sha256(
            (compiler / 'build/cket_plugin.so').read_bytes()
        ).hexdigest(),
        'sanitizer_scope': (
            'plugin, driver, engine, runtime, fixture and adapter' if args.sanitized else 'none'
        ),
        'scope': 'Independent joint-state formulas and gate evolution for growing/shrinking heaps, coherent selected writes, old/new update values, live results across free, captured indices and saved inverses; finite native/domain cases.',
    }
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary), flush=True)


if __name__ == '__main__':
    main()
