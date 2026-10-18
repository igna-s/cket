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
        validation_fail(
            "strncmp case failed: %s builtins=%u\n%s\n%s", levels[level], builtins, program, log);
    free(log);
    return !execute && !diagnostic ? validation_read(output) : NULL;
}

static size_t basis(const qc_register *r, unsigned value) {
    size_t index = 0;
    for (unsigned bit = 0; bit < r->width; ++bit)
        index |= (size_t)((value >> bit) & 1) << r->wires[bit];
    return index;
}

static int predicate(unsigned profile, unsigned x, unsigned y) {
    uint16_t word = (uint16_t)x, reference = profile == 1 ? 0xab00 : 0xab41;
    unsigned char byte = (unsigned char)x, other = profile == 3 ? (unsigned char)y : 65;
    const char *left = (const char *)&byte, *right = (const char *)&other;
    size_t count = 1;
    if (profile == 1 || profile == 2 || profile == 4) {
        left = (const char *)&word;
        right = (const char *)&reference;
        count = profile == 4 ? 0 : sizeof word;
    }
    return strncmp(left, right, count) <= 0;
}

static void compare_state(qc_context *c, unsigned profile, uint32_t result_id, int inverse) {
    qc_register *input = qc_lookup(c, 0), *other = profile == 3 ? qc_lookup(c, 1) : NULL;
    qc_register *result = inverse ? NULL : qc_lookup(c, result_id);
    unsigned nx = profile == 3 ? 8 : profile == 0 || profile == 5 ? 256 : 65536;
    unsigned ny = profile == 3 ? 8 : 1;
    if (!input || (!inverse && !result) || (profile == 3 && !other))
        validation_fail("strncmp expected register missing");
    double complex *expected = calloc(qc_state_size(c), sizeof *expected);
    if (!expected)
        validation_fail("strncmp state reference allocation failed");
    for (unsigned x = 0; x < nx; ++x)
        for (unsigned y = 0; y < ny; ++y) {
            int value = predicate(profile, x, y);
            size_t index = basis(input, x) | (other ? basis(other, y) : 0) |
                           (result ? basis(result, (unsigned)value) : 0);
            double angle =
                (x == 3 ? .31 : 0) + (value ? .17 : 0) + (profile == 3 && y == 7 ? .23 : 0);
            expected[index] = cexp(I * angle) / sqrt((double)nx * ny);
        }
    for (size_t i = 0; i < qc_state_size(c); ++i)
        if (cabs(qc_amplitude(c, i) - expected[i]) > 1e-10)
            validation_fail("strncmp full state mismatch: profile=%u inverse=%d basis=%zu",
                            profile,
                            inverse,
                            i);
    free(expected);
    if (c->reset || c->measure)
        validation_fail("strncmp computation or inverse introduced a channel");
}

static void states(validation *v) {
    const char *declarations[] = {
        "quant uint8_t input=superposition(0,255);",
        "quant uint16_t input=superposition(0,65535);const uint16_t ref=0xab00;",
        "quant uint16_t input=superposition(0,65535);const uint16_t ref=0xab41;",
        "quant uint8_t input=superposition(0,7),other=superposition(0,7);if(other==7)phase(.23);",
        "quant uint16_t input=superposition(0,65535);const uint16_t ref=0xab00;",
        "quant int8_t input=0;fourier(input);",
    };
    const char *expressions[] = {
        "strncmp((const char*)&input,\"A\",1)<=0",
        "strncmp((const char*)&input,(const char*)&ref,2)<=0",
        "strncmp((const char*)&input,(const char*)&ref,2)<=0",
        "strncmp((const char*)&input,(const char*)&other,1)<=0",
        "strncmp((const char*)&input+2,(const char*)&ref+2,0)==0",
        "strncmp((const char*)&input,\"A\",1)<=0",
    };
    unsigned count = 0;
    for (unsigned profile = 0; profile < 6; ++profile)
        for (unsigned level = 0; level < 5; ++level) {
            if (profile != 0 && profile != 5 && level != 2)
                continue;
            for (unsigned builtins = 0; builtins < 2; ++builtins) {
                char program[4096];
                validation_path(program,
                                sizeof program,
                                "#include <quantum.h>\n#include <string.h>\nint main(void){%s"
                                "if(input==3)phase(.31);quant bool result=%s;if(result)phase(.17);"
                                "measure(result);measure(input);%sreturn 0;}\n",
                                declarations[profile],
                                expressions[profile],
                                profile == 3 ? "measure(other);" : "");
                char *ir = compile_case(v, program, level, builtins, 0, NULL);
                cj_doc d = {0};
                if (!cj_parse(&d, ir))
                    validation_fail("strncmp generated invalid IR");
                for (unsigned tables = 0; tables < 2; ++tables) {
                    qc_context *c = qc_create(1, 7);
                    qc_allow_tables(c, tables);
                    if (!c || !qc_verify_document(c, &d))
                        validation_fail("strncmp IR verification failed");
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
                                    validation_fail("strncmp inverse allocation failed");
                                validation_path(undo,
                                                size,
                                                "{\"op\":\"quncompute\",\"id\":%u,\"expr\":%s}",
                                                (unsigned)result_id,
                                                r->definition);
                                if (!qc_apply(c, undo, NULL))
                                    validation_fail("strncmp saved inverse failed: %s",
                                                    qc_error(c));
                                free(undo);
                            }
                            char release[128];
                            validation_path(release,
                                            sizeof release,
                                            "{\"op\":\"qdispose\",\"id\":%u}",
                                            (unsigned)result_id);
                            if (!qc_apply(c, release, NULL))
                                validation_fail("strncmp clean disposal failed profile=%u: %s",
                                                profile,
                                                qc_error(c));
                            compare_state(c, profile, (uint32_t)result_id, 1);
                            found = 1;
                            break;
                        }
                        uint64_t unused;
                        if (!qc_apply_token(c, &d, t, 0, 0, &unused))
                            validation_fail("strncmp synthesis failed: %s", qc_error(c));
                    }
                    if (!found)
                        validation_fail("strncmp result observation missing");
                    qc_destroy(c);
                    ++count;
                }
                cj_free(&d);
                free(ir);
            }
        }
    checks += count;
    printf(
        "PASS %u strncmp complete phased states and saved inverses: bytes, signed object bits, all two-byte patterns, NUL stop, continued tails, two quantum inputs and zero-length one-past views\n",
        count);
}

static void native(validation *v) {
    const char *programs[] = {
        "static quant bool equal(quant const uint16_t *p){const uint16_t ref=0xab00;return strncmp((const char*)p,(const char*)&ref,2)==0;}int main(void){quant uint16_t a=0x1200;quant bool r=equal(&a);int bad=!measure(r);reset(a);return bad;}",
        "int main(void){quant uint16_t a=0x0041;const uint16_t b=0xab41;quant bool r=strncmp((const char*)&a,(const char*)&b,2)<0;int bad=!measure(r);reset(a);return bad;}",
        "int main(void){quant uint16_t a=257;const uint16_t b=0;quant bool r=strncmp((const char*)&a+2,(const char*)&b+2,0)==0;int bad=!measure(r);reset(a);return bad;}",
        "int main(void){quant uint8_t a=128;const char b=127;quant bool r=0<strncmp((const char*)&a,&b,1);int bad=!measure(r);reset(a);return bad;}",
        "int main(void){quant uint16_t a=0x4141;quant bool r=strncmp((const char*)&a,(const char*)&a+1,1)==0;int bad=!measure(r);reset(a);return bad;}",
        "int main(void){quant uint8_t a=65;const char b=66;quant uint8_t r=strncmp((const char*)&a,&b,1);int bad=measure(r)!=255;reset(a);return bad;}",
        "int main(void){quant uint8_t a[2]={0,255};const char b[2]={0,17};quant uint8_t r=strncmp((const char*)a,b,2);int bad=measure(r)!=0;reset(a[0]);reset(a[1]);return bad;}",
        "int main(void){char a[3]={0,127,1},b[3]={0,-1,2};char c[3]={'A',0,127};return strncmp(a,b,3)!=0||strncmp(c,\"B\",1)>=0||strncmp(a+3,b+3,0)!=0;}",
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
        "PASS %u native strncmp programs: const helper borrows, NUL termination, continued tails, zero count, unsigned bytes, reversed relation, overlap, raw result and classical libc\n",
        count);
}

static void rejections(validation *v) {
    const struct {
        const char *declaration, *expression, *diagnostic;
    } cases[] = {
        {"quant uint8_t a=1;",
         "strncmp((const char*)&a,\"ab\",2)",
         "exceeds its checked storage bounds"},
        {"quant uint8_t a=1;",
         "strncmp((const char*)&a+2,\"a\",0)",
         "byte view is outside its checked storage bounds"},
        {"quant uint8_t a=1;",
         "strncmp((const char*)&a-1,\"a\",1)",
         "byte view is outside its checked storage bounds"},
        {"quant uint8_t a=1;unsigned n=1;",
         "strncmp((const char*)&a,\"a\",n)",
         "requires a constant byte count"},
        {"quant uint8_t a=1;volatile char b=2;",
         "strncmp((const char*)&a,&b,1)",
         "volatile/atomic CPU access contract"},
        {"quant bool a=false;",
         "strncmp((const char*)&a,\"a\",1)",
         "integer object bytes without padding"},
        {"quant uint8_t a[65]={0};const char b[65]={0};",
         "strncmp((const char*)a,b,65)",
         "64-byte expression synthesis bound"},
        {"quant uint16_t a=1;unsigned n=1;",
         "strncmp((const char*)&a+n,\"a\",1)",
         "constant checked address"},
    };

    unsigned count = 0;
    for (unsigned profile = 0; profile < sizeof cases / sizeof *cases; ++profile)
        for (unsigned level = 0; level < 5; ++level)
            for (unsigned builtins = 0; builtins < 2; ++builtins) {
                char program[4096];
                validation_path(
                    program,
                    sizeof program,
                    "#include <quantum.h>\n#include <string.h>\nint main(void){%squant bool r=(%s)<0;measure(r);return 0;}\n",
                    cases[profile].declaration,
                    cases[profile].expression);
                free(compile_case(v, program, level, builtins, 0, cases[profile].diagnostic));
                ++count;
            }
    checks += count;
    printf(
        "PASS %u strncmp diagnostics: checked extents/offsets, volatile/atomic CPU access, dynamic counts, padding and expression budget\n",
        count);
}

static void wide(validation *v) {
    unsigned count = 0;
    for (unsigned width = 16; width <= 64; width *= 2)
        for (unsigned level = 0; level < 5; ++level)
            for (unsigned builtins = 0; builtins < 2; ++builtins) {
                char program[4096];
                validation_path(
                    program,
                    sizeof program,
                    "#include <quantum.h>\n#include <string.h>\nint main(void){quant uint%u_t a=0;fourier(a);const uint%u_t b=0xab00;quant bool r=strncmp((const char*)&a,(const char*)&b,sizeof a)<=0;measure(r);measure(a);return 0;}\n",
                    width,
                    width);
                char *ir = compile_case(v, program, level, builtins, 0, NULL);
                qc_context *c = qc_create(0, 7);
                qc_allow_tables(c, 0);
                if (!c || !qc_document(c, ir, 0, 0) || c->reset || qc_qubits(c) > 128)
                    validation_fail("wide strncmp synthesis failed: %s",
                                    c ? qc_error(c) : "allocation");
                qc_destroy(c);
                free(ir);
                ++count;
            }
    checks += count;
    printf("PASS %u strncmp no-table wide exports: 16/32/64-bit objects; generation evidence\n",
           count);
}

int main(int argc, char **argv) {
    if (argc != 1)
        validation_fail("usage: validate_strncmp");
    validation v;
    validation_init(&v, argv[0]);
    states(&v);
    native(&v);
    rejections(&v);
    wide(&v);
    validation_cleanup(&v);
    printf(
        "%u strncmp checks passed; NUL-terminated comparisons remain distinct from complete string/library support.\n",
        checks);
    return 0;
}
