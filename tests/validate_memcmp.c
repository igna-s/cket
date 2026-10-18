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
                          int run,
                          const char *diagnostic) {
    char source[4096], compiler[4096], output[4096];
    validation_path(source, sizeof source, "%s/compare.c", v->temp);
    validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
    validation_path(output, sizeof output, "%s/compare.out", v->temp);
    validation_write(source, program);
    char *command[] = {compiler,
                       source,
                       run ? "--run" : "--emit-ir",
                       (char *)levels[level],
                       builtins ? "-fbuiltin" : "-fno-builtin",
                       "-o",
                       output,
                       NULL};
    int code = validation_run(v, command, NULL, 120);
    char *log = validation_read(v->log);
    if (diagnostic ? !validation_rejected(code, log, diagnostic) : code != 0)
        validation_fail(
            "memcmp case failed at %s builtins=%u: %s\n%s", levels[level], builtins, program, log);
    free(log);
    return !diagnostic && !run ? validation_read(output) : NULL;
}

static size_t basis(const qc_register *r, unsigned value) {
    size_t result = 0;
    for (unsigned bit = 0; bit < r->width; ++bit)
        result |= (size_t)((value >> bit) & 1) << r->wires[bit];
    return result;
}

static int difference(unsigned profile, unsigned x, unsigned y) {
    uint16_t word = (uint16_t)x, reference = 0xab20;
    unsigned char byte = (unsigned char)x, other = (unsigned char)y;
    const unsigned char *a = &byte, *b = &other;
    size_t count = 1;
    if (profile == 0)
        other = 65;
    if (profile == 1)
        other = 128;
    if (profile == 2) {
        a = (const unsigned char *)&word;
        b = (const unsigned char *)&reference;
        count = 2;
    }
    if (profile == 3) {
        a = (const unsigned char *)&word + 1;
        other = 81;
    }
    if (profile == 5)
        other = 2;
    if (profile == 6)
        other = byte;
    int result = 0;
    for (size_t i = 0; i < count; ++i)
        if (a[i] != b[i]) {
            result = (int)a[i] - b[i];
            break;
        }
    int classical = memcmp(a, b, count);
    if ((classical > 0) - (classical < 0) != (result > 0) - (result < 0))
        validation_fail("memcmp reference disagrees with C");
    return result;
}

static void compare_state(qc_context *c, unsigned profile, uint32_t result_id, int inverse) {
    double complex *expected = calloc(qc_state_size(c), sizeof *expected);
    if (!expected)
        validation_fail("memcmp state reference allocation failed");
    qc_register *a = qc_lookup(c, 0), *b = profile == 4 ? qc_lookup(c, 1) : NULL;
    qc_register *r = inverse ? NULL : qc_lookup(c, result_id);
    unsigned nx = profile == 2 || profile == 3 ? 65536 : profile == 4 || profile == 5 ? 8 : 256;
    unsigned ny = profile == 4 ? 8 : 1;
    if (!a || (!inverse && !r) || (profile == 4 && !b))
        validation_fail("memcmp state register missing");
    for (unsigned x = 0; x < nx; ++x)
        for (unsigned y = 0; y < ny; ++y) {
            int diff = difference(profile, x, y);
            unsigned value = profile == 1   ? diff >= 0
                             : profile == 5 ? (unsigned char)diff
                             : profile == 6 ? diff == 0
                                            : diff < 0;
            size_t index = basis(a, x) | (b ? basis(b, y) : 0) | (r ? basis(r, value) : 0);
            expected[index] = cexp(I * ((x == 3 ? .31 : 0) + (profile == 4 && y == 7 ? .17 : 0))) /
                              sqrt((double)nx * ny);
        }
    for (size_t i = 0; i < qc_state_size(c); ++i)
        if (cabs(qc_amplitude(c, i) - expected[i]) > 1e-10)
            validation_fail("memcmp complete state mismatch: profile=%u inverse=%d basis=%zu",
                            profile,
                            inverse,
                            i);
    free(expected);
}

static void states(validation *v) {
    const char *declarations[] = {
        "quant uint8_t a=superposition(0,255);",
        "quant int8_t a=0;fourier(a);",
        "quant uint16_t a=superposition(0,65535);const uint16_t b=0xab20;",
        "quant uint16_t a=superposition(0,65535);",
        "quant uint8_t a=superposition(0,7),b=superposition(0,7);if(b==7)phase(.17);",
        "quant uint8_t a=superposition(0,7);",
        "quant uint8_t a=superposition(0,255);",
    };
    const char *computations[] = {
        "quant bool result=memcmp((const void*)&a,\"A\",1)<0;",
        "quant bool result=memcmp((const void*)&a,\"\\x80\",1)>=0;",
        "quant bool result=memcmp((const void*)&a,&b,2)<0;",
        "quant bool result=memcmp((const char*)&a+1,\"Q\",1)<0;",
        "quant bool result=memcmp((const void*)&a,(const void*)&b,1)<0;",
        "quant uint8_t result=memcmp((const void*)&a,\"\\x02\",1);",
        "quant bool result=memcmp((const void*)&a,(const void*)&a,1)==0;",
    };
    unsigned count = 0;
    for (unsigned profile = 0; profile < 7; ++profile)
        for (unsigned level = 0; level < 5; ++level)
            for (unsigned builtins = 0; builtins < 2; ++builtins) {
                // Wider complete-state cases complement the full GCC matrix for
                // byte inputs. They are checked at -O2 in both builtin modes.
                if (profile >= 2 && profile <= 4 && level != 2)
                    continue;
                char program[4096];
                validation_path(program,
                                sizeof program,
                                "#include <quantum.h>\n#include <string.h>\nint main(void){%s"
                                "if(a==3)phase(.31);%smeasure(result);measure(a);%sreturn 0;}\n",
                                declarations[profile],
                                computations[profile],
                                profile == 4 ? "measure(b);" : "");
                char *ir = compile_case(v, program, level, builtins, 0, NULL);
                cj_doc d = {0};
                if (!cj_parse(&d, ir))
                    validation_fail("memcmp generated invalid IR");
                for (unsigned tables = 0; tables < 2; ++tables) {
                    qc_context *c = qc_create(1, 7);
                    qc_allow_tables(c, tables);
                    if (!c || !qc_verify_document(c, &d))
                        validation_fail("memcmp IR verification failed");
                    int list = cj_get(&d, 0, "instructions"), found = 0;
                    uint64_t id = UINT32_MAX;
                    for (int t = list + 1; t < d.tokens[list].next; t = d.tokens[t].next) {
                        if (cj_equal(&d, cj_get(&d, t, "op"), "qalloc") &&
                            cj_equal(&d, cj_get(&d, t, "name"), "result"))
                            cj_u64(&d, cj_get(&d, t, "id"), &id);
                        uint64_t target;
                        int result = cj_equal(&d, cj_get(&d, t, "op"), "qcompute") &&
                                     cj_u64(&d, cj_get(&d, t, "id"), &target) && target == id;
                        uint64_t unused;
                        if (!qc_apply_token(c, &d, t, 0, 0, &unused))
                            validation_fail("memcmp synthesis failed: %s", qc_error(c));
                        if (!result)
                            continue;
                        compare_state(c, profile, (uint32_t)id, 0);
                        qc_register *r = qc_lookup(c, (uint32_t)id);
                        size_t size = strlen(r->definition) + 128;
                        char *inverse = malloc(size);
                        if (!inverse)
                            validation_fail("memcmp inverse allocation failed");
                        validation_path(inverse,
                                        size,
                                        "{\"op\":\"quncompute\",\"id\":%u,\"expr\":%s}",
                                        (unsigned)id,
                                        r->definition);
                        if (!qc_apply(c, inverse, NULL))
                            validation_fail("memcmp inverse failed: %s", qc_error(c));
                        free(inverse);
                        validation_path(program,
                                        sizeof program,
                                        "{\"op\":\"qrelease\",\"id\":%u}",
                                        (unsigned)id);
                        if (!qc_apply(c, program, NULL))
                            validation_fail("memcmp release failed: %s", qc_error(c));
                        compare_state(c, profile, (uint32_t)id, 1);
                        if (c->measure || c->reset)
                            validation_fail("memcmp introduced a hidden channel");
                        found = 1;
                        ++count;
                        break;
                    }
                    if (!found)
                        validation_fail("memcmp result computation missing");
                    qc_destroy(c);
                }
                cj_free(&d);
                free(ir);
            }
    checks += count;
    printf(
        "PASS %u memcmp complete phased states and inverses: bytes, signed representation, lexicographic words, byte views, two quantum inputs and raw results\n",
        count);
}

static void native(validation *v) {
    const char *relations[] = {"<", "<=", ">", ">=", "==", "!=", ""};
    for (unsigned relation = 0; relation < 7; ++relation) {
        char program[4096], comparison[256], expected[256];
        if (relation < 6) {
            validation_path(comparison,
                            sizeof comparison,
                            "memcmp((const void*)&a,\"\\x80\",1)%s0",
                            relations[relation]);
            validation_path(expected, sizeof expected, "i%s128", relations[relation]);
        } else {
            validation_path(comparison, sizeof comparison, "memcmp((const void*)&a,\"A\",1)");
            validation_path(expected, sizeof expected, "(uint8_t)(i-65)");
        }
        validation_path(program,
                        sizeof program,
                        "#include <quantum.h>\n#include <string.h>\nint main(void){int bad=0;"
                        "for(unsigned i=0;i<256;++i){quant uint8_t a=i;quant %s r=%s;"
                        "bad|=measure(r)!=(%s);reset(r);reset(a);}return bad;}\n",
                        relation < 6 ? "bool" : "uint8_t",
                        comparison,
                        expected);
        for (unsigned level = 0; level < 5; ++level)
            for (unsigned builtins = 0; builtins < 2; ++builtins) {
                free(compile_case(v, program, level, builtins, 1, NULL));
                ++checks;
            }
    }
    const char *programs[] = {
        "#include <quantum.h>\n#include <string.h>\n"
        "static quant bool less(quant const uint8_t*a,quant const uint8_t*b){return memcmp(a,b,1)<0;}"
        "int main(void){quant uint8_t a=128,b=255;quant bool r=less(&a,&b);int bad=!measure(r);"
        "reset(a);reset(b);return bad;}\n",
        "#include <quantum.h>\n#include <string.h>\nint main(void){"
        "quant uint16_t a=0x1234;const unsigned char b[3]={[0]=0x34,[1]=0x12};"
        "quant bool r=memcmp((const void*)&a,b,2)==0;int bad=!measure(r);"
        "quant bool s=0<memcmp((const char*)&a+1,b,1);bad|=measure(s)!=0;"
        "quant bool t=memcmp((const char*)&a+2,b+3,0)==0;bad|=!measure(t);"
        "reset(a);return bad;}\n",
        "#include <quantum.h>\n#include <string.h>\nint main(void){"
        "quant uint8_t a=1;const uint8_t b=3;quant bool r=((uint8_t)memcmp((const void*)&a,&b,1))>0;"
        "int bad=!measure(r);reset(a);return bad;}\n",
        "#include <quantum.h>\n#include <string.h>\nint main(void){"
        "unsigned char a[3]={0,2,255},b[3]={0,3,0};"
        "return !(memcmp(a,b,3)<0&&memcmp(a,a,3)==0&&memcmp(a+3,b+3,0)==0);}\n",
    };
    for (unsigned i = 0; i < sizeof programs / sizeof *programs; ++i)
        for (unsigned level = 0; level < 5; ++level)
            for (unsigned builtins = 0; builtins < 2; ++builtins) {
                free(compile_case(v, programs[i], level, builtins, 1, NULL));
                ++checks;
            }
    puts(
        "PASS 110 native memcmp programs: all byte values, six relations, raw result, const borrows, byte offsets, one-past zero length, cast semantics and classical libc");
}

static void rejections(validation *v) {
    const struct {
        const char *declaration, *expression, *diagnostic;
    } cases[] = {
        {"quant uint8_t a=1;",
         "memcmp((const void*)&a,\"ab\",2)",
         "exceeds its checked storage bounds"},
        {"quant uint8_t a=1;",
         "memcmp((const char*)&a+2,\"a\",0)",
         "byte view is outside its checked storage bounds"},
        {"quant uint8_t a=1;",
         "memcmp((const char*)&a-1,\"a\",1)",
         "byte view is outside its checked storage bounds"},
        {"quant uint8_t a=1;unsigned n=1;",
         "memcmp((const void*)&a,\"a\",n)",
         "requires a constant byte count"},
        {"quant uint8_t a=1;volatile unsigned char b=2;",
         "memcmp((const void*)&a,&b,1)",
         "volatile/atomic CPU access contract"},
        {"quant bool a=false;",
         "memcmp((const void*)&a,\"a\",1)",
         "integer object bytes without padding"},
        {"quant uint8_t a[65]={0};const uint8_t b[65]={0};",
         "memcmp((const void*)a,b,65)",
         "64-byte expression synthesis bound"},
    };

    for (unsigned i = 0; i < sizeof cases / sizeof *cases; ++i)
        for (unsigned level = 0; level < 5; ++level)
            for (unsigned builtins = 0; builtins < 2; ++builtins) {
                char program[4096];
                validation_path(
                    program,
                    sizeof program,
                    "#include <quantum.h>\n#include <string.h>\nint main(void){%squant bool result=(%s)<0;"
                    "measure(result);return 0;}\n",
                    cases[i].declaration,
                    cases[i].expression);
                free(compile_case(v, program, level, builtins, 0, cases[i].diagnostic));
                ++checks;
            }
    puts(
        "PASS 70 memcmp rejection contracts: bounds, offsets, dynamic extents, volatile/atomic CPU access, padding and expression budget");
}

static void wide(validation *v) {
    for (unsigned width = 16; width <= 64; width *= 2)
        for (unsigned level = 0; level < 5; ++level)
            for (unsigned builtins = 0; builtins < 2; ++builtins) {
                char program[4096];
                validation_path(
                    program,
                    sizeof program,
                    "#include <quantum.h>\n#include <string.h>\nint main(void){quant uint%u_t a=0;fourier(a);"
                    "const uint%u_t b=0x1234;quant bool result=memcmp((const void*)&a,&b,sizeof a)<0;"
                    "measure(result);measure(a);return 0;}\n",
                    width,
                    width);
                char *ir = compile_case(v, program, level, builtins, 0, NULL);
                qc_context *c = qc_create(0, 7);
                qc_allow_tables(c, 0);
                if (!c || !qc_document(c, ir, 0, 0) || c->reset || qc_qubits(c) > 128)
                    validation_fail("wide memcmp synthesis failed: %s",
                                    c ? qc_error(c) : "allocation");
                free(ir);
                qc_destroy(c);
                ++checks;
            }
    puts(
        "PASS 30 memcmp wide no-table circuits: 16/32/64-bit layouts, five GCC levels and both builtin modes; generation evidence");
}

int main(int argc, char **argv) {
    if (argc != 1)
        validation_fail("usage: validate_memcmp");
    validation v;
    validation_init(&v, argv[0]);
    states(&v);
    native(&v);
    rejections(&v);
    wide(&v);
    validation_cleanup(&v);
    printf(
        "%u memcmp checks passed; coherent byte comparisons are distinct from general memory/library completeness.\n",
        checks);
    return 0;
}
