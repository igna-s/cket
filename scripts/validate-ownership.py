#!/usr/bin/env python3
"""Check native ownership contracts against independent complex gate evolution."""

from __future__ import annotations

import argparse
import cmath
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parent.parent
HEADER = '#include <quantum.h>\n#include <string.h>\n#include <stdlib.h>\nvoid ownership_checkpoint(const char*);\n'
CASES = {
    'copy': '''int main(void) {
        quant uint8_t x=superposition(0,1), y=0, z=0;
        if(x==1) phase(.31);
        memcpy(&y,&x,1); memcpy(&z,&x,1);
        ownership_checkpoint("copied");
        if(y==1) phase(.17);
        z^=x; y^=x;
        ownership_checkpoint("inverse");
        measure(z); measure(y); measure(x); return 0;
    }''',
    'move': '''int main(void) {
        quant uint8_t x=superposition(0,1);
        if(x==1) phase(.31);
        quant uint8_t y=x;
        ownership_checkpoint("moved");
        measure(y); return 0;
    }''',
    'derived': '''int main(void) {
        quant uint8_t x=superposition(0,3);
        if(x==3) phase(.31);
        quant uint8_t result=x^1;
        ownership_checkpoint("computed");
        if(result==2) phase(.17);
        ownership_checkpoint("inverse");
        measure(x); return 0;
    }''',
    'overwrite': '''int main(void) {
        quant uint8_t x=superposition(0,3);
        if(x==3) phase(.31);
        quant uint8_t old=x^1;
        x=9;
        ownership_checkpoint("retained");
        if(old==2) phase(.17);
        ownership_checkpoint("inverse");
        measure(x); return 0;
    }''',
    'cpu_index': '''int main(void) {
        quant uint8_t values[2]={superposition(0,3),5};
        if(values[0]==3) phase(.31);
        size_t index=0;
        quant uint8_t result=values[index]^1;
        index=1;
        ownership_checkpoint("computed");
        if(result==2) phase(.17);
        ownership_checkpoint("inverse");
        measure(values[1]); measure(values[0]); return index!=1;
    }''',
    'quantum_index': '''int main(void) {
        quant bool index=superposition(0,1);
        quant uint8_t values[2]={3,9};
        if(index) phase(.31);
        quant uint8_t result=values[index]^1;
        ownership_checkpoint("computed");
        if(result==8) phase(.17);
        ownership_checkpoint("inverse");
        measure(values[1]); measure(values[0]); measure(index); return 0;
    }''',
    'cpu_scan': '''void scan(quant const uint8_t *values,size_t count,quant uint8_t total) {
        for(size_t i=0;i<count;++i) total+=values[i];
    }
    void undo(quant const uint8_t *values,size_t count,quant uint8_t total) {
        size_t i=0; while(i<count) {total-=values[i];++i;}
    }
    int main(void) {
        quant uint8_t values[2]={superposition(0,3),5}, total=0;
        if(values[0]==3) phase(.31);
        size_t count=2;
        scan(values,count,total);
        ownership_checkpoint("computed");
        if(total==8) phase(.17);
        undo(values,count,total);
        ownership_checkpoint("inverse");
        measure(total); measure(values[1]); measure(values[0]); return 0;
    }''',
}
CASES.update(
    {
        'pointer_index': '''quant uint8_t read_buffer(quant const uint8_t *p,quant const bool index) {
        return p[index]^3;
    }
    int main(void) {
        quant bool index=superposition(0,1);
        quant uint8_t values[3]={2,5,9};
        if(index)phase(.31);
        quant uint8_t *tail=values+1;
        quant uint8_t result=read_buffer(tail,index);
        ownership_checkpoint("computed");
        if(result==10)phase(.17);
        ownership_checkpoint("inverse");
        measure(values[2]);measure(values[1]);measure(values[0]);measure(index);return 0;
    }''',
        'pointer_word': '''quant uint16_t read_word(quant const uint16_t *p,quant const bool index) {
        return p[index]^0x100;
    }
    int main(void) {
        quant bool index=superposition(0,1);
        quant uint16_t values[2]={0x201,0x405};
        if(index)phase(.31);
        quant uint16_t result=read_word(values,index);
        ownership_checkpoint("computed");
        if(result==0x505)phase(.17);
        ownership_checkpoint("inverse");
        measure(values[1]);measure(values[0]);measure(index);return 0;
    }''',
        'coherent_write': '''int main(void) {
        quant bool index=superposition(0,1);
        quant uint8_t values[2]={3,9};
        if(index)phase(.31);
        values[index]=7;
        if(values[0]==7)phase(.17);
        ownership_checkpoint("written");
        measure(values[1]);measure(values[0]);measure(index);return 0;
    }''',
        'search_write': '''int main(void) {
        quant bool index=superposition(0,1);
        quant char text[3]={'a',index?'a':'b',0};
        if(index)phase(.31);
        char *last=strrchr(text,'a');
        (*last)++;
        *last='x';
        if(text[1]=='x')phase(.17);
        ownership_checkpoint("written");
        measure(text[2]);measure(text[1]);measure(text[0]);measure(index);return 0;
    }''',
    }
)
for decimal_name, result_type in (('atoi', 'int'), ('atol', 'int8_t'), ('atoll', 'int8_t')):
    CASES['decimal_' + decimal_name] = (
        '''int main(void) {
        quant bool index=superposition(0,1);
        quant char text[3]={index?'1':'2','3',0};
        if(index)phase(.31);
        quant RESULT_TYPE result=DECIMAL_NAME((const char*)text);
        ownership_checkpoint("computed");
        if(result==13)phase(.17);
        ownership_checkpoint("inverse");
        measure(text[2]);measure(text[1]);measure(text[0]);measure(index);return 0;
    }'''.replace(
            'RESULT_TYPE', result_type
        ).replace(
            'DECIMAL_NAME', decimal_name
        )
    )

NEGATIVE = {
    'use_after_move': (
        'int main(void){quant uint8_t x=superposition(0,1);quant uint8_t y=x;measure(x);measure(y);}',
        'requires available quantum storage',
    ),
    'borrow_move': (
        'void f(quant uint8_t x){quant uint8_t y=x;measure(y);} int main(void){quant uint8_t x=1;f(x);measure(x);}',
        'borrow',
    ),
    'readonly_store': (
        'void f(quant const uint8_t *x,quant const bool b){x[b]=1;} int main(void){quant uint8_t x[2]={1,2};quant bool b=superposition(0,1);f(x,b);measure(x[0]);measure(x[1]);measure(b);}',
        'read-only',
    ),
    'self_add': (
        'int main(void){quant uint8_t x=1;quant uint8_t *p=&x;size_t i=0;x+=p[i];measure(x);}',
        'independent',
    ),
}
ARITHMETIC_DIAGNOSTICS = ('overflow', 'typed modular addition domain could not be proven')
DOMAINS = {
    'cpu_bounds': (
        'int main(void){quant uint8_t x[2]={1,2};size_t i=2;quant uint8_t y=x[i]^1;measure(y);measure(x[0]);measure(x[1]);}',
        'division',
    ),
    'pointer_wrap': (
        'int main(void){quant uint16_t x[2]={1,2};quant uint16_t *p=x;size_t i=(size_t)1<<63;quant uint16_t y=p[i]^1;measure(y);measure(x[0]);measure(x[1]);}',
        'division',
    ),
    'quantum_bounds': (
        'int main(void){quant uint8_t x[2]={1,2};quant uint8_t i=superposition(0,3);quant uint8_t y=x[i]^1;measure(y);measure(x[0]);measure(x[1]);measure(i);}',
        'division',
    ),
    'signed_overflow': (
        '#include <limits.h>\nint main(void){quant uint8_t x=1;quant int y=INT_MAX;x+=y;measure(x);measure(y);}',
        ARITHMETIC_DIAGNOSTICS,
    ),
    'signed_underflow': (
        '#include <limits.h>\nint main(void){quant uint8_t x=1;quant int y=INT_MIN;x-=y;measure(x);measure(y);}',
        ARITHMETIC_DIAGNOSTICS,
    ),
    'nested_overflow': (
        '#include <limits.h>\nint main(void){quant uint8_t x=1;quant int y=INT_MAX;x+=(y+1);measure(x);measure(y);}',
        ARITHMETIC_DIAGNOSTICS,
    ),
    'wide_overflow': (
        '#include <limits.h>\nint main(void){quant uint32_t x=1;quant long long y=LLONG_MAX;x+=y;measure(x);measure(y);}',
        ARITHMETIC_DIAGNOSTICS,
    ),
    'controlled_overflow': (
        '#include <limits.h>\nint main(void){quant bool b=superposition(0,1);quant uint8_t x=1;quant int y=INT_MAX;if(b)x+=y;measure(x);measure(y);measure(b);}',
        ARITHMETIC_DIAGNOSTICS,
    ),
}

# Explicit casts and unsigned arithmetic retain their C conversion semantics.
VALID_UPDATES = {
    'signed_boundary': ('int', 'INT_MAX-1', 'y', '+=', 255),
    'explicit_unsigned_cast': ('int', 'INT_MAX', '(uint8_t)y', '+=', 0),
    'explicit_signed_cast': ('int', 'INT_MAX', '(int8_t)y', '+=', 0),
    'negative_addend': ('int', 'INT_MIN', 'y', '+=', 1),
    'unsigned_wrap': ('unsigned', 'UINT_MAX', 'y', '+=', 0),
    'signed_subtract': ('int', 'INT_MIN+2', 'y', '-=', 255),
}
VALID_PROGRAMS = {
    name: '#include <limits.h>\nint main(void){quant uint8_t x=1;quant TYPE y=INIT;x UPDATE VALUE;unsigned result=measure(x);measure(y);return result!=EXPECTED;}'.replace(
        'TYPE', source_type
    )
    .replace('INIT', initial)
    .replace('UPDATE', update)
    .replace('VALUE', value)
    .replace('EXPECTED', str(expected))
    for name, (source_type, initial, value, update, expected) in VALID_UPDATES.items()
}
VALID_PROGRAMS['inactive_overflow'] = (
    '#include <limits.h>\nint main(void){quant bool b=0;quant uint8_t x=1;quant int y=INT_MAX;if(b)x+=y;unsigned result=measure(x);measure(y);measure(b);return result!=1;}'
)


def evolve(state: dict[int, complex], gates: list) -> dict[int, complex]:
    for kind, wire, mask, pattern, angle in gates:
        mask, pattern = int(mask, 0), int(pattern, 0)
        updated: dict[int, complex] = {}
        for basis, amplitude in state.items():
            if basis & mask != pattern:
                updated[basis] = updated.get(basis, 0j) + amplitude
            elif kind == 0:
                low = basis & ~(1 << wire)
                high = low | (1 << wire)
                updated[low] = updated.get(low, 0j) + amplitude / math.sqrt(2)
                updated[high] = updated.get(high, 0j) + amplitude * (
                    -1 if basis >> wire & 1 else 1
                ) / math.sqrt(2)
            elif kind == 1:
                destination = basis ^ (1 << wire)
                updated[destination] = updated.get(destination, 0j) + amplitude
            elif kind == 2:
                updated[basis] = updated.get(basis, 0j) + amplitude * cmath.exp(1j * angle)
            else:
                raise AssertionError('implicit measurement/reset before checkpoint')
        state = {basis: amplitude for basis, amplitude in updated.items() if abs(amplitude) > 1e-14}
    return state


def equal_states(actual: dict[int, complex], expected: dict[int, complex]) -> None:
    for basis in actual.keys() | expected.keys():
        assert abs(actual.get(basis, 0j) - expected.get(basis, 0j)) < 1e-10, (
            hex(basis),
            actual.get(basis),
            expected.get(basis),
        )


def expected_state(case: str, point: dict) -> dict[int, complex]:
    registers = {r['name']: r for r in point['registers'] if r['name'] is not None}
    inverse = point['tag'] == 'inverse'
    count = 2 if case in ('copy', 'move', 'quantum_index', 'pointer_index', 'pointer_word') else 4
    expected = {}
    for label in range(count):
        values = {}
        phase = (0.31 if label == (1 if count == 2 else 3) else 0) + (
            0.17 if inverse and label == (1 if count == 2 else 3) else 0
        )
        if case == 'copy':
            values = {'x': label, 'y': 0 if inverse else label, 'z': 0 if inverse else label}
        elif case == 'move':
            values = {'y': label}
            assert 'x' not in registers, 'move retained a second public owner'
        elif case == 'derived':
            values = {'x': label}
            if not inverse:
                values['result'] = label ^ 1
        elif case == 'overwrite':
            values = {'x': 9, '__history_0': label}
            if not inverse:
                values['old'] = label ^ 1
        elif case in ('cpu_index', 'cpu_scan'):
            values = {'values[0]': label, 'values[1]': 5}
            if case == 'cpu_scan':
                values['total'] = 0 if inverse else label + 5
            elif not inverse:
                values['result'] = label ^ 1
        elif case == 'quantum_index':
            values = {'index': label, 'values[0]': 3, 'values[1]': 9}
            if not inverse:
                values['result'] = (9 if label else 3) ^ 1
                values.update(
                    {name: label for name in registers if name.startswith('__cket_index_')}
                )
        elif case in ('pointer_index', 'pointer_word'):
            word = case == 'pointer_word'
            values = {
                'index': label,
                'values[0]': 0x201 if word else 2,
                'values[1]': 0x405 if word else 5,
            }
            if not word:
                values['values[2]'] = 9
            if not inverse:
                values['result'] = (
                    ((0x405 if label else 0x201) ^ 0x100) if word else ((9 if label else 5) ^ 3)
                )
                values.update(
                    {name: label for name in registers if name.startswith('__cket_index_')}
                )
        basis = 0
        for name, value in values.items():
            assert name in registers, (case, point['tag'], name, sorted(registers))
            for bit, wire in enumerate(registers[name]['wires']):
                basis |= ((value >> bit) & 1) << wire
        expected[basis] = cmath.exp(1j * phase) / math.sqrt(count)
    return expected


def check_retained_semantics(case: str, point: dict, native: dict[int, complex]) -> None:
    registers = {r['name']: r for r in point['registers'] if r['name'] is not None}

    def read(basis: int, name: str) -> int:
        return sum(
            ((basis >> wire) & 1) << bit for bit, wire in enumerate(registers[name]['wires'])
        )

    labels = set()
    for basis, amplitude in native.items():
        label = read(basis, 'index')
        assert label not in labels and label in (0, 1)
        labels.add(label)
        if case == 'coherent_write':
            assert read(basis, 'values[0]') == (3 if label else 7)
            assert read(basis, 'values[1]') == (7 if label else 9)
            phase = 0.31 * label + 0.17 * (not label)
        elif case == 'search_write':
            assert read(basis, 'text[0]') == (97 if label else 120)
            assert read(basis, 'text[1]') == (120 if label else 98)
            assert read(basis, 'text[2]') == 0
            phase = 0.48 * label
        else:
            assert read(basis, 'text[0]') == (49 if label else 50)
            assert read(basis, 'text[1]') == 51 and read(basis, 'text[2]') == 0
            phase = (0.48 if point['tag'] == 'inverse' else 0.31) * label
            if point['tag'] != 'inverse':
                assert read(basis, 'result') == (13 if label else 23)
            else:
                public = {'index', 'text[0]', 'text[1]', 'text[2]'}
                keep = sum(1 << wire for name in public for wire in registers[name]['wires'])
                assert basis & ~keep == 0, 'decimal inverse left nonzero private workspace'
        assert abs(amplitude - cmath.exp(1j * phase) / math.sqrt(2)) < 1e-10
    assert labels == {0, 1}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler-root', type=Path, default=ROOT)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/ownership-validation')
    parser.add_argument('--levels', nargs='+', default=['-O2'])
    parser.add_argument('--sanitized', action='store_true')
    args = parser.parse_args()
    args.levels = [level if level.startswith('-') else '-' + level for level in args.levels]
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    compiler_root = args.compiler_root.resolve()
    base_env = dict(os.environ, CC='gcc-11', CKET_SIMULATOR='sparse', CKET_SEED='7')
    if args.sanitized:
        base_env.update(CC=str(compiler_root / 'build/gcc-wrapper'), CKET_REAL_GCC='gcc-11')
    records = []

    def run(command: list[str], label: str, *, env=None) -> subprocess.CompletedProcess:
        result = subprocess.run(
            command,
            cwd=compiler_root,
            env=base_env if env is None else env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=120,
        )
        (output / (label + '.log')).write_text(result.stdout)
        records.append({'command': command, 'exit_code': result.returncode, 'log': label + '.log'})
        (output / 'commands.json').write_text(json.dumps(records, indent=2) + '\n')
        return result

    flags = (
        [
            '-O1',
            '-g',
            '-fsanitize=address,undefined',
            '-fno-omit-frame-pointer',
            '-fno-pie',
            '-no-pie',
        ]
        if args.sanitized
        else []
    )
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
                ['nm', '-u', str(compiler_root / 'build' / component)],
                'instrumentation-' + component,
            )
            assert result.returncode == 0 and '__asan_init' in result.stdout, component
            assert '__ubsan_handle' in result.stdout, component
    adapter = output / 'ownership_trace.o'
    result = run(
        [
            'gcc-11',
            '-std=c11',
            '-O2',
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
    for level in args.levels:
        for case, body in CASES.items():
            source = output / (case + '.c')
            source.write_text(HEADER + body + '\n')
            binary = output / (case + level)
            result = run(
                [
                    str(compiler_root / 'build/cket'),
                    str(source),
                    level,
                    *flags,
                    *links,
                    '-o',
                    str(binary),
                ],
                case + level + '-compile',
            )
            assert result.returncode == 0, result.stdout
            for tables in (0, 1):
                env = dict(
                    base_env,
                    OWNERSHIP_TABLES=str(tables),
                )
                result = run([str(binary)], case + level + f'-tables{tables}', env=env)
                assert result.returncode == 0, result.stdout[-3000:]
                state = {0: 1 + 0j}
                snapshots = [
                    json.loads(line) for line in result.stdout.splitlines() if line.startswith('{')
                ]
                assert len(snapshots) == (
                    1 if case in ('move', 'coherent_write', 'search_write') else 2
                )
                recorded = []
                for point in snapshots:
                    assert not point['measurements'] and not point['resets']
                    state = evolve(state, point['gates'])
                    recorded.extend(point['gates'])
                    native = {
                        int(basis, 0): complex(real, imaginary)
                        for basis, real, imaginary in point['state']
                    }
                    equal_states(native, state)
                    if case in ('coherent_write', 'search_write') or case.startswith('decimal_'):
                        check_retained_semantics(case, point, native)
                    else:
                        equal_states(native, expected_state(case, point))
                    points += 1
                inverse_gates = [
                    [kind, wire, mask, pattern, -angle if kind == 2 else angle]
                    for kind, wire, mask, pattern, angle in reversed(recorded)
                ]
                equal_states(evolve(state, inverse_gates), {0: 1 + 0j})
                print(
                    f'PASS {case} {level} tables={tables}: {len(snapshots)} state checkpoints',
                    flush=True,
                )
        for case, (body, diagnostic) in NEGATIVE.items():
            source = output / (case + '.c')
            source.write_text(HEADER + body + '\n')
            result = run(
                [str(compiler_root / 'build/cket'), str(source), level, '-o', str(output / case)],
                case + level + '-compile',
            )
            assert result.returncode > 0 and diagnostic in result.stdout, result.stdout
        for case, body in VALID_PROGRAMS.items():
            source = output / (case + '.c')
            source.write_text(HEADER + body + '\n')
            binary = output / (case + level)
            result = run(
                [str(compiler_root / 'build/cket'), str(source), level, *flags, '-o', str(binary)],
                case + level + '-compile',
            )
            assert result.returncode == 0, result.stdout
            result = run(
                [str(binary)],
                case + level + '-run',
            )
            assert result.returncode == 0, result.stdout
        for case, (body, diagnostic) in DOMAINS.items():
            source = output / (case + '.c')
            source.write_text(HEADER + body + '\n')
            binary = output / (case + level)
            result = run(
                [str(compiler_root / 'build/cket'), str(source), level, *flags, '-o', str(binary)],
                case + level + '-compile',
            )
            if result.returncode == 0:
                result = run(
                    [str(binary)],
                    case + level + '-run',
                )
            diagnostics = (diagnostic,) if isinstance(diagnostic, str) else diagnostic
            assert result.returncode > 0 and any(
                part in result.stdout.lower() for part in diagnostics
            ), result.stdout
    summary = {
        'status': 'passed',
        'complete_state_checkpoints': points,
        'profiles': list(CASES),
        'levels': args.levels,
        'synthesis_policies': [0, 1],
        'sanitized': args.sanitized,
        'tolerance': 1e-10,
        'reference': 'Independent evolution of the entire joint gate state; full-state formulas for copy/move/pure consumers; public values and phases with retained garbage for writes/decimal; private-zero checks after decimal cleanup.',
        'inverse_gate_checks': len(CASES) * len(args.levels) * 2,
        'ownership_rejections': len(NEGATIVE) * len(args.levels),
        'undefined_domain_rejections': len(DOMAINS) * len(args.levels),
        'valid_conversion_programs': len(VALID_PROGRAMS) * len(args.levels),
        'sanitizer_scope': (
            'plugin, driver, engine, runtime, fixture and tracing adapter'
            if args.sanitized
            else 'none'
        ),
        'compiler_sha256': hashlib.sha256(
            (compiler_root / 'build/cket_plugin.so').read_bytes()
        ).hexdigest(),
    }
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary), flush=True)


if __name__ == '__main__':
    main()
