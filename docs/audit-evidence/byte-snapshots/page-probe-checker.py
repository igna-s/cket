from pathlib import Path
import json
import os
import subprocess
import sys

root = Path(__file__).resolve().parent.parent
candidate = root / 'build/byte-snapshot-prototype'
directory = candidate / 'build' / (sys.argv[1] if len(sys.argv) > 1 else 'nul-page-before')
directory.mkdir(exist_ok=False)
source = directory / 'guarded-nul.c'
source.write_text('''#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#ifdef __CKET__
#include <quantum.h>
#else
#define quant
#define measure(value) (value)
#endif
_Alignas(4096) static unsigned char reference[8192];
int main(void) {
    if (sysconf(_SC_PAGESIZE)!=4096) abort();
    reference[4095]=0;
    if (mprotect(reference+4096,4096,PROT_NONE)) abort();
    quant uint16_t input=0;
    quant bool result=strncmp((const char*)&input,(const char*)reference+4095,2)<=0;
    if (mprotect(reference+4096,4096,PROT_READ|PROT_WRITE)) abort();
    unsigned actual=measure(result);measure(input);
    return actual!=1;
}
'''.replace('#include <stdint.h>', '#include <stdint.h>\n#include <stdbool.h>'))
rows = []
for configuration, command in (
    ('ordinary-c', ['gcc-11', '-std=c11', '-O2', str(source), '-o', str(directory / 'ordinary.out')]),
    ('candidate', [str(candidate / 'build/cket'), str(source), '-O2', '-o', str(directory / 'candidate.out')]),
):
    compiled = subprocess.run(command, cwd=candidate, env=dict(os.environ, CC='gcc-11'), capture_output=True, text=True, timeout=120)
    if compiled.returncode:
        rows.append({'configuration': configuration, 'compile_returncode': compiled.returncode, 'stderr': compiled.stderr})
        continue
    result = subprocess.run([str(directory / ('ordinary.out' if configuration == 'ordinary-c' else 'candidate.out'))], cwd=candidate, env=dict(os.environ, CC='gcc-11'), capture_output=True, text=True, timeout=60)
    rows.append({'configuration': configuration, 'compile_returncode': 0, 'run_returncode': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr})
(directory / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')
print(json.dumps(rows, indent=2))
if len(sys.argv) > 2 and sys.argv[2] == '--expect-fixed':
    assert all(row['compile_returncode'] == 0 and row['run_returncode'] == 0 for row in rows), rows
