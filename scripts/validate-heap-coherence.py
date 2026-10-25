#!/usr/bin/env python3
"""Finite heap channels, untouched-byte coherence and independent gate inverses."""
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
    'store',
    'open',
    'nested',
    'self',
    'self_open',
    'borrow',
    'byte',
    'byte_plain',
    'partial_borrow',
    'negative_byte',
    'heap_copy',
    'fill',
    'fill_two',
    'move_left',
    'move_right',
    'scatter',
    'controlled_scatter',
)


def width(type_name):
    return 1 if type_name == 'bool' else int(''.join(c for c in type_name if c.isdigit()))


def supported(type_name, profile):
    bits = width(type_name)
    if bits == 1 and profile not in (
        'store',
        'open',
        'nested',
        'self',
        'self_open',
        'borrow',
        'scatter',
        'controlled_scatter',
    ):
        return False
    if profile in ('move_left', 'move_right', 'scatter', 'controlled_scatter') and bits == 32:
        return False  # Two 32-bit destinations plus two old-word histories exceed 128 wires.
    if profile == 'fill_two' and bits < 16:
        return False
    if profile == 'negative_byte' and not type_name.startswith('int'):
        return False
    return True


def source(type_name, profile):
    bits = width(type_name)
    count = 3 if profile in ('move_left', 'move_right') else 2 if 'scatter' in profile else 1
    marker = 256 if bits >= 16 else 4 if bits > 1 else 1
    initial = 'a' if bits == 1 else f'((a?{marker}:0)|b)'
    if profile == 'negative_byte':
        initial = '~' + initial
    allocate = f'quant word *p=calloc({count},sizeof(word));if(!p)return 2;'
    initialize = f'p[0]=(word)({initial});'
    if count > 1:
        initialize += 'p[1]=(word)z;'
    if count > 2:
        initialize += 'p[2]=(word)b;'
    if profile == 'heap_copy':
        allocate += 'quant word *q=calloc(1,sizeof(word));if(!q)return 2;'
        initialize += 'q[0]=(word)z;'
    operation = 'p[0]=(word)x;'
    if profile in ('byte', 'byte_plain', 'partial_borrow', 'negative_byte'):
        operation = 'memcpy((void*)p,(const void*)&x,n);'
    elif profile == 'heap_copy':
        operation = 'memcpy((void*)p,(const void*)q,n);'
    elif profile in ('fill', 'fill_two'):
        operation = 'memset((void*)p,x,n);'
    elif profile == 'move_left':
        operation = 'memmove((void*)p,(const void*)(p+1),n);'
    elif profile == 'move_right':
        operation = 'memmove((void*)(p+1),(const void*)p,n);'
    elif 'scatter' in profile:
        operation = 'p[d]=(word)x;'
    predicate = {'open': '!c', 'nested': 'c&&d', 'self': '(p[0]&1)', 'self_open': '!(p[0]&1)'}.get(
        profile, 'c'
    )
    effect = (
        operation if profile in ('byte_plain', 'scatter') else f'if({predicate}){{{operation}}}'
    )
    if profile in ('borrow', 'partial_borrow'):
        body = (
            '{quant word saved=p[0]^1;'
            + effect
            + 'if(saved&1)phase(.47);ownership_checkpoint("borrowed");'
            f'if(saved&{marker})phase(.13);' + '}'
        )
    else:
        body = effect
    return f'''#include <quantum.h>
#include <stdlib.h>
#include <string.h>
void ownership_checkpoint(const char*);
typedef {type_name} word;
int main(int argc,char **argv){{
size_t n=argc>1?strtoul(argv[1],0,10):1;
{allocate}
quant bool a=superposition(0,1),b=superposition(0,1),z=superposition(0,1);
quant uint8_t x=superposition(0,1);quant bool c=superposition(0,1),d=superposition(0,1);
if(a)phase(.11);if(b)phase(.23);if(z)phase(.31);if(x)phase(.41);if(c)phase(.17);if(d)phase(.29);
{initialize}ownership_checkpoint("before");{body}ownership_checkpoint("after");
free((void*)p);{'free((void*)q);' if profile=='heap_copy' else ''}
ownership_checkpoint("freed");measure(a);measure(b);measure(z);measure(x);measure(c);measure(d);return 0;}}
'''


def actual_density(point, registers):
    wires = [w for r in registers for w in r['wires']]
    mask = sum(1 << w for w in wires)
    blocks = {}
    for basis, real, imag in point['state']:
        basis = int(basis, 0)
        local = sum(((basis >> w) & 1) << i for i, w in enumerate(wires))
        blocks.setdefault(basis & ~mask, {})[local] = complex(real, imag)
    return density(blocks)


def density(blocks):
    result = {}
    for rows in blocks.values():
        for a, left in rows.items():
            for b, right in rows.items():
                result[a, b] = result.get((a, b), 0j) + left * right.conjugate()
    return result


def check(type_name, profile, count, points):
    borrowed = profile in ('borrow', 'partial_borrow')
    assert [p['tag'] for p in points] == (
        ['before', 'borrowed', 'after', 'freed'] if borrowed else ['before', 'after', 'freed']
    )
    bits = width(type_name)
    marker = 256 if bits >= 16 else 4 if bits > 1 else 1
    public_names = ('a', 'b', 'z', 'x', 'c', 'd')
    heap_ids = None
    original_roots = None
    original_heap_ids = None
    evolved, gates = {0: 1 + 0j}, []
    for point in points:
        assert point['measurements'] == point['resets'] == 0
        names = {r['name']: r for r in point['registers'] if r['name'] and not r['history']}
        roots = [(names[n]['id'], names[n]['wires']) for n in public_names]
        if original_roots is None:
            original_roots = roots
        assert roots == original_roots, 'fixed public root changed identity'
        if point['heaps']:
            # Allocation order is reversed by the intrusive heap list.
            heap_ids = [h['ids'] for h in sorted(point['heaps'], key=lambda h: h['handle'])]
        assert heap_ids is not None
        if original_heap_ids is None:
            original_heap_ids = heap_ids
        if profile != 'byte_plain' or bits > 8:
            assert heap_ids == original_heap_ids, 'controlled/partial write replaced a public owner'
        by_id = {r['id']: r for r in point['registers']}
        registers = [names[n] for n in public_names] + [by_id[i] for h in heap_ids for i in h]
        if point['tag'] == 'borrowed':
            registers.append(names['saved'])
        expected_blocks = {}
        for a, b, z, x, c, d in itertools.product((0, 1), repeat=6):
            old = a if bits == 1 else marker * a + b
            if profile == 'negative_byte':
                old = (~old) & ((1 << bits) - 1)
            values = [old] + ([z] if len(heap_ids[0]) > 1 else [])
            if len(heap_ids[0]) > 2:
                values.append(b)
            original = values[:]
            active = (
                not c
                if profile == 'open'
                else (
                    c and d
                    if profile == 'nested'
                    else (
                        bool(old & 1)
                        if profile == 'self'
                        else (
                            not (old & 1)
                            if profile == 'self_open'
                            else True if profile in ('byte_plain', 'scatter') else bool(c)
                        )
                    )
                )
            )
            history = []
            if point['tag'] != 'before' and active:
                if profile in (
                    'byte',
                    'byte_plain',
                    'partial_borrow',
                    'negative_byte',
                    'fill',
                    'fill_two',
                    'heap_copy',
                ):
                    mask = (1 << (8 * count)) - 1
                    incoming = (
                        z if profile == 'heap_copy' else sum(x << (8 * i) for i in range(count))
                    )
                    if count:
                        history.append(old & mask)
                        values[0] = (old & ~mask) | incoming
                elif profile in ('move_left', 'move_right'):
                    words = count // (bits // 8)
                    for i in range(words):
                        destination = i if profile == 'move_left' else i + 1
                        origin = i + 1 if profile == 'move_left' else i
                        history.append(original[destination])
                        values[destination] = original[origin]
                elif 'scatter' in profile:
                    history = [original[i] if i == d else 0 for i in range(2)]
                    values[d] = x
                else:
                    history.append(old)
                    values[0] = x
            elif point['tag'] != 'before':
                # Histories are zero on inactive amplitudes, with fixed arity.
                if profile in ('move_left', 'move_right'):
                    history = [0] * (count // (bits // 8))
                elif 'scatter' in profile:
                    history = [0, 0]
                elif profile in (
                    'byte',
                    'partial_borrow',
                    'negative_byte',
                    'fill',
                    'fill_two',
                    'heap_copy',
                ):
                    history = [0] if count else []
                else:
                    history = [0]
            if point['tag'] != 'before' and profile == 'self_open':
                history.append(int(bool(old & 1)))
            outputs = [a, b, z, x, c, d, *values]
            if profile == 'heap_copy':
                outputs.append(z)
            saved = bool(old ^ 1) if bits == 1 else old ^ 1
            if point['tag'] == 'borrowed':
                outputs.append(saved)
            angle = 0.11 * a + 0.23 * b + 0.31 * z + 0.41 * x + 0.17 * c + 0.29 * d
            if borrowed and point['tag'] != 'before':
                angle += 0.47 * bool(saved & 1)
                if point['tag'] != 'borrowed':
                    angle += 0.13 * bool(saved & marker)
            local = shift = 0
            for register, value in zip(registers, outputs, strict=True):
                local |= (int(value) & ((1 << len(register['wires'])) - 1)) << shift
                shift += len(register['wires'])
            expected_blocks.setdefault(tuple(history), {})[local] = cmath.exp(1j * angle) / 8
        actual, expected = actual_density(point, registers), density(expected_blocks)
        error = max(
            abs(actual.get(k, 0j) - expected.get(k, 0j)) for k in actual.keys() | expected.keys()
        )
        assert error < 1e-10, (type_name, profile, count, point['tag'], error)
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

    def run(label, command, environment=env):
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
        assert result.returncode == 0, result.stdout[-6000:]
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
    for type_name, profile in itertools.product(TYPES, PROFILES):
        if not supported(type_name, profile):
            continue
        path = output / (type_name + '-' + profile + '.c')
        path.write_text(source(type_name, profile))
        bits = width(type_name)
        counts = (
            (0, bits // 8)
            if profile == 'heap_copy'
            else (
                (0, 2 * (bits // 8))
                if profile in ('move_left', 'move_right')
                else (
                    (0, 2)
                    if profile == 'fill_two'
                    else (
                        (0, 1)
                        if profile
                        in ('byte', 'byte_plain', 'partial_borrow', 'negative_byte', 'fill')
                        else (1,)
                    )
                )
            )
        )
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
            for policy, count in itertools.product((0, 1), counts):
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
        public_density_comparisons=states,
        joint_gate_state_comparisons=states,
        complete_gate_inverses=inverses,
        compiled_programs=programs,
        levels=args.levels,
        synthesis_policies=[0, 1],
        sanitized=args.sanitized,
        scope='Finite native heap root stores, byte patches, copies/fills, overlapping moves and selected writes. '
        'Public reduced density matrices use explicit virtual old-value histories; physical joint states use independent gate evolution/reversal. '
        'Includes phased independent roots, untouched high-byte coherence, scoped borrowed phases and post-free retained outputs. '
        'Bool stores/scatter; signed/unsigned 8/16/32-bit scalar and byte profiles; multiword moves/scatter through 16 bits. '
        'Two 32-bit targets plus two full-word histories exceed 128 wires. No general ABI, lifetime, C11 or QPU qualification.',
    )
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary), flush=True)


if __name__ == '__main__':
    main()
