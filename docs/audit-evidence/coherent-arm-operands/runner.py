"""Distinguish lazy condition arithmetic from arithmetic in controlled bodies."""
from pathlib import Path
import hashlib
import json
import os
import subprocess

root = Path('/tmp/quantum-audit-isolated-20261017')
out = root / 'build/coherent-arm-operand-probes-v1'
out.mkdir()
profiles = {
    'xor': 'x ^= 10/k;',
    'add': 'x += 10/k;',
    'assignment': 'x = 10/k;',
    'mixed-add': 'x = x + 10/k;',
    'memset': 'memset(&x,10/k,1);',
    'abs': 'x = abs(10/k);',
    'phase': 'phase(10/k);',
    'condition-control': 'if(x + 10/k > 1) phase(.17);',
}
owners = [('main134', root), ('corrected137', root / 'build/coherent-cpu-maintained')]
rows = []
env = dict(os.environ, CC='gcc-11')
for name, operation in profiles.items():
    source = out / (name + '.c')
    source.write_text('#include <quantum.h>\n#include <string.h>\n#include <stdlib.h>\n'
                      'int main(int argc,char **argv){int k=argc>1?argv[1][0]-\'0\':0;'
                      'quant bool b=0;quant uint8_t x=5;if(b){' + operation + '}'
                      'unsigned a=measure(x),v=measure(b);return a!=5||v;}\n')
    ordinary = out / (name + '-reference.c')
    ordinary.write_text('#include <stdint.h>\n#include <stdbool.h>\n#define quant\n'
                        '#define measure(x) (x)\n#define phase(x) ((void)(x))\n' +
                        source.read_text().replace('#include <quantum.h>\n', ''))
    reference = out / (name + '-reference')
    compiled = subprocess.run(['gcc-11', '-std=c11', '-O2', str(ordinary), '-o', str(reference)],
                              capture_output=True, text=True)
    assert compiled.returncode == 0, compiled.stderr
    result = subprocess.run([str(reference), '0'], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    row = {'name': name, 'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
           'ordinary_c_returncode': result.returncode}
    for label, owner in owners:
        binary = out / (name + '-' + label)
        result = subprocess.run([str(owner / 'build/cket'), str(source), '-O2', '-fbuiltin',
                                 '-o', str(binary)], cwd=owner, env=env, capture_output=True, text=True)
        detail = {'compile_returncode': result.returncode, 'compile_stderr': result.stderr,
                  'compiler_sha256': hashlib.sha256((owner / 'build/cket_plugin.so').read_bytes()).hexdigest()}
        if result.returncode == 0:
            result = subprocess.run([str(binary), '0'], capture_output=True, text=True)
            detail.update(run_returncode=result.returncode, run_stderr=result.stderr)
        row[label] = detail
    rows.append(row)
    (out / 'progress.json').write_text(json.dumps(rows, indent=2) + '\n')
    print(json.dumps(row), flush=True)
(out / 'results.json').write_text(json.dumps({
    'status': 'closed-exploratory-probes', 'optimization': '-O2', 'builtins': '-fbuiltin',
    'rows': rows, 'scope': 'Eight native controlled-body probes on the original main134 and '
    'the corrected137 compiler. Ordinary C references are separate. No broad proof, fix, '
    'general compatibility or physical execution claim.'}, indent=2) + '\n')
