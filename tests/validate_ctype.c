#include "internal.h"
#include "validation.h"
#include <ctype.h>
#include <locale.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
static const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
static const char *names[] = {"isalnum",
                              "isalpha",
                              "isblank",
                              "iscntrl",
                              "isdigit",
                              "isgraph",
                              "islower",
                              "isprint",
                              "ispunct",
                              "isspace",
                              "isupper",
                              "isxdigit",
                              "tolower",
                              "toupper"};
static int (*volatile reference[])(int) = {isalnum,
                                           isalpha,
                                           isblank,
                                           iscntrl,
                                           isdigit,
                                           isgraph,
                                           islower,
                                           isprint,
                                           ispunct,
                                           isspace,
                                           isupper,
                                           isxdigit,
                                           tolower,
                                           toupper};
static const int witnesses[] = {'A', 'A', ' ', 0, '0', 'A', 'a', ' ', '!', ' ', 'A', 'A'};

static size_t register_basis(const qc_register *r, unsigned value) {
    size_t basis = 0;
    for (unsigned bit = 0; bit < r->width; ++bit)
        if (value & (1u << bit))
            basis |= (size_t)1 << r->wires[bit];
    return basis;
}

static void states(validation *v) {
    unsigned count = 0;
    // The mappings are checked bit by bit, keeping the full byte domain while
    // respecting the native simulator's resource limit. No basis is sampled.
    for (unsigned profile = 0; profile < 28; ++profile) {
        unsigned operation = profile < 12 ? profile : 12 + (profile - 12) / 8;
        unsigned bit = profile < 12 ? 0 : (profile - 12) % 8;
        int mask = profile < 12 ? reference[operation](witnesses[operation]) : 0;
        for (unsigned form = 0; form < 2; ++form)
            for (unsigned level = 0; level < 5; ++level) {
                char source[4096], document[4096], compiler[4096], program[4096], value[256];
                validation_path(source, sizeof source, "%s/ctype-state.c", v->temp);
                validation_path(document, sizeof document, "%s/ctype-state.json", v->temp);
                validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
                snprintf(value,
                         sizeof value,
                         profile < 12 ? "%s%s%s(x)==%d" : "(%s%s%s(x)&%d)!=0",
                         form ? "(" : "",
                         names[operation],
                         form ? ")" : "",
                         profile < 12 ? mask : 1 << bit);
                validation_path(program,
                                sizeof program,
                                "#include <quantum.h>\n#include <ctype.h>\n"
                                "int main(void){quant uint8_t x=superposition(0,255);"
                                "if(x&1)phase(.31);quant bool y=%s;if(y)phase(.17);"
                                "measure(y);measure(x);}\n",
                                value);
                validation_write(source, program);
                char *command[] = {compiler,
                                   source,
                                   "--emit-ir",
                                   (char *)levels[level],
                                   form ? "-fno-builtin" : "-fno-strict-aliasing",
                                   "-o",
                                   document,
                                   NULL};
                if (validation_run(v, command, NULL, 90))
                    validation_fail("ctype compilation failed %s bit=%u form=%u level=%s\n%s",
                                    names[operation],
                                    bit,
                                    form,
                                    levels[level],
                                    validation_read(v->log));
                char *ir = validation_read(document);
                for (unsigned tables = 0; tables < 2; ++tables) {
                    qc_context *c = qc_create(1, 7);
                    if (!c)
                        validation_fail("ctype state allocation failed");
                    qc_allow_tables(c, tables);
                    if (!qc_document(c, ir, 0, 1))
                        validation_fail(
                            "ctype state failed %s bit=%u form=%u level=%s tables=%u: %s",
                            names[operation],
                            bit,
                            form,
                            levels[level],
                            tables,
                            qc_error(c));
                    qc_register *x = qc_lookup(c, 0), *y = qc_lookup(c, 1);
                    if (!x || !y || c->measure != 9 || c->reset)
                        validation_fail(
                            "ctype changed source ownership or added channels: roots=%d/%d measurements=%llu resets=%llu",
                            !!x,
                            !!y,
                            (unsigned long long)c->measure,
                            (unsigned long long)c->reset);
                    double complex *expected = calloc(qc_state_size(c), sizeof *expected);
                    if (!expected)
                        validation_fail("ctype reference allocation failed");
                    for (unsigned byte = 0; byte < 256; ++byte) {
                        int result = reference[operation]((int)byte);
                        unsigned output = profile < 12 ? result == mask : !!(result & (1 << bit));
                        size_t basis = register_basis(x, byte) | register_basis(y, output);
                        expected[basis] =
                            cexp(I * ((byte & 1 ? .31 : 0) + (output ? .17 : 0))) / 16;
                    }
                    for (size_t basis = 0; basis < qc_state_size(c); ++basis)
                        if (cabs(qc_amplitude(c, basis) - expected[basis]) > 1e-10)
                            validation_fail(
                                "ctype full state mismatch %s bit=%u form=%u level=%s tables=%u basis=%zu",
                                names[operation],
                                bit,
                                form,
                                levels[level],
                                tables,
                                basis);
                    free(expected);
                    qc_destroy(c);
                    ++count;
                }
                free(ir);
            }
    }
    checks += count;
    printf(
        "PASS %u ctype full states: all 256 bytes, exact classification masks, all mapping bits, phases, clean private wires, two call forms, five GCC levels and both table policies\n",
        count);
}

static void programs(validation *v) {
    const char *bodies[] = {
        "quant int8_t x=-1;quant int8_t y=tolower(x);int r=measure(y);return r!=-1||measure(x)!=-1;",
        "quant int8_t x=-1;quant bool y=isalpha(x);int r=measure(y);return r||measure(x)!=-1;",
        "quant uint8_t x=65;quant uint8_t y=tolower(x++);int r=measure(y);return r!=97||measure(x)!=66;",
        "quant uint8_t x=97;quant uint8_t y=toupper(++x);int r=measure(y);return r!=66||measure(x)!=98;",
        "quant uint8_t a[2]={65,98};quant uint8_t *p=&a[0];quant uint8_t y=convert(*p);int r=measure(y);return r!=97||measure(a[0])!=65||measure(a[1])!=98;",
        "quant uint8_t x=65,y=0;for(unsigned i=0;i<3;++i)y=tolower(x);int r=measure(y);return r!=97||measure(x)!=65;",
        "quant bool flag=false;quant int8_t x=-2;quant bool y=flag?isalpha(x):true;int r=measure(y);return r!=1||measure(x)!=-2||measure(flag);",
        "quant uint8_t x=65;quant uint8_t y=tolower(x);int ok=setlocale(LC_CTYPE,\"C.UTF-8\")!=0;int r=measure(y),s=measure(x);return !ok||r!=97||s!=65;",
        "int calls=0;int r=isalpha((++calls,65));quant bool x=false;return !r||calls!=1||measure(x);",
        "quant uint8_t x=65;int r=measure((bool)isalpha(x));return r!=1||measure(x)!=65;",
    };
    unsigned count = 0;
    for (unsigned profile = 0; profile < sizeof bodies / sizeof *bodies; ++profile)
        for (unsigned level = 0; level < 5; ++level) {
            char source[4096], compiler[4096], executable[4096], program[8192];
            validation_path(source, sizeof source, "%s/ctype-program.c", v->temp);
            validation_path(executable, sizeof executable, "%s/ctype-program", v->temp);
            validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
            validation_path(program,
                            sizeof program,
                            "#include <quantum.h>\n#include <ctype.h>\n#include <locale.h>\n"
                            "static quant uint8_t convert(quant uint8_t x){return tolower(x);}\n"
                            "int main(void){%s}\n",
                            bodies[profile]);
            validation_write(source, program);
            char *command[] = {
                compiler, source, "--run", (char *)levels[level], "-o", executable, NULL};
            if (validation_run(v, command, NULL, 90))
                validation_fail("ctype combined program failed profile=%u level=%s\n%s",
                                profile,
                                levels[level],
                                validation_read(v->log));
            ++count;
        }
    checks += count;
    printf(
        "PASS %u ctype combined programs: EOF, once-only effects, memory borrows, quantum functions, static loops, lazy domains, captured inverses and native classical calls\n",
        count);
}

static void rejections(validation *v) {
    const char *bodies[] = {
        "quant int8_t x=-2;quant bool y=isalpha(x);measure(y);measure(x);",
        "quant uint16_t x=256;quant bool y=isdigit(x);measure(y);measure(x);",
        "(void)setlocale(LC_CTYPE,\"C.UTF-8\");quant uint8_t x=65;quant bool y=isalpha(x);measure(y);measure(x);",
    };
    unsigned count = 0;
    for (unsigned profile = 0; profile < sizeof bodies / sizeof *bodies; ++profile)
        for (unsigned level = 0; level < 5; ++level) {
            char source[4096], compiler[4096], executable[4096], program[8192];
            validation_path(source, sizeof source, "%s/ctype-negative.c", v->temp);
            validation_path(executable, sizeof executable, "%s/ctype-negative", v->temp);
            validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
            validation_path(program,
                            sizeof program,
                            "#include <quantum.h>\n#include <ctype.h>\n#include <locale.h>\n"
                            "int main(void){%s}\n",
                            bodies[profile]);
            validation_write(source, program);
            char *command[] = {
                compiler, source, "--run", (char *)levels[level], "-o", executable, NULL};
            int status = validation_run(v, command, NULL, 90);
            char *log = validation_read(v->log);
            if (!validation_rejected(status,
                                     log,
                                     profile == 2 ? "requires the C locale"
                                                  : "EOF or representable as unsigned char"))
                validation_fail(
                    "ctype rejection failed profile=%u level=%s\n%s", profile, levels[level], log);
            free(log);
            ++count;
        }
    checks += count;
    printf(
        "PASS %u ctype diagnostics: reachable invalid domains and pending locale snapshots, five GCC levels\n",
        count);
}

static void classical(validation *v) {
    unsigned count = 0;
    char source[4096], executable[4096], native[4096], driver[4096], output[4096], expected[4096];
    validation_path(source, sizeof source, "%s/ctype-classical.c", v->temp);
    validation_path(executable, sizeof executable, "%s/ctype-classical", v->temp);
    validation_path(native, sizeof native, "%s/ctype-reference", v->temp);
    validation_path(driver, sizeof driver, "%s/build/cket", v->root);
    validation_path(output, sizeof output, "%s/ctype-output.txt", v->temp);
    validation_path(expected, sizeof expected, "%s/ctype-expected.txt", v->temp);
    validation_write(
        source,
        "#include <ctype.h>\n#include <stdio.h>\n"
        "int main(void){for(int x=-1;x<256;++x)printf(\"%d %d %d %d %d %d %d %d %d %d %d %d %d %d\\n\","
        "isalnum(x),isalpha(x),isblank(x),iscntrl(x),isdigit(x),isgraph(x),islower(x),"
        "isprint(x),ispunct(x),isspace(x),isupper(x),isxdigit(x),tolower(x),toupper(x));"
        "int calls=0;int r=isalpha((++calls,65));return !r||calls!=1;}\n");
    const char *compiler = getenv("CC");
    if (!compiler || !*compiler)
        compiler = "gcc";
    for (unsigned builtins = 0; builtins < 2; ++builtins)
        for (unsigned level = 0; level < 5; ++level) {
            char *flag = builtins ? "-fbuiltin" : "-fno-builtin";
            char *reference_command[] = {(char *)compiler,
                                         "-std=c11",
                                         (char *)levels[level],
                                         flag,
                                         source,
                                         "-o",
                                         native,
                                         NULL};
            char *cket_command[] = {
                driver, source, (char *)levels[level], flag, "-o", executable, NULL};
            char *reference_run[] = {native, NULL}, *cket_run[] = {executable, NULL};
            if (validation_run(v, reference_command, NULL, 90) ||
                validation_run(v, cket_command, NULL, 90) ||
                validation_run(v, reference_run, expected, 90) ||
                validation_run(v, cket_run, output, 90))
                validation_fail("ctype classical differential execution failed\n%s",
                                validation_read(v->log));
            char *a = validation_read(expected), *b = validation_read(output);
            if (strcmp(a, b))
                validation_fail(
                    "ctype classical output differs at %s builtins=%u", levels[level], builtins);
            free(a);
            free(b);
            ++count;
        }
    checks += count;
    printf(
        "PASS %u ctype classical differentials: 14 interfaces, EOF and all bytes, raw results and once-only arguments\n",
        count);
}

int main(int argc, char **argv) {
    if (argc != 1)
        validation_fail("usage: validate_ctype");
    validation v;
    validation_init(&v, argv[0]);
    states(&v);
    programs(&v);
    rejections(&v);
    classical(&v);
    validation_cleanup(&v);
    printf(
        "%u ctype checks passed; C-locale byte coverage is distinct from general locale and source-folding contracts.\n",
        checks);
    return 0;
}
