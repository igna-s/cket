"""Make the isolated address-difference tests part of a recurrent C validator."""
from pathlib import Path
import hashlib
import importlib.util
import json
import shutil
import sys

root = Path('/tmp/quantum-audit-isolated-20261017')
base = root / 'build/pointer-difference-prototype'
candidate = root / 'build/pointer-difference-maintained'
candidate.mkdir()
(candidate / 'build').mkdir()
report = json.loads((base / 'build/difference-native-v2/results.json').read_text())
frozen = report['source_sha256']
for name, expected in frozen.items():
    source = base / name
    assert hashlib.sha256(source.read_bytes()).hexdigest() == expected, name
    target = candidate / name
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, target)
shutil.copyfile(root / 'LICENSE', candidate / 'LICENSE')
(candidate / '.deps').mkdir()
(candidate / '.deps/gcc').symlink_to(root / '.deps/gcc', target_is_directory=True)
native = [row['name'] for row in report['rows'] if row['optimization'] == '-O0' and row['builtins'] == '-fbuiltin' and row.get('kind') != 'diagnostic']
negative = [row['name'] for row in report['rows'] if row['optimization'] == '-O0' and row['builtins'] == '-fbuiltin' and row.get('kind') == 'diagnostic']
assert len(native) == 8 and len(negative) == 4


def cstring(text):
    return '\n'.join('     ' + json.dumps(line + '\n') for line in text.splitlines())


cases = []
for name in native + negative:
    program = (base / 'build/difference-native-v1' / (name + '.c')).read_text()
    diagnostic = 'NULL' if name in native else json.dumps('|c>: quantum reference cannot escape' if name == 'null-address' else '|c>: quantum pointer subtraction')
    cases.append('    {' + json.dumps(name) + ',\n' + cstring(program) + ',\n     ' + diagnostic + '}')
pending = (base / 'build/difference-states-v1/difference.c').read_text()
cases.append('    {"pending-cpu-scalar-control",\n' + cstring(pending) + ',\n     "|c>: classical branch inside quantum control is not supported yet"}')
state = (base / 'build/difference-states-v2/difference.c').read_text()
text = '''#define _POSIX_C_SOURCE 200809L
#include "internal.h"
#include "validation.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Profile 4 checks the same two-label coherent write and .48-radian phase.
 * Reuse the existing independent action/inverse checker without changing it. */
#include "memory_return_state.inc"

typedef struct {
    const char *name, *program, *diagnostic;
} difference_case;

static const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
static const difference_case cases[] = {
''' + ',\n'.join(cases) + '\n};\n\nstatic const difference_case coherent = {"coherent-distance",\n' + cstring(state) + ',\n     NULL};\n' + '''
static char *compile_case(validation *v, const difference_case *test,
                          unsigned level, unsigned builtin, int execute) {
    char source[4096], output[4096], compiler[4096];
    validation_path(source, sizeof source, "%s/difference.c", v->temp);
    validation_path(output, sizeof output, "%s/difference.out", v->temp);
    validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
    validation_write(source, test->program);
    char *command[] = {compiler, source, execute ? "--run" : "--emit-ir",
                       (char *)levels[level], builtin ? "-fbuiltin" : "-fno-builtin",
                       "-o", output, NULL};
    int status = validation_run(v, command, NULL, 240);
    char *log = validation_read(v->log);
    if (test->diagnostic ? !validation_rejected(status, log, test->diagnostic) : status != 0)
        validation_fail("pointer difference %s %s builtins=%u failed:\\n%s\\n%s",
                        test->name, levels[level], builtin, test->program, log);
    free(log);
    return !execute && !test->diagnostic ? validation_read(output) : NULL;
}

static unsigned reference(validation *v, const difference_case *test) {
    static const char header[] = "#include <quantum.h>\\n";
    static const char preamble[] = "#include <stdint.h>\\n#include <stdbool.h>\\n"
                                   "#define quant\\n#define measure(x) (x)\\n";
    const char *start = strstr(test->program, header);
    if (start != test->program)
        validation_fail("unexpected pointer difference reference preamble");
    size_t size = sizeof preamble + strlen(test->program);
    char *program = malloc(size);
    if (!program)
        validation_fail("pointer difference reference allocation");
    snprintf(program, size, "%s%s", preamble, start + sizeof header - 1);
    char source[4096], binary[4096];
    validation_path(source, sizeof source, "%s/reference.c", v->temp);
    validation_path(binary, sizeof binary, "%s/reference.out", v->temp);
    validation_write(source, program);
    free(program);
    const char *cc = getenv("CC");
    for (unsigned level = 0; level < 5; ++level) {
        char *command[] = {(char *)(cc ? cc : "gcc"), "-std=c11", (char *)levels[level],
                           "-Werror=implicit-function-declaration", source, "-o", binary, NULL};
        char *run[] = {binary, NULL};
        if (validation_run(v, command, NULL, 60) || validation_run(v, run, NULL, 60)) {
            char *log = validation_read(v->log);
            validation_fail("ordinary C pointer distance %s %s failed:\\n%s", test->name, levels[level], log);
        }
    }
    return 5;
}

int main(int argc, char **argv) {
    if (argc != 1)
        validation_fail("usage: validate_pointer_differences");
    validation v;
    validation_init(&v, argv[0]);
    unsigned native = 0, rejected = 0, references = 0, circuits = 0, maximum = 0;
    for (unsigned i = 0; i < sizeof cases / sizeof *cases; ++i) {
        const difference_case *test = &cases[i];
        if (!test->diagnostic)
            references += reference(&v, test);
        for (unsigned level = 0; level < 5; ++level)
            for (unsigned builtin = 0; builtin < 2; ++builtin) {
                free(compile_case(&v, test, level, builtin, !test->diagnostic));
                if (test->diagnostic) ++rejected;
                else ++native;
            }
    }
    printf("PASS %u native pointer distances and %u rejection contracts; %u ordinary C executions separate; pending CPU-scalar coherent conditions remain rejections.\\n", native, rejected, references);
    fflush(stdout);
    for (unsigned level = 0; level < 5; ++level)
        for (unsigned builtin = 0; builtin < 2; ++builtin) {
            char *ir = compile_case(&v, &coherent, level, builtin, 0);
            circuits += memory_return_state_probe(ir, 4, &maximum);
            free(ir);
        }
    printf("PASS %u pointer-distance action/inverse circuits: both Boolean labels, .48-radian relative phase, saved write/definition inverses and private zeros; both synthesis policies, maximum %u physical wires.\\n", circuits, maximum);
    validation_cleanup(&v);
    printf("%u pointer difference checks passed; native distances, finite coherent action and unsupported contexts have separate scopes.\\n", native + rejected + circuits);
    return 0;
}
'''
(candidate / 'tests/validate_pointer_differences.c').write_text(text)
edits = {
    'scripts/build.sh': ('"$gcc_compiler" "${flags[@]}" tests/validate_memory_returns.c tests/validation.c build/libcket_quantum.a -lm -o build/validate_memory_returns\n',
                         '"$gcc_compiler" "${flags[@]}" tests/validate_pointer_differences.c tests/validation.c build/libcket_quantum.a -lm -o build/validate_pointer_differences\n'),
    'scripts/sanitize-all.sh': ('"$real_cc" "${flags[@]}" tests/validate_memory_returns.c tests/validation.c "$san_root/build/libcket_quantum.a" -lm -o "$san_root/build/validate_memory_returns"\n',
                              '"$real_cc" "${flags[@]}" tests/validate_pointer_differences.c tests/validation.c "$san_root/build/libcket_quantum.a" -lm -o "$san_root/build/validate_pointer_differences"\n'),
    'scripts/verify.sh': ('build/validate_memory_returns 2>&1 | tee build/verify-memory-returns.log\n',
                        'build/validate_pointer_differences 2>&1 | tee build/verify-pointer-differences.log\n'),
}
for name, (marker, extra) in edits.items():
    path = candidate / name
    source = path.read_text()
    assert source.count(marker) == 1, name
    source = source.replace(marker, marker + extra)
    if name.endswith('sanitize-all.sh'):
        marker = '"$san_root/build/validate_memory_returns"\n'
        assert source.count(marker) == 2
        index = source.rfind(marker)
        source = source[:index] + source[index:].replace(marker, marker + '"$san_root/build/validate_pointer_differences"\n', 1)
    path.write_text(source)
path = candidate / 'scripts/archive-audit.py'
source = path.read_text()
marker = "        'memory_returns': "
line = next(line for line in source.splitlines(keepends=True) if line.startswith(marker))
source = source.replace(line, line + "        'pointer_differences': r'^150 pointer difference checks passed; native distances, finite coherent action and unsupported contexts have separate scopes\\.$',\n")
marker = "            'memory_return_checks': 290,\n"
assert source.count(marker) == 1
source = source.replace(marker, marker + "            'pointer_difference_checks': 150,\n")
path.write_text(source)
sys.path.insert(0, str(root / '.deps/format-venv/lib/python3.10/site-packages'))
spec = importlib.util.spec_from_file_location('maintained_difference_format', candidate / 'scripts/format.py')
formatter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(formatter)
formatter.CLANG = root / '.deps/format-venv/bin/clang-format'
formatter.SHFMT = root / '.deps/format-tools/shfmt'
changed = ['tests/validate_pointer_differences.c', *edits, 'scripts/archive-audit.py']
for name in changed:
    path = candidate / name
    formatted = formatter.format_text(path, path.read_text())
    assert formatter.format_text(path, formatted) == formatted, name
    path.write_text(formatted)
spec = importlib.util.spec_from_file_location('maintained_difference_sources', candidate / 'scripts/archive-audit.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
snapshot = module.source_hashes()
assert len(snapshot) == 137
(candidate / 'build/maintained-sources.json').write_text(json.dumps(snapshot, indent=2) + '\n')
print('Prepared recurrent 150-check validator on 137 frozen inputs; compiler extension unchanged from the scoped proof. No main changes.')
