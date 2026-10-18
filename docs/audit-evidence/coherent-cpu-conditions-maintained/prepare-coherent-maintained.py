"""Promote the newly supported CPU scalar condition into positive regressions."""
from pathlib import Path
import hashlib
import importlib.util
import json
import shutil
import sys

root=Path('/tmp/quantum-audit-isolated-20261017')
base=root/'build/coherent-cpu-branch-v3'
candidate=root/'build/coherent-cpu-maintained'
candidate.mkdir()
(candidate/'build').mkdir()
frozen=json.loads((base/'build/sources.json').read_text())
for name,expected in frozen.items():
    source=base/name
    assert hashlib.sha256(source.read_bytes()).hexdigest()==expected,name
    target=candidate/name
    target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(source,target)
shutil.copyfile(base/'LICENSE',candidate/'LICENSE')
(candidate/'.deps').mkdir()
(candidate/'.deps/gcc').symlink_to(root/'.deps/gcc',target_is_directory=True)


def cstring(text):
    return '\n'.join('     '+json.dumps(line+'\n') for line in text.splitlines())


path=candidate/'tests/validate_pointer_differences.c'
source=path.read_text()
start=source.index('    {"pending-cpu-scalar-control",')
end=source.index('\n\nstatic const difference_case coherent',start)
program=(base/'build/native-v2/address-distance.c').read_text()
native=program.replace('quant bool b=superposition(0,1);','quant bool b=1;')
native=native.replace('if(b)phase(.31);','').replace('phase(.17);','')
source=source[:start]+'    {"cpu-scalar-control",\n'+cstring(native)+',\n     NULL}};'+source[end:]
marker='static char *compile_case('
assert source.count(marker)==1
source=source.replace(marker,'static const difference_case coherent_scalar = {"coherent-scalar-distance",\n'+cstring(program)+',\n     NULL};\n\n'+marker)
marker='int main(int argc, char **argv) {'
assert source.count(marker)==1
helper='''/* This fixture has exactly one classical input: its source-level distance
 * is one. Bind that explicit precondition before synthesis. Actual native
 * captures and independent phase/inverse checks are archived separately. */
static char *bind_unit_distance(const char *ir) {
    cj_doc document = {0};
    if (!cj_parse(&document, ir))
        validation_fail("invalid scalar-distance template");
    char rows[4096] = "[";
    size_t length = 1;
    unsigned bindings = 0, index = 0;
    int list = cj_get(&document, 0, "instructions");
    for (int token = list + 1; token < document.tokens[list].next;
         token = document.tokens[token].next, ++index) {
        if (!cj_equal(&document, cj_get(&document, token, "op"), "qbind"))
            continue;
        int parameters = cj_get(&document, token, "parameters");
        uint64_t bits;
        if (parameters < 0 || document.tokens[parameters].children != 1 ||
            !cj_u64(&document, cj_get(&document, parameters + 1, "bits"), &bits) || bits != 64)
            validation_fail("unit-distance fixture acquired an unexpected CPU input");
        int added = snprintf(rows + length, sizeof rows - length,
                             "%s{\\"instruction\\":%u,\\"values\\":[1]}", bindings++ ? "," : "", index);
        if (added < 0 || (size_t)added >= sizeof rows - length)
            validation_fail("unit-distance binding budget exceeded");
        length += (size_t)added;
    }
    if (length + 2 > sizeof rows)
        validation_fail("unit-distance binding terminator exceeded budget");
    memcpy(rows + length, "]", 2);
    qc_context *context = qc_create(0, 7);
    char *bound = context ? qc_bind_document(context, ir, rows) : NULL;
    if (!bound)
        validation_fail("unit-distance binding failed: %s", context ? qc_error(context) : "allocation");
    qc_destroy(context);
    cj_free(&document);
    return bound;
}

'''
source=source.replace(marker,helper+marker)
source=source.replace('pending CPU-scalar coherent conditions remain rejections.', 'CPU-scalar coherent conditions are positive regressions.')
marker='''            circuits += memory_return_state_probe(ir, 4, &maximum);
            free(ir);
'''
assert source.count(marker)==1
source=source.replace(marker,marker+'''            char *scalar = compile_case(&v, &coherent_scalar, level, builtin, 0);
            char *bound = bind_unit_distance(scalar);
            circuits += memory_return_state_probe(bound, 4, &maximum);
            free(bound);
            free(scalar);
''')
path.write_text(source)
path=candidate/'scripts/archive-audit.py'
source=path.read_text()
assert source.count("r'^150 pointer difference checks passed;")==1
source=source.replace("r'^150 pointer difference checks passed;", "r'^170 pointer difference checks passed;")
assert source.count("'pointer_difference_checks': 150")==1
path.write_text(source.replace("'pointer_difference_checks': 150", "'pointer_difference_checks': 170"))
sys.path.insert(0,str(root/'.deps/format-venv/lib/python3.10/site-packages'))
spec=importlib.util.spec_from_file_location('coherent_maintained_format',candidate/'scripts/format.py')
formatter=importlib.util.module_from_spec(spec)
spec.loader.exec_module(formatter)
formatter.CLANG=root/'.deps/format-venv/bin/clang-format'
for name in ('tests/validate_pointer_differences.c','scripts/archive-audit.py'):
    path=candidate/name
    formatted=formatter.format_text(path,path.read_text())
    assert formatter.format_text(path,formatted)==formatted
    path.write_text(formatted)
spec=importlib.util.spec_from_file_location('coherent_maintained_sources',candidate/'scripts/archive-audit.py')
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
snapshot=module.source_hashes()
assert len(snapshot)==137
(candidate/'build/maintained-sources.json').write_text(json.dumps(snapshot,indent=2)+'\n')
print('Prepared 170-check maintained suite: 90 native,40 diagnostics,40 action/inverse circuits;45 C references separate. Source count remains137.')
