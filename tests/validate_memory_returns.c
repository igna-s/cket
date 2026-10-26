#define _POSIX_C_SOURCE 200809L
#include "internal.h"
#include "validation.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "memory_return_state.inc"

typedef struct {
    const char *name, *program, *diagnostic;
} return_case;

static const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
static unsigned checks;

static const return_case native_cases[] = {
    {"array-order",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "\n"
     "int main(void) {\n"
     "    quant uint8_t a[2] = {5, 7};\n"
     "    uint8_t *p = memset(a + 1, 17, 1);\n"
     "    int bad = !(p > a && p <= a + 2 && p != a);\n"
     "    measure(a[1]);\n"
     "    measure(a[0]);\n"
     "    return bad;\n"
     "}\n",
     NULL},
    {"byte-view",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "\n"
     "int main(void) {\n"
     "    quant uint16_t x = 0x1234;\n"
     "    void *p = memset((char *)&x + 1, 0xab, 1);\n"
     "    unsigned got = measure(x);\n"
     "    return got != 0xab34 || p != (char *)&x + 1;\n"
     "}\n",
     NULL},
    {"classical-return",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "\n"
     "int main(void) {\n"
     "    quant bool b = 0;\n"
     "    unsigned char x = 5;\n"
     "    void *p = memset(&x, 17, 1);\n"
     "    measure(b);\n"
     "    return x != 17 || p != &x;\n"
     "}\n",
     NULL},
    {"coherent-pointer",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "\n"
     "int main(void) {\n"
     "    quant bool b = superposition(0, 1);\n"
     "    quant uint8_t x = 5;\n"
     "    if (b) {\n"
     "        void *p = memset(&x, 17, 1);\n"
     "        if (p == &x)\n"
     "            phase(.17);\n"
     "    }\n"
     "    unsigned got = measure(x), bit = measure(b);\n"
     "    return got != (bit ? 17 : 5);\n"
     "}\n",
     NULL},
    {"helper-alias",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "\n"
     "void fill(quant uint8_t *d, quant const bool b) {\n"
     "    void *p = memset(d, b, 1);\n"
     "    if (p == d)\n"
     "        phase(.17);\n"
     "}\n"
     "\n"
     "int main(void) {\n"
     "    quant bool b = superposition(0, 1);\n"
     "    quant uint8_t x = 5;\n"
     "    void *p = &x;\n"
     "    fill((uint8_t *)p, b);\n"
     "    unsigned got = measure(*(uint8_t *)p), bit = measure(b);\n"
     "    return got != bit;\n"
     "}\n",
     NULL},
    {"memcpy-typed",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "\n"
     "int main(void) {\n"
     "    quant uint8_t x = 5, y = 7;\n"
     "    uint8_t *p = memcpy(&x, &y, 1);\n"
     "    unsigned got = measure(*p);\n"
     "    measure(y);\n"
     "    return got != 7 || p != &x;\n"
     "}\n",
     NULL},
    {"memmove-offset",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "\n"
     "int main(void) {\n"
     "    quant uint8_t a[2] = {5, 7};\n"
     "    void *p = memmove(a + 1, a, 1);\n"
     "    quant bool r = memcmp(p, \"\\x05\", 1) == 0;\n"
     "    unsigned got = measure(r);\n"
     "    measure(a[1]);\n"
     "    measure(a[0]);\n"
     "    return !got || p != a + 1;\n"
     "}\n",
     NULL},
    {"memset-typed",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "\n"
     "int main(void) {\n"
     "    quant uint8_t x = 5;\n"
     "    uint8_t *p = memset(&x, 17, 1);\n"
     "    unsigned y = measure(*p);\n"
     "    return y != 17 || p != &x;\n"
     "}\n",
     NULL},
    {"memset-void",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "\n"
     "int main(void) {\n"
     "    quant uint8_t x = 5;\n"
     "    void *p = memset(&x, 17, 1);\n"
     "    quant bool r = memcmp(p, \"\\x11\", 1) == 0;\n"
     "    unsigned y = measure(r);\n"
     "    measure(x);\n"
     "    return !y || p != &x;\n"
     "}\n",
     NULL},
    {"null-predicate",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "\n"
     "int main(void) {\n"
     "    quant uint8_t x = 5;\n"
     "    void *p = memset(&x, 17, 1);\n"
     "    if (!p)\n"
     "        return 1;\n"
     "    unsigned got = measure(x);\n"
     "    return got != 17;\n"
     "}\n",
     NULL},
    {"reborrow",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "\n"
     "int main(void) {\n"
     "    quant uint8_t x = 5;\n"
     "    void *p = memset(&x, 17, 1);\n"
     "    uint8_t *q = p;\n"
     "    unsigned got = measure(*q);\n"
     "    return got != 17 || q != &x;\n"
     "}\n",
     NULL},
    {"static-loop",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "\n"
     "int main(void) {\n"
     "    quant bool b = superposition(0, 1);\n"
     "    quant uint8_t x = 5;\n"
     "    for (int i = 0; i < 2; ++i) {\n"
     "        void *p = memset(&x, b, 1);\n"
     "        if (p == &x)\n"
     "            phase(.11);\n"
     "    }\n"
     "    unsigned got = measure(x), bit = measure(b);\n"
     "    return got != bit;\n"
     "}\n",
     NULL},
    {"zero-one-past",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "\n"
     "int main(void) {\n"
     "    quant uint8_t a[1] = {5};\n"
     "    quant bool b = 0;\n"
     "    void *p = memset(a + 1, b--, 0);\n"
     "    unsigned got = measure(b);\n"
     "    measure(a[0]);\n"
     "    return !got || p != a + 1;\n"
     "}\n",
     NULL},
    {"cpu-value",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "#include <stdio.h>\n"
     "\n"
     "int main(void) {\n"
     "    quant uint8_t x = 5;\n"
     "    void *p = memset(&x, 17, 1);\n"
     "    unsigned got = *(uint8_t *)p;\n"
     "    measure(x);\n"
     "    return got != 17;\n"
     "}\n",
     "measure explicitly"},
    {"cpu-escape",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "#include <stdio.h>\n"
     "\n"
     "int main(void) {\n"
     "    quant uint8_t x = 5;\n"
     "    void *p = memset(&x, 17, 1);\n"
     "    printf(\"%p\", p);\n"
     "    measure(x);\n"
     "    return 0;\n"
     "}\n",
     "escape"},
    {"readonly",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "#include <stdio.h>\n"
     "\n"
     "int main(void) {\n"
     "    quant uint8_t x = 5;\n"
     "    const uint8_t *p = memset(&x, 17, 1);\n"
     "    memset((void *)p, 7, 1);\n"
     "    measure(x);\n"
     "    return 0;\n"
     "}\n",
     "mutable"},
    {"incompatible",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "#include <stdio.h>\n"
     "\n"
     "int main(void) {\n"
     "    quant uint8_t x = 5;\n"
     "    uint16_t *p = memset(&x, 17, 1);\n"
     "    measure(x);\n"
     "    return p == 0;\n"
     "}\n",
     "compatible checked borrow"},
    {"rebind",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "#include <stdio.h>\n"
     "\n"
     "int main(void) {\n"
     "    quant uint8_t x = 5, y = 7;\n"
     "    void *p = memset(&x, 17, 1);\n"
     "    p = memset(&y, 3, 1);\n"
     "    measure(x);\n"
     "    measure(y);\n"
     "    return p == 0;\n"
     "}\n",
     "escape"},
    {"coherent-cpu",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "#include <stdio.h>\n"
     "\n"
     "int main(void) {\n"
     "    quant bool b = superposition(0, 1);\n"
     "    quant uint8_t x = 5;\n"
     "    int count = 0;\n"
     "    if (b) {\n"
     "        void *p = memset(&x, 17, 1);\n"
     "        if (p)\n"
     "            ++count;\n"
     "    }\n"
     "    measure(x);\n"
     "    measure(b);\n"
     "    return count;\n"
     "}\n",
     "inside coherent quantum control"}};

static const return_case state_cases[] = {{"word-known",
                                           "#include <quantum.h>\n"
                                           "#include <string.h>\n"
                                           "\n"
                                           "int main(void) {\n"
                                           "    quant uint16_t x = 0x1234;\n"
                                           "    void *p = memset((char *)&x + 1, 0xab, 1);\n"
                                           "    memset(p, 0xcd, 1);\n"
                                           "    unsigned got = measure(x);\n"
                                           "    return got != 0xcd34 || p != (char *)&x + 1;\n"
                                           "}\n",
                                           NULL},
                                          {"zero-known",
                                           "#include <quantum.h>\n"
                                           "#include <string.h>\n"
                                           "\n"
                                           "int main(void) {\n"
                                           "    quant uint8_t a[2] = {5, 7};\n"
                                           "    quant bool b = 0;\n"
                                           "    void *p = memset(a + 2, b--, 0);\n"
                                           "    unsigned got = measure(b);\n"
                                           "    measure(a[1]);\n"
                                           "    measure(a[0]);\n"
                                           "    return !got || p != a + 2;\n"
                                           "}\n",
                                           NULL},
                                          {"word-coherent",
                                           "#include <quantum.h>\n"
                                           "#include <string.h>\n"
                                           "\n"
                                           "int main(void) {\n"
                                           "    quant uint16_t x = superposition(0, 65535);\n"
                                           "    if (x & 1)\n"
                                           "        phase(1.0000000000000001e-05);\n"
                                           "    if (x & 2)\n"
                                           "        phase(2.0000000000000002e-05);\n"
                                           "    if (x & 4)\n"
                                           "        phase(4.0000000000000003e-05);\n"
                                           "    if (x & 8)\n"
                                           "        phase(8.0000000000000007e-05);\n"
                                           "    if (x & 16)\n"
                                           "        phase(0.00016000000000000001);\n"
                                           "    if (x & 32)\n"
                                           "        phase(0.00032000000000000003);\n"
                                           "    if (x & 64)\n"
                                           "        phase(0.00064000000000000005);\n"
                                           "    if (x & 128)\n"
                                           "        phase(0.0012800000000000001);\n"
                                           "    if (x & 256)\n"
                                           "        phase(0.0025600000000000002);\n"
                                           "    if (x & 512)\n"
                                           "        phase(0.0051200000000000004);\n"
                                           "    if (x & 1024)\n"
                                           "        phase(0.010240000000000001);\n"
                                           "    if (x & 2048)\n"
                                           "        phase(0.020480000000000002);\n"
                                           "    if (x & 4096)\n"
                                           "        phase(0.040960000000000003);\n"
                                           "    if (x & 8192)\n"
                                           "        phase(0.081920000000000007);\n"
                                           "    if (x & 16384)\n"
                                           "        phase(0.16384000000000001);\n"
                                           "    if (x & 32768)\n"
                                           "        phase(0.32768000000000003);\n"
                                           "    void *p = memset((char *)&x + 1, 0xab, 1);\n"
                                           "    memset(p, 0xcd, 1);\n"
                                           "    if (x == 0xcd34)\n"
                                           "        phase(.17);\n"
                                           "    measure(x);\n"
                                           "    return p != (char *)&x + 1;\n"
                                           "}\n",
                                           NULL},
                                          {"zero-coherent",
                                           "#include <quantum.h>\n"
                                           "#include <string.h>\n"
                                           "\n"
                                           "int main(void) {\n"
                                           "    quant uint8_t a[2] = {5, 7};\n"
                                           "    quant bool b = superposition(0, 1);\n"
                                           "    if (b)\n"
                                           "        phase(.31);\n"
                                           "    void *p = memset(a + 2, b--, 0);\n"
                                           "    if (b)\n"
                                           "        phase(.17);\n"
                                           "    measure(b);\n"
                                           "    measure(a[1]);\n"
                                           "    measure(a[0]);\n"
                                           "    return p != a + 2;\n"
                                           "}\n",
                                           NULL},
                                          {"pointer-control",
                                           "#include <quantum.h>\n"
                                           "#include <string.h>\n"
                                           "\n"
                                           "int main(void) {\n"
                                           "    quant bool b = superposition(0, 1);\n"
                                           "    quant uint8_t x = 5;\n"
                                           "    if (b)\n"
                                           "        phase(.31);\n"
                                           "    if (b) {\n"
                                           "        void *p = memset(&x, 17, 1);\n"
                                           "        if (p == &x)\n"
                                           "            phase(.17);\n"
                                           "    }\n"
                                           "    measure(x);\n"
                                           "    measure(b);\n"
                                           "    return 0;\n"
                                           "}\n",
                                           NULL}};

static char *compile_case(
    validation *v, const return_case *test, unsigned level, unsigned builtin, int execute) {
    char source[4096], output[4096], compiler[4096];
    validation_path(source, sizeof source, "%s/return.c", v->temp);
    validation_path(output, sizeof output, "%s/return.out", v->temp);
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
        validation_fail("memory return %s %s builtins=%u failed:\n%s\n%s",
                        test->name,
                        levels[level],
                        builtin,
                        test->program,
                        log);
    free(log);
    return !execute && !test->diagnostic ? validation_read(output) : NULL;
}

static char *reference_program(const return_case *test) {
    static const char preamble[] = "#include <stdint.h>\n#include <stdbool.h>\n#define quant\n"
                                   "#define measure(x) (x)\n#define phase(x) ((void)(x))\n"
                                   "#define superposition(a,b) (RETURN_INPUT ? (b) : (a))\n";
    static const char quantum_header[] = "#include <quantum.h>\n";
    const char *header = strstr(test->program, quantum_header);
    if (!header)
        validation_fail("missing memory return fixture preamble");
    size_t prefix = (size_t)(header - test->program);
    size_t size = sizeof preamble + strlen(test->program);
    char *program = malloc(size);
    if (!program)
        validation_fail("memory return fixture allocation failed");
    memcpy(program, preamble, sizeof preamble - 1);
    memcpy(program + sizeof preamble - 1, test->program, prefix);
    strcpy(program + sizeof preamble - 1 + prefix, header + sizeof quantum_header - 1);
    return program;
}

static void reference(validation *v, const return_case *test, int execute) {
    char source[4096], binary[4096];
    validation_path(source, sizeof source, "%s/reference.c", v->temp);
    validation_path(binary, sizeof binary, "%s/reference.out", v->temp);
    char *program = reference_program(test);
    validation_write(source, program);
    free(program);
    const char *cc = getenv("CC");
    for (unsigned input = 0; input < (execute ? 2u : 1u); ++input) {
        char *command[] = {(char *)(cc ? cc : "gcc"),
                           "-std=c11",
                           "-O2",
                           "-Werror=implicit-function-declaration",
                           input ? "-DRETURN_INPUT=1" : "-DRETURN_INPUT=0",
                           execute ? "-o" : "-fsyntax-only",
                           execute ? binary : source,
                           execute ? source : NULL,
                           NULL};
        if (validation_run(v, command, NULL, 60)) {
            char *log = validation_read(v->log);
            validation_fail("ordinary C memory return %s compilation failed:\n%s", test->name, log);
        }
        if (execute) {
            char *run[] = {binary, NULL};
            if (validation_run(v, run, NULL, 60)) {
                char *log = validation_read(v->log);
                validation_fail(
                    "ordinary C memory return %s input=%u failed:\n%s", test->name, input, log);
            }
        }
    }
}

static void native(validation *v) {
    unsigned positive = 0, negative = 0, references = 0, syntax = 0;
    for (unsigned i = 0; i < sizeof native_cases / sizeof *native_cases; ++i) {
        const return_case *test = &native_cases[i];
        reference(v, test, !test->diagnostic);
        if (test->diagnostic)
            ++syntax;
        else
            references += 2;
        for (unsigned level = 0; level < 5; ++level)
            for (unsigned builtin = 0; builtin < 2; ++builtin) {
                free(compile_case(v, test, level, builtin, !test->diagnostic));
                if (test->diagnostic)
                    ++negative;
                else
                    ++positive;
            }
    }
    checks += positive + negative;
    printf(
        "PASS %u native memory returns: typed/void borrows, byte offsets, pointer relations, helper/loop scopes and classical libc; five GCC levels, both builtin modes; %u ordinary C reference executions\n",
        positive,
        references);
    printf(
        "PASS %u memory return rejection contracts: CPU reads/escapes, const/type violations, rebinding and coherent CPU effects; %u ordinary C syntax checks\n",
        negative,
        syntax);
    fflush(stdout);
}

static void states(validation *v) {
    unsigned count = 0, words = 0, saved = 0, maximum = 0;
    for (unsigned profile = 0; profile < sizeof state_cases / sizeof *state_cases; ++profile)
        for (unsigned level = 0; level < 5; ++level)
            for (unsigned builtin = 0; builtin < 2; ++builtin) {
                char *ir = compile_case(v, &state_cases[profile], level, builtin, 0);
                unsigned circuits = memory_return_state_probe(ir, profile, &maximum);
                count += circuits;
                if (profile == 0 || profile == 2)
                    words += circuits;
                else
                    saved += circuits;
                free(ir);
            }
    checks += count;
    printf(
        "PASS %u complete memory return basis-action circuits: %u word C channels/phases and independent complete gate inverses, %u Boolean paths with saved write/definition inverses; known/coherent byte views, zero-count effects and pointer control; verified H preparations, both synthesis policies, maximum %u physical wires\n",
        count,
        words,
        saved,
        maximum);
    fflush(stdout);
}

int main(int argc, char **argv) {
    if (argc != 1)
        validation_fail("usage: validate_memory_returns");
    validation v;
    validation_init(&v, argv[0]);
    states(&v);
    native(&v);
    validation_cleanup(&v);
    printf(
        "%u memory return checks passed; complete labeled action, native integration and expected rejections have distinct proof scopes.\n",
        checks);
    return 0;
}
