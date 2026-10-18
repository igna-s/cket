#define _POSIX_C_SOURCE 200809L
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
    {"byte-array",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "#include <stddef.h>\n"
     "int main(void){\n"
     "quant uint8_t a[2]={3,5};uint8_t *p=memcpy(a+1,a,1);\n"
     "ptrdiff_t n=p-a,m=a-p;unsigned x=measure(a[0]),y=measure(a[1]);return n!=1||m!=-1||x!=3||y!=3;\n"
     "}\n",
     NULL},
    {"one-past-array",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "#include <stddef.h>\n"
     "int main(void){\n"
     "quant uint8_t a[2]={3,5};uint8_t *p=memset(a+2,0,0);\n"
     "ptrdiff_t n=p-a,m=a-p;unsigned x=measure(a[0]),y=measure(a[1]);return n!=2||m!=-2||x!=3||y!=5;\n"
     "}\n",
     NULL},
    {"word-element-units",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "#include <stddef.h>\n"
     "int main(void){\n"
     "quant uint16_t x=0x1234;uint16_t *p=memmove(&x,&x,0);\n"
     "ptrdiff_t n=(p+1)-p,m=p-(p+1);unsigned y=measure(x);return n!=1||m!=-1||y!=0x1234;\n"
     "}\n",
     NULL},
    {"word-byte-units",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "#include <stddef.h>\n"
     "int main(void){\n"
     "quant uint16_t x=0x1234;uint16_t *p=memmove(&x,&x,0);\n"
     "ptrdiff_t n=(unsigned char*)(p+1)-(unsigned char*)p;unsigned y=measure(x);return n!=2||y!=0x1234;\n"
     "}\n",
     NULL},
    {"byte-view-offset",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "#include <stddef.h>\n"
     "int main(void){\n"
     "quant uint16_t x=0x1234;void *p=memset((char*)&x+1,0xab,1);\n"
     "ptrdiff_t n=(char*)p-(char*)&x,m=(char*)&x-(char*)p;unsigned y=measure(x);return n!=1||m!=-1||y!=0xab34;\n"
     "}\n",
     NULL},
    {"signed-word",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "#include <stddef.h>\n"
     "int main(void){\n"
     "quant int16_t x=-1;int16_t *p=memmove(&x,&x,0);\n"
     "ptrdiff_t n=(p+1)-p;int y=measure(x);return n!=1||y!=-1;\n"
     "}\n",
     NULL},
    {"const-borrow",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "#include <stddef.h>\n"
     "int main(void){\n"
     "quant uint8_t a[2]={3,5};const uint8_t *p=memset(a,0,0);\n"
     "ptrdiff_t n=(a+2)-p;unsigned x=measure(a[0]),y=measure(a[1]);return n!=2||x!=3||y!=5;\n"
     "}\n",
     NULL},
    {"classical-array",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "#include <stddef.h>\n"
     "int main(void){\n"
     "uint16_t a[3]={1,2,3};uint16_t *p=a+2;ptrdiff_t n=p-a,m=a-p;\n"
     "quant bool b=1;int bit=measure(b);return n!=2||m!=-2||!bit;\n"
     "}\n",
     NULL},
    {"different-objects",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "#include <stddef.h>\n"
     "int main(void){\n"
     "quant uint8_t a=3,b=4;uint8_t *p=memset(&a,0,0);ptrdiff_t n=p-&b;measure(a);measure(b);return n!=0;\n"
     "}\n",
     "|c>: quantum pointer subtraction"},
    {"cpu-object",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "#include <stddef.h>\n"
     "int main(void){\n"
     "quant uint8_t a=3;uint8_t b=4;uint8_t *p=memset(&a,0,0);ptrdiff_t n=p-&b;measure(a);return n!=0;\n"
     "}\n",
     "|c>: quantum pointer subtraction"},
    {"mutating-address",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "#include <stddef.h>\n"
     "int main(void){\n"
     "quant uint8_t a[2]={3,4};uint8_t *p=memset(a,0,0);ptrdiff_t n=p++-a;measure(a[0]);measure(a[1]);return n!=0;\n"
     "}\n",
     "|c>: quantum pointer subtraction"},
    {"null-address",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "#include <stddef.h>\n"
     "int main(void){\n"
     "quant uint8_t a=3;uint8_t *p=memset(&a,0,0);ptrdiff_t n=p-(uint8_t*)0;measure(a);return n!=0;\n"
     "}\n",
     "|c>: quantum reference cannot escape"},
    {"cpu-scalar-control",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "#include <stddef.h>\n"
     "int main(int argc,char**argv){(void)argc;(void)argv;\n"
     "quant bool b=1;quant uint8_t x=5;void*p=memset(&x,0,0);ptrdiff_t n=(uint8_t*)p+1-(uint8_t*)p;if(b){if(n==1){memset(p,17,1);}}unsigned a=measure(x),v=measure(b);return a!=(v?17:5)||n!=1;\n"
     "}\n",
     NULL}};

static const difference_case coherent = {
    "coherent-distance",
    "#include <quantum.h>\n"
    "#include <string.h>\n"
    "#include <stddef.h>\n"
    "int main(void) {\n"
    "    quant bool b = superposition(0, 1);\n"
    "    quant uint8_t x = 5;\n"
    "    void *p = memset(&x, 0, 0);\n"
    "    ptrdiff_t n = (uint8_t *)p + 1 - (uint8_t *)p;\n"
    "    if (b) phase(.31);\n"
    "    if (b) { if ((uint8_t *)p + 1 - (uint8_t *)p == 1) { memset(p, 17, 1); phase(.17); } }\n"
    "    measure(x);\n"
    "    measure(b);\n"
    "    return n != 1;\n"
    "}\n",
    NULL};

static const difference_case coherent_scalar = {
    "coherent-scalar-distance",
    "#include <quantum.h>\n"
    "#include <string.h>\n"
    "#include <stddef.h>\n"
    "int main(int argc,char**argv){(void)argc;(void)argv;\n"
    "quant bool b=superposition(0,1);quant uint8_t x=5;void*p=memset(&x,0,0);ptrdiff_t n=(uint8_t*)p+1-(uint8_t*)p;if(b)phase(.31);if(b){if(n==1){memset(p,17,1);phase(.17);}}unsigned a=measure(x),v=measure(b);return a!=(v?17:5)||n!=1;\n"
    "}\n",
    NULL};

static char *compile_case(
    validation *v, const difference_case *test, unsigned level, unsigned builtin, int execute) {
    char source[4096], output[4096], compiler[4096];
    validation_path(source, sizeof source, "%s/difference.c", v->temp);
    validation_path(output, sizeof output, "%s/difference.out", v->temp);
    validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
    validation_write(source, test->program);
    char *command[] = {compiler,
                       source,
                       execute ? "--run" : "--emit-ir",
                       (char *)levels[level],
                       builtin ? "-fbuiltin" : "-fno-builtin",
                       "-o",
                       output,
                       NULL};
    int status = validation_run(v, command, NULL, 240);
    char *log = validation_read(v->log);
    if (test->diagnostic ? !validation_rejected(status, log, test->diagnostic) : status != 0)
        validation_fail("pointer difference %s %s builtins=%u failed:\n%s\n%s",
                        test->name,
                        levels[level],
                        builtin,
                        test->program,
                        log);
    free(log);
    return !execute && !test->diagnostic ? validation_read(output) : NULL;
}

static unsigned reference(validation *v, const difference_case *test) {
    static const char header[] = "#include <quantum.h>\n";
    static const char preamble[] = "#include <stdint.h>\n#include <stdbool.h>\n"
                                   "#define quant\n#define measure(x) (x)\n";
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
        char *command[] = {(char *)(cc ? cc : "gcc"),
                           "-std=c11",
                           (char *)levels[level],
                           "-Werror=implicit-function-declaration",
                           source,
                           "-o",
                           binary,
                           NULL};
        char *run[] = {binary, NULL};
        if (validation_run(v, command, NULL, 60) || validation_run(v, run, NULL, 60)) {
            char *log = validation_read(v->log);
            validation_fail(
                "ordinary C pointer distance %s %s failed:\n%s", test->name, levels[level], log);
        }
    }
    return 5;
}

/* This fixture has exactly one classical input: its source-level distance
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
        int added = snprintf(rows + length,
                             sizeof rows - length,
                             "%s{\"instruction\":%u,\"values\":[1]}",
                             bindings++ ? "," : "",
                             index);
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
        validation_fail("unit-distance binding failed: %s",
                        context ? qc_error(context) : "allocation");
    qc_destroy(context);
    cj_free(&document);
    return bound;
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
                if (test->diagnostic)
                    ++rejected;
                else
                    ++native;
            }
    }
    printf(
        "PASS %u native pointer distances and %u rejection contracts; %u ordinary C executions separate; CPU-scalar coherent conditions are positive regressions.\n",
        native,
        rejected,
        references);
    fflush(stdout);
    for (unsigned level = 0; level < 5; ++level)
        for (unsigned builtin = 0; builtin < 2; ++builtin) {
            char *ir = compile_case(&v, &coherent, level, builtin, 0);
            circuits += memory_return_state_probe(ir, 4, &maximum);
            free(ir);
            char *scalar = compile_case(&v, &coherent_scalar, level, builtin, 0);
            char *bound = bind_unit_distance(scalar);
            circuits += memory_return_state_probe(bound, 4, &maximum);
            free(bound);
            free(scalar);
        }
    printf(
        "PASS %u pointer-distance action/inverse circuits: both Boolean labels, .48-radian relative phase, saved write/definition inverses and private zeros; both synthesis policies, maximum %u physical wires.\n",
        circuits,
        maximum);
    validation_cleanup(&v);
    printf(
        "%u pointer difference checks passed; native distances, finite coherent action and unsupported contexts have separate scopes.\n",
        native + rejected + circuits);
    return 0;
}
