#include "internal.h"
#include "validation.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
static const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};

static char *compile_case(validation *v,
                          const char *program,
                          unsigned level,
                          unsigned builtins,
                          int execute,
                          const char *diagnostic) {
    char source[4096], compiler[4096], output[4096];
    validation_path(source, sizeof source, "%s/string.c", v->temp);
    validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
    validation_path(output, sizeof output, "%s/string.out", v->temp);
    validation_write(source, program);
    char *command[] = {compiler,
                       source,
                       execute ? "--run" : "--emit-ir",
                       (char *)levels[level],
                       builtins ? "-fbuiltin" : "-fno-builtin",
                       "-o",
                       output,
                       NULL};
    int status = validation_run(v, command, NULL, 120);
    char *log = validation_read(v->log);
    if (diagnostic ? !validation_rejected(status, log, diagnostic) : status != 0)
        validation_fail("string family case failed: %s builtins=%u\n%s\n%s",
                        levels[level],
                        builtins,
                        program,
                        log);
    free(log);
    return !execute && !diagnostic ? validation_read(output) : NULL;
}

static size_t basis(const qc_register *r, unsigned value) {
    size_t index = 0;
    for (unsigned bit = 0; bit < r->width; ++bit)
        index |= (size_t)((value >> bit) & 1) << r->wires[bit];
    return index;
}

/* Ordinary libc is the reference. All bytes inspected by a successful C call
 * have a terminator; the one-byte case calls strlen only when that byte is 0. */
static int predicate(unsigned profile, unsigned x) {
    uint16_t word = (uint16_t)x;
    unsigned char byte = (unsigned char)x;
    const char *string = (const char *)&word;
    switch (profile) {
    case 0:
        return strlen(string) == 1;
    case 1:
        return strlen(string + 1) == 0;
    case 2:
        return strcmp(string, "A") <= 0;
    case 3:
        return strcmp(string, string + 1) > 0;
    case 4:
        return strcmp(string, string) == 0;
    case 5:
        return (byte == 0 ? strlen((const char *)&byte) : 3) == 0;
    }
    validation_fail("unknown string reference profile");
    return 0;
}

static void compare_state(qc_context *c, unsigned profile, uint32_t result_id, int inverse) {
    qc_register *input = qc_lookup(c, 0);
    qc_register *result = inverse ? NULL : qc_lookup(c, result_id);
    if (!input || (!inverse && !result))
        validation_fail("string reference register missing");
    double complex *expected = calloc(qc_state_size(c), sizeof *expected);
    if (!expected)
        validation_fail("string state reference allocation failed");
    for (unsigned x = 0; x < 256; ++x) {
        int value = predicate(profile, x);
        size_t index = basis(input, x) | (result ? basis(result, (unsigned)value) : 0);
        double angle = (x == 3 ? .31 : 0) + (value ? .17 : 0);
        expected[index] = cexp(I * angle) / 16;
    }
    for (size_t i = 0; i < qc_state_size(c); ++i)
        if (cabs(qc_amplitude(c, i) - expected[i]) > 1e-10)
            validation_fail(
                "string full state mismatch: profile=%u inverse=%d basis=%zu", profile, inverse, i);
    free(expected);
    if (c->reset || c->measure)
        validation_fail("string computation or inverse introduced a channel");
}

static void states(validation *v) {
    const char *expressions[] = {
        "strlen((const char*)&input)==1",
        "strlen((const char*)&input+1)==0",
        "strcmp((const char*)&input,\"A\")<=0",
        "strcmp((const char*)&input,(const char*)&input+1)>0",
        "strcmp((const char*)&input,(const char*)&input)==0",
        "(input==0?strlen((const char*)&input):3)==0",
    };
    unsigned count = 0;
    for (unsigned profile = 0; profile < sizeof expressions / sizeof *expressions; ++profile)
        for (unsigned level = 0; level < 5; ++level)
            for (unsigned builtins = 0; builtins < 2; ++builtins) {
                char program[4096];
                validation_path(program,
                                sizeof program,
                                "#include <quantum.h>\n#include <string.h>\n"
                                "int main(void){quant uint%u_t input=superposition(0,255);"
                                "if(input==3)phase(.31);quant bool result=%s;"
                                "if(result)phase(.17);measure(result);measure(input);return 0;}\n",
                                profile == 5 ? 8 : 16,
                                expressions[profile]);
                char *ir = compile_case(v, program, level, builtins, 0, NULL);
                cj_doc d = {0};
                if (!cj_parse(&d, ir))
                    validation_fail("invalid string IR");
                for (unsigned tables = 0; tables < 2; ++tables) {
                    qc_context *c = qc_create(1, 7);
                    qc_allow_tables(c, tables);
                    if (!c || !qc_verify_document(c, &d))
                        validation_fail("string IR verification failed");
                    uint64_t result_id = UINT32_MAX;
                    int list = cj_get(&d, 0, "instructions"), found = 0;
                    for (int t = list + 1; t < d.tokens[list].next; t = d.tokens[t].next) {
                        if (cj_equal(&d, cj_get(&d, t, "op"), "qalloc") &&
                            cj_equal(&d, cj_get(&d, t, "name"), "result"))
                            cj_u64(&d, cj_get(&d, t, "id"), &result_id);
                        uint64_t id;
                        if (cj_equal(&d, cj_get(&d, t, "op"), "qmeasure") &&
                            cj_u64(&d, cj_get(&d, t, "id"), &id) && id == result_id) {
                            compare_state(c, profile, (uint32_t)result_id, 0);
                            qc_register *r = qc_lookup(c, (uint32_t)result_id);
                            if (r->definition) {
                                size_t size = strlen(r->definition) + 128;
                                char *undo = malloc(size);
                                if (!undo)
                                    validation_fail("string inverse allocation failed");
                                validation_path(undo,
                                                size,
                                                "{\"op\":\"quncompute\",\"id\":%u,\"expr\":%s}",
                                                (unsigned)result_id,
                                                r->definition);
                                if (!qc_apply(c, undo, NULL))
                                    validation_fail("string inverse failed: %s", qc_error(c));
                                free(undo);
                            }
                            char release[128];
                            validation_path(release,
                                            sizeof release,
                                            "{\"op\":\"qdispose\",\"id\":%u}",
                                            (unsigned)result_id);
                            if (!qc_apply(c, release, NULL))
                                validation_fail("string clean disposal failed: %s", qc_error(c));
                            compare_state(c, profile, (uint32_t)result_id, 1);
                            found = 1;
                            break;
                        }
                        uint64_t unused;
                        if (!qc_apply_token(c, &d, t, 0, 0, &unused))
                            validation_fail(
                                "string synthesis failed profile=%u: %s", profile, qc_error(c));
                    }
                    if (!found)
                        validation_fail("string result observation missing");
                    qc_destroy(c);
                    ++count;
                }
                cj_free(&d);
                free(ir);
            }
    checks += count;
    printf(
        "PASS %u string complete phased states and saved inverses: strlen, strcmp, unsigned bytes, overlap and selected NUL domains\n",
        count);
}

static void native(validation *v) {
    const char *programs[] = {
        "int main(void){quant char a[2]={'a',0};quant uint8_t r=strlen(a);int bad=measure(r)!=1;reset(a[0]);reset(a[1]);return bad;}",
        "int main(void){quant uint16_t a=0x0061;quant uint8_t r=strlen((const char*)&a);int bad=measure(r)!=1;reset(a);return bad;}",
        "int main(void){quant uint16_t a=0;quant bool r=strlen((const char*)&a)==0;int bad=!measure(r);reset(a);return bad;}",
        "int main(void){quant uint16_t a=0x0061;quant bool r=strlen((const char*)&a+1)==0;int bad=!measure(r);reset(a);return bad;}",
        "int main(void){quant uint16_t a=0x0061;quant bool r=strcmp((const char*)&a,\"a\")==0;int bad=!measure(r);reset(a);return bad;}",
        "int main(void){quant uint16_t a=0x00ff;quant bool r=strcmp((const char*)&a,\"a\")>0;int bad=!measure(r);reset(a);return bad;}",
        "int main(void){quant uint16_t a=0x0061;quant bool r=0<strcmp((const char*)&a,\"A\");int bad=!measure(r);reset(a);return bad;}",
        "int main(void){quant uint16_t a=0x0061;quant bool r=strcmp((const char*)&a,(const char*)&a+1)>0;int bad=!measure(r);reset(a);return bad;}",
        "int main(void){quant uint8_t a=97;quant bool b=0,r=1;if(b)r=strlen((const char*)&a)==0;int bad=!measure(r);reset(a);reset(b);return bad;}",
        "int main(void){quant uint16_t a=0x6261;quant bool b=0,r=0;if(b)r=strcmp((const char*)&a,\"c\")>0;int bad=measure(r);reset(a);reset(b);return bad;}",
        "static quant bool length(quant const uint16_t *p){return strlen((const char*)p)==1;}int main(void){quant uint16_t a=0x0061;quant bool r=length(&a);int bad=!measure(r);reset(a);return bad;}",
        "static quant bool equal(quant const uint16_t *p){return strcmp((const char*)p,\"a\")==0;}int main(void){quant uint16_t a=0x0061;quant bool r=equal(&a);int bad=!measure(r);reset(a);return bad;}",
        "int main(int argc,char **argv){(void)argv;char cpu[2]={(char)(96+argc),0};quant uint16_t a=0x0061;quant bool r=strcmp((const char*)&a,cpu)==0;cpu[0]='z';int bad=!measure(r);reset(a);return bad;}",
        "int main(void){char a[3]={'a',0,'z'};size_t (*length)(const char*)=strlen;int (*compare)(const char*,const char*)=strcmp;return strlen(a)!=1||length(a)!=1||strcmp(a,\"a\")!=0||compare(a,\"b\")>=0;}",
        "int main(int argc,char **argv){(void)argv;char cpu[2]={(char)(96+argc),0};quant uint16_t a=0x0061;quant bool r=strcmp(cpu,(const char*)&a)==0;cpu[0]='z';int bad=!measure(r);reset(a);return bad;}",
    };
    unsigned count = 0;
    for (unsigned profile = 0; profile < sizeof programs / sizeof *programs; ++profile)
        for (unsigned level = 0; level < 5; ++level)
            for (unsigned builtins = 0; builtins < 2; ++builtins) {
                char program[4096];
                validation_path(program,
                                sizeof program,
                                "#include <quantum.h>\n#include <string.h>\n%s\n",
                                programs[profile]);
                free(compile_case(v, program, level, builtins, 1, NULL));
                ++count;
            }
    checks += count;
    printf(
        "PASS %u native string programs: size_t conversion, NUL, unsigned ordering, aliases, borrows, inactive domains, CPU snapshots and classical libc\n",
        count);
}

static void rejections(validation *v) {
    const struct {
        const char *declaration, *expression, *diagnostic;
    } cases[] = {
        {"quant uint16_t a=0;",
         "strlen((const char*)&a+2)",
         "needs a NUL inside its checked storage bounds"},
        {"quant uint16_t a=0;",
         "strcmp((const char*)&a+2,\"a\")",
         "needs a NUL inside both checked storage views"},
        {"quant bool a=0;", "strlen((const char*)&a)", "integer object bytes without padding"},
        {"quant bool a=0;", "strcmp((const char*)&a,\"\")", "integer object bytes without padding"},
        {"quant uint8_t a[65]={0};",
         "strlen((const char*)a)",
         "64-byte expression synthesis bound"},
        {"quant uint8_t a[65]={0};",
         "strcmp((const char*)a,\"a\")",
         "64-byte expression synthesis bound"},
        {"quant uint16_t a=0;unsigned n=1;",
         "strlen((const char*)&a+n)",
         "constant checked address"},
        {"quant uint16_t a=0;volatile char b[2]={0};",
         "strcmp((const char*)&a,(const char*)b)",
         "volatile/atomic CPU access contract"},
        {"quant uint16_t a=0;",
         "strcmp((const char*)&a,(const char*)0)",
         "requires checked quantum storage or known CPU object storage"},
        {"quant uint16_t a=0;",
         "strlen((const char*)&a-1)",
         "byte view is outside its checked storage bounds"},
    };

    unsigned count = 0;
    for (unsigned profile = 0; profile < sizeof cases / sizeof *cases; ++profile)
        for (unsigned level = 0; level < 5; ++level)
            for (unsigned builtins = 0; builtins < 2; ++builtins) {
                char program[4096];
                validation_path(
                    program,
                    sizeof program,
                    "#include <quantum.h>\n#include <string.h>\nint main(void){%squant bool r=(%s)==0;measure(r);return 0;}\n",
                    cases[profile].declaration,
                    cases[profile].expression);
                free(compile_case(v, program, level, builtins, 0, cases[profile].diagnostic));
                ++count;
            }
    const char *invalid[] = {
        "strlen((const char*)&a)",
        "strcmp((const char*)&a,\"c\")",
        "strcmp(\"c\",(const char*)&a)",
    };
    for (unsigned profile = 0; profile < sizeof invalid / sizeof *invalid; ++profile)
        for (unsigned level = 0; level < 5; ++level)
            for (unsigned builtins = 0; builtins < 2; ++builtins) {
                char program[4096];
                validation_path(
                    program,
                    sizeof program,
                    "#include <quantum.h>\n#include <string.h>\nint main(void){quant uint16_t a=0x6261;quant uint8_t r=%s;measure(r);reset(a);return 0;}\n",
                    invalid[profile]);
                free(compile_case(
                    v, program, level, builtins, 1, "C division/remainder by zero is undefined"));
                ++count;
            }
    checks += count;
    printf(
        "PASS %u string diagnostics: missing NUL, bounds, pointer provenance, effects, layouts and synthesis limits\n",
        count);
}

static void wide(validation *v) {
    unsigned count = 0;
    for (unsigned width = 16; width <= 64; width *= 2)
        for (unsigned function = 0; function < 2; ++function)
            for (unsigned level = 0; level < 5; ++level)
                for (unsigned builtins = 0; builtins < 2; ++builtins) {
                    char program[4096];
                    validation_path(
                        program,
                        sizeof program,
                        "#include <quantum.h>\n#include <string.h>\nint main(void){quant uint%u_t a=superposition(0,255);quant bool r=%s;measure(r);measure(a);return 0;}\n",
                        width,
                        function ? "strcmp((const char*)&a,\"A\")<=0"
                                 : "strlen((const char*)&a)==1");
                    char *ir = compile_case(v, program, level, builtins, 0, NULL);
                    qc_context *c = qc_create(0, 7);
                    qc_allow_tables(c, 0);
                    if (!c || !qc_document(c, ir, 0, 0) || c->reset || qc_qubits(c) > 128)
                        validation_fail("wide string synthesis failed: %s",
                                        c ? qc_error(c) : "allocation");
                    qc_destroy(c);
                    free(ir);
                    ++count;
                }
    checks += count;
    printf(
        "PASS %u string no-table wide exports: bounded strings in 16/32/64-bit objects; generation evidence\n",
        count);
}

int main(int argc, char **argv) {
    if (argc != 1)
        validation_fail("usage: validate_strings");
    validation v;
    validation_init(&v, argv[0]);
    native(&v);
    rejections(&v);
    wide(&v);
    states(&v);
    validation_cleanup(&v);
    printf(
        "%u string family checks passed; bounded strlen/strcmp adapters are distinct from complete C string compatibility.\n",
        checks);
    return 0;
}
