#define _XOPEN_SOURCE 700
#include "internal.h"
#include "validation.h"
#include "locale_fixture.h"
#include <ctype.h>
#include <locale.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

static void states(validation *v, const char *locale) {
    unsigned count = 0;
    if (!setlocale(LC_CTYPE, locale))
        validation_fail("cannot select reference locale %s", locale);
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
                    if (!qc_ctype_locale(c, locale))
                        validation_fail("cannot specialize locale %s: %s", locale, qc_error(c));
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
        "PASS %u ctype full states in %s: all 256 bytes, exact classification masks, all mapping bits, phases, clean private wires, two call forms, five GCC levels and both table policies\n",
        count,
        locale);
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
        "int ok=setlocale(LC_CTYPE,\"en_US.ISO-8859-1\")!=0;quant uint8_t x=192;quant uint8_t y=tolower(x);ok &= setlocale(LC_CTYPE,\"C\")!=0;int r=measure(y),s=measure(x);return !ok||r!=224||s!=192;",
        "int ok=setlocale(LC_CTYPE,\"en_US.ISO-8859-1\")!=0;quant uint8_t x=233;quant bool y=isalpha(x)==1024;ok &= setlocale(LC_CTYPE,\"C\")!=0;quant bool z=isalpha(x);int r=measure(y),s=measure(z),t=measure(x);return !ok||!r||s||t!=233;",
        "locale_t l=newlocale(LC_CTYPE_MASK,\"en_US.ISO-8859-1\",0);int ok=l!=0;locale_t old=uselocale(l);quant uint8_t x=192;quant uint8_t y=tolower(x);ok &= uselocale(old)!=0;freelocale(l);int r=measure(y),s=measure(x);return !ok||r!=224||s!=192;",
        "int ok=setlocale(LC_CTYPE,\"en_US.ISO-8859-1\")!=0;quant uint8_t a[1]={192};quant uint8_t y=convert(a[0]);quant bool z=toupper(tolower(a[0]))==192;int r=measure(y),s=measure(z),t=measure(a[0]);return !ok||r!=224||!s||t!=192;",
    };
    unsigned count = 0;
    for (unsigned profile = 0; profile < sizeof bodies / sizeof *bodies; ++profile)
        for (unsigned level = 0; level < 5; ++level) {
            char source[4096], compiler[4096], executable[4096], program[8192];
            validation_path(source, sizeof source, "%s/ctype-program.c", v->temp);
            validation_path(executable, sizeof executable, "%s/ctype-program", v->temp);
            validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
            validation_path(
                program,
                sizeof program,
                "#define _XOPEN_SOURCE 700\n#include <quantum.h>\n#include <ctype.h>\n#include <locale.h>\n"
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
            if (!validation_rejected(status, log, "EOF or representable as unsigned char"))
                validation_fail(
                    "ctype rejection failed profile=%u level=%s\n%s", profile, levels[level], log);
            free(log);
            ++count;
        }
    checks += count;
    printf("PASS %u ctype diagnostics: reachable invalid domains, five GCC levels\n", count);
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

static void eof_states(validation *v) {
    const char *locales[] = {"C", "C.UTF-8", "en_US.ISO-8859-1"};
    unsigned count = 0;
    char source[4096], document[4096], compiler[4096];
    validation_path(source, sizeof source, "%s/ctype-eof.c", v->temp);
    validation_path(document, sizeof document, "%s/ctype-eof.json", v->temp);
    validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
    validation_write(source,
                     "#include <quantum.h>\n#include <ctype.h>\n#include <stdio.h>\n"
                     "int main(void){quant bool x=superposition(0,1);"
                     "quant bool y=isalpha(x?EOF:255);if(y)phase(.23);measure(y);measure(x);}\n");
    for (unsigned level = 0; level < 5; ++level) {
        char *command[] = {
            compiler, source, "--emit-ir", (char *)levels[level], "-o", document, NULL};
        if (validation_run(v, command, NULL, 90))
            validation_fail("ctype EOF compilation failed\n%s", validation_read(v->log));
        char *ir = validation_read(document);
        for (unsigned locale = 0; locale < 3; ++locale) {
            if (!setlocale(LC_CTYPE, locales[locale]))
                validation_fail("cannot select EOF reference locale");
            for (unsigned tables = 0; tables < 2; ++tables) {
                qc_context *c = qc_create(1, 7);
                if (!c || !qc_ctype_locale(c, locales[locale]))
                    validation_fail("cannot capture EOF locale");
                qc_allow_tables(c, tables);
                if (!qc_document(c, ir, 0, 1))
                    validation_fail("coherent EOF state failed: %s", qc_error(c));
                qc_register *x = qc_lookup(c, 0), *y = qc_lookup(c, 1);
                if (!x || !y || c->measure != 2 || c->reset)
                    validation_fail("coherent EOF introduced channels");
                size_t a = register_basis(y, !!reference[1](255));
                size_t b = register_basis(x, 1);
                for (size_t basis = 0; basis < qc_state_size(c); ++basis) {
                    double complex expected = basis == a   ? cexp(I * (a ? .23 : 0)) / sqrt(2.)
                                              : basis == b ? 1 / sqrt(2.)
                                                           : 0;
                    if (cabs(qc_amplitude(c, basis) - expected) > 1e-10)
                        validation_fail("EOF and byte 255 were conflated: %s tables=%u",
                                        locales[locale],
                                        tables);
                }
                qc_destroy(c);
                ++count;
            }
        }
        free(ir);
    }
    checks += count;
    printf(
        "PASS %u coherent EOF/byte-255 complete states: three locales, five GCC levels and both synthesis policies\n",
        count);
}

static void profile_contracts(void) {
    const char *invalid[] = {"null",
                             "\"unknown\"",
                             "[[0,256,1]]",
                             "[[2,1,1]]",
                             "[[1,2,1],[2,3,1]]",
                             "[[0,1,0]]",
                             "[[0,1,-1]]",
                             "[[0,1,2147483648]]",
                             "[[0,1,true]]",
                             "[[0,1]]"};
    unsigned count = 0;
    for (unsigned i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
        qc_context *c = qc_create(0, 7);
        char expression[1024];
        snprintf(expression,
                 sizeof expression,
                 "{\"op\":\"ctype_isalpha\",\"bits\":32,\"signed\":true,\"profile\":%s,"
                 "\"args\":[{\"op\":\"const\",\"bits\":32,\"signed\":true,\"value\":65}]}",
                 invalid[i]);
        cj_doc d = {0};
        if (!c || !cj_parse(&d, expression) || qc_verify_expression(c, &d, 0) || !qc_error(c)[0])
            validation_fail("malformed ctype profile accepted: %u", i);
        cj_free(&d);
        qc_destroy(c);
        ++count;
    }
    qc_context *c = qc_create(0, 7);
    uint64_t result;
    if (!c || qc_apply(c, "{\"op\":\"ctype_capture\"}", &result) ||
        !strstr(qc_error(c), "--ctype-locale") || c->ngates)
        validation_fail("static export silently chose the runtime locale");
    qc_destroy(c);
    ++count;
    c = qc_create(0, 7);
    if (!c || qc_ctype_locale(c, "cket-invalid-locale") || !qc_error(c)[0])
        validation_fail("unavailable static locale was accepted");
    qc_destroy(c);
    ++count;
    checks += count;
    printf(
        "PASS %u immutable locale profile contracts: malformed intervals, explicit static specialization and unavailable locale\n",
        count);
}

static void module_profiles(void) {
    const char *template =
        "{\"format\":\"cket.qir\",\"version\":1,\"instructions\":["
        "{\"op\":\"ctype_capture\"},{\"op\":\"qborrow\",\"id\":0,\"width\":8,\"signed\":false},"
        "{\"op\":\"qalloc\",\"id\":1,\"width\":1,\"signed\":false},"
        "{\"op\":\"qcompute\",\"id\":1,\"expr\":{\"op\":\"cast\",\"bits\":1,\"signed\":false,\"args\":["
        "{\"op\":\"ctype_isalpha\",\"bits\":32,\"signed\":true,\"args\":["
        "{\"op\":\"ctype_tolower\",\"bits\":32,\"signed\":true,\"profile\":\"current\",\"args\":["
        "{\"op\":\"cast\",\"bits\":32,\"signed\":true,\"args\":["
        "{\"op\":\"reg\",\"id\":0,\"bits\":8,\"signed\":false}]}]}],\"profile\":\"current\"}]}},"
        "{\"op\":\"qphase\",\"angle\":0.37,\"controls\":[{\"id\":1,\"value\":1}]},"
        "{\"op\":\"quncompute\",\"id\":1,\"saved\":true},{\"op\":\"qrelease\",\"id\":1}]}";
    char error[512];
    qc_circuit *unbound = qc_circuit_compile(template, error, sizeof error);
    if (unbound || !strstr(error, "CPU locale"))
        validation_fail("unitary module accepted a dynamic locale capture: %s", error);
    ++checks;
    qc_context *binder = qc_create(0, 7);
    if (!binder || !qc_ctype_locale(binder, "en_US.ISO-8859-1"))
        validation_fail("cannot capture module locale");
    char *ir = qc_ctype_specialize_document(binder, template);
    qc_destroy(binder);
    if (!ir || strstr(ir, "current") || strstr(ir, "ctype_capture"))
        validation_fail("module artifact retained mutable locale references");
    qc_circuit *module = qc_circuit_compile(ir, error, sizeof error);
    if (!module)
        validation_fail("immutable module compilation failed: %s", error);
    if (!setlocale(LC_CTYPE, "en_US.ISO-8859-1"))
        validation_fail("cannot select module reference locale");
    unsigned char expected[256];
    for (unsigned byte = 0; byte < 256; ++byte)
        expected[byte] = !!reference[1](reference[12]((int)byte));
    if (!setlocale(LC_CTYPE, "C"))
        validation_fail("cannot change locale before module execution");
    for (unsigned tables = 0; tables < 2; ++tables) {
        qc_context *c = qc_create(1, 7);
        uint64_t result;
        uint32_t input = 7;
        if (!c)
            validation_fail("cannot allocate module state");
        qc_allow_tables(c, tables);
        if (!qc_apply(c, "{\"op\":\"qalloc\",\"id\":7,\"width\":8,\"signed\":false}", &result) ||
            !qc_apply(c, "{\"op\":\"qprepare\",\"id\":7,\"lower\":0,\"upper\":255}", &result) ||
            !qc_circuit_apply(c, module, &input, 1, 0, NULL, NULL, 0))
            validation_fail("immutable locale module failed: %s", qc_error(c));
        qc_register *x = qc_lookup(c, input);
        for (size_t basis = 0; basis < qc_state_size(c); ++basis) {
            double complex amplitude = 0;
            for (unsigned byte = 0; byte < 256; ++byte)
                if (basis == register_basis(x, byte))
                    amplitude = cexp(I * (expected[byte] ? .37 : 0)) / 16;
            if (cabs(qc_amplitude(c, basis) - amplitude) > 1e-10)
                validation_fail("module profile lost its saved locale or private cleanup");
        }
        if (!qc_circuit_apply(c, module, &input, 1, 1, NULL, NULL, 0) || c->measure || c->reset)
            validation_fail("locale module inverse introduced channels");
        for (size_t basis = 0; basis < qc_state_size(c); ++basis) {
            double expected_amplitude = basis < 256 ? 1 / 16. : 0;
            if (cabs(qc_amplitude(c, basis) - expected_amplitude) > 1e-10)
                validation_fail("locale module inverse failed to restore the full state");
        }
        qc_destroy(c);
        ++checks;
    }
    qc_circuit_destroy(module);
    free(ir);
    puts(
        "PASS 3 locale module contracts: unresolved capture rejection, immutable phased byte states and full inverses under another locale");
}

int main(int argc, char **argv) {
    if (argc != 1)
        validation_fail("usage: validate_ctype");
    validation v;
    validation_init(&v, argv[0]);
    validation_locale_fixture(&v, 0);
    states(&v, "C");
    states(&v, "C.UTF-8");
    states(&v, "en_US.ISO-8859-1");
    eof_states(&v);
    profile_contracts();
    module_profiles();
    if (!setlocale(LC_CTYPE, "C"))
        validation_fail("cannot restore the reference C locale");
    programs(&v);
    rejections(&v);
    classical(&v);
    validation_locale_fixture(&v, 1);
    validation_cleanup(&v);
    printf(
        "%u ctype checks passed; immutable byte-locale coverage is distinct from wide-character, concurrency and source-folding contracts.\n",
        checks);
    return 0;
}
