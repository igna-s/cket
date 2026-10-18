#define _POSIX_C_SOURCE 200809L
#include "validation.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name, *operation, *expected;
} operand_case;

static const operand_case cases[] = {
    {"xor", "x ^= 10 / k;", "5 ^ (10 / k)"},
    {"add", "x += 10 / k;", "5 + 10 / k"},
    {"assignment", "x = 10 / k;", "10 / k"},
    {"mixed-add", "x = x + 10 / k;", "5 + 10 / k"},
    {"memset", "memset((void *)&x, 10 / k, 1);", "10 / k"},
    {"abs", "x = abs(10 / k);", "abs(10 / k)"},
    {"labs", "x = labs(10L / k);", "labs(10L / k)"},
    {"llabs", "x = llabs(10LL / k);", "llabs(10LL / k)"},
    {"imaxabs", "x = imaxabs((intmax_t)10 / k);", "imaxabs((intmax_t)10 / k)"},
    {"zero-length-memset", "memset((void *)&x, 10 / k, 0);", "5"},
};
static const operand_case effects[] = {
    {"xor-increment", "x ^= 10 / k++;", NULL},
    {"add-increment", "x += 10 / k++;", NULL},
    {"abs-increment", "x = abs(10 / k++);", NULL},
    {"fill-increment", "memset((void *)&x, 10 / k++, 1);", NULL},
    {"zero-fill-increment", "memset((void *)&x, 10 / k++, 0);", NULL},
    {"volatile-read", "x = 10 / k;", NULL},
};
static const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
static const char *basis_inputs[][2] = {
    {"0", "0"},
    {"2", "0"},
    {"5", "0"},
    {"-3", "0"},
    {"2", "1"},
    {"5", "1"},
    {"-3", "1"},
    {"0", "1"},
};
static const char *coherent_inputs[][2] = {
    {"2", "1"},
    {"5", "1"},
    {"-3", "1"},
    {"0", "1"},
};
static const char quantum_header[] = "#include <quantum.h>\n";
static const char ordinary_header[] = "#include <stdint.h>\n#include <stdbool.h>\n#define quant\n"
                                      "#define measure(x) (x)\n#define phase(x) ((void)(x))\n"
                                      "#define superposition(a,b) (b)\n";

static void program(char *out, size_t size, const operand_case *test, bool coherent, bool effect) {
    int length =
        snprintf(out,
                 size,
                 "%s#include <stdlib.h>\n#include <string.h>\n#include <inttypes.h>\n"
                 "int main(int argc, char **argv) {\n"
                 "    %sint k = argc > 1 ? atoi(argv[1]) : 0;\n"
                 "    int enabled = argc > 2 ? argv[2][0] - '0' : 0;\n"
                 "    (void)enabled;\n"
                 "    quant bool b = %s;\n"
                 "    quant uint8_t x = 5;\n"
                 "    if (b) phase(.31);\n"
                 "    if (b) { %s phase(.17); }\n"
                 "    unsigned value = measure(x), bit = measure(b);\n"
                 "    return %s;\n"
                 "}\n",
                 quantum_header,
                 effect && !strcmp(test->name, "volatile-read") ? "volatile " : "",
                 coherent ? "superposition(0, 1)" : "(bool)enabled",
                 test->operation,
                 effect ? "0" : "value != (bit ? (unsigned)(uint8_t)(EXPECTED) : 5) || CONTROL");
    if (length < 0 || (size_t)length >= size)
        validation_fail("controlled operand fixture exceeds its checked buffer");
    if (!effect) {
        char *expected = strstr(out, "EXPECTED"), *control = strstr(out, "CONTROL");
        if (!expected || !control)
            validation_fail("controlled operand fixture placeholders missing");
        char completed[8192];
        length = snprintf(completed,
                          sizeof completed,
                          "%.*s%s%.*s%s%s",
                          (int)(expected - out),
                          out,
                          test->expected,
                          (int)(control - expected - 8),
                          expected + 8,
                          coherent ? "0" : "bit != (unsigned)(enabled != 0)",
                          control + 7);
        if (length < 0 || (size_t)length >= sizeof completed || (size_t)length >= size)
            validation_fail("controlled operand substitution exceeds its checked buffer");
        memcpy(out, completed, (size_t)length + 1);
    }
}

static void compile_fixture(validation *v,
                            const char *source,
                            const char *binary,
                            unsigned level,
                            unsigned builtin,
                            bool reject) {
    char compiler[4096];
    validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
    char *command[] = {compiler,
                       (char *)source,
                       (char *)levels[level],
                       builtin ? "-fbuiltin" : "-fno-builtin",
                       reject ? "--emit-ir" : "--emit-quantum-ir",
                       "-o",
                       (char *)binary,
                       NULL};
    /* A normal executable requires the driver's default action. */
    if (!reject) {
        command[4] = "-o";
        command[5] = (char *)binary;
        command[6] = NULL;
    }
    int status = validation_run(v, command, NULL, 240);
    char *log = validation_read(v->log);
    bool accepted = reject ? validation_rejected(status, log, "CPU effects") ||
                                 validation_rejected(status, log, "CPU effect") ||
                                 validation_rejected(status, log, "classical side effect")
                           : status == 0;
    if (!accepted)
        validation_fail("controlled operand compiler contract %s builtins=%u failed:\n%s",
                        levels[level],
                        builtin,
                        log);
    free(log);
}

static unsigned run_inputs(validation *v, const char *binary, bool coherent, bool domains) {
    const char *(*inputs)[2] = coherent ? coherent_inputs : basis_inputs;
    unsigned count = coherent ? 4 : 8, completed = 0;
    for (unsigned i = 0; i < count - (domains ? 0u : 1u); ++i) {
        char *command[] = {(char *)binary, (char *)inputs[i][0], (char *)inputs[i][1], NULL};
        int status = validation_run(v, command, NULL, 60);
        char *log = validation_read(v->log);
        bool undefined = domains && i == count - 1;
        if (undefined
                ? !validation_rejected(status, log, "C division/remainder by zero is undefined")
                : status != 0)
            validation_fail("controlled operand %s / %s failed: status=%d\n%s",
                            inputs[i][0],
                            inputs[i][1],
                            status,
                            log);
        free(log);
        ++completed;
    }
    return completed;
}

static unsigned reference(validation *v, const char *fixture, bool coherent) {
    if (strncmp(fixture, quantum_header, sizeof quantum_header - 1))
        validation_fail("unexpected controlled operand reference preamble");
    char text[16384], source[4096], binary[4096];
    int length =
        snprintf(text, sizeof text, "%s%s", ordinary_header, fixture + sizeof quantum_header - 1);
    if (length < 0 || (size_t)length >= sizeof text)
        validation_fail("ordinary controlled operand reference exceeds buffer");
    validation_path(source, sizeof source, "%s/reference.c", v->temp);
    validation_path(binary, sizeof binary, "%s/reference.out", v->temp);
    validation_write(source, text);
    const char *cc = getenv("CC");
    char *command[] = {(char *)(cc ? cc : "gcc"), "-std=c11", "-O2", source, "-o", binary, NULL};
    if (validation_run(v, command, NULL, 60)) {
        char *log = validation_read(v->log);
        validation_fail("ordinary controlled operand compilation failed:\n%s", log);
    }
    return run_inputs(v, binary, coherent, false);
}

int main(int argc, char **argv) {
    if (argc != 1)
        validation_fail("usage: validate_controlled_operands");
    validation v;
    validation_init(&v, argv[0]);
    char source[4096], binary[4096], fixture[8192];
    validation_path(source, sizeof source, "%s/operand.c", v.temp);
    validation_path(binary, sizeof binary, "%s/operand.out", v.temp);
    unsigned native = 0, domains = 0, rejected = 0, homonyms = 0, references = 0;
    for (unsigned i = 0; i < sizeof cases / sizeof *cases; ++i) {
        for (unsigned coherent = 0; coherent < 2; ++coherent) {
            program(fixture, sizeof fixture, &cases[i], coherent, false);
            references += reference(&v, fixture, coherent);
            validation_write(source, fixture);
            for (unsigned level = 0; level < 5; ++level)
                for (unsigned builtin = 0; builtin < 2; ++builtin) {
                    compile_fixture(&v, source, binary, level, builtin, false);
                    native += run_inputs(&v, binary, coherent, true) - 1;
                    ++domains;
                }
        }
    }
    for (unsigned i = 0; i < sizeof effects / sizeof *effects; ++i) {
        program(fixture, sizeof fixture, &effects[i], true, true);
        validation_write(source, fixture);
        for (unsigned level = 0; level < 5; ++level)
            for (unsigned builtin = 0; builtin < 2; ++builtin) {
                compile_fixture(&v, source, binary, level, builtin, true);
                ++rejected;
            }
    }
    validation_write(source,
                     "#include <quantum.h>\nstatic int abs(int x) { return x + 1; }\n"
                     "int main(void) { int n = abs(-2); quant bool b = 1; quant uint8_t x = 5;\n"
                     "if (b) x ^= n; unsigned r = measure(x), bit = measure(b);\n"
                     "return r != 250 || bit != 1 || n != -1; }\n");
    for (unsigned level = 0; level < 5; ++level)
        for (unsigned builtin = 0; builtin < 2; ++builtin) {
            compile_fixture(&v, source, binary, level, builtin, false);
            char *command[] = {binary, NULL};
            if (validation_run(&v, command, NULL, 60))
                validation_fail("private abs homonym changed its C behavior");
            ++homonyms;
        }
    if (native != 1000 || domains != 200 || rejected != 60 || homonyms != 10 || references != 100)
        validation_fail("controlled operand coverage count mismatch");
    printf(
        "PASS %u defined native controlled operands, %u undefined-domain diagnostics, %u CPU-effect rejections and %u internal homonyms; %u ordinary C executions separate.\n",
        native,
        domains,
        rejected,
        homonyms,
        references);
    validation_cleanup(&v);
    printf(
        "%u controlled operand checks passed; native values, undefined-domain diagnostics, effect boundaries and independent archived states have separate scopes.\n",
        native + domains + rejected + homonyms);
    return 0;
}
