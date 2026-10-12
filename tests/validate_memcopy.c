#include "internal.h"
#include "validation.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
static const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};

static char *compile_case(
    validation *v, const char *program, unsigned level, int builtins, int run, int expected) {
    char source[4096], compiler[4096], artifact[4096];
    validation_path(source, sizeof source, "%s/memory.c", v->temp);
    validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
    validation_path(artifact, sizeof artifact, "%s/memory.json", v->temp);
    validation_write(source, program);
    char *command[] = {compiler,
                       source,
                       run ? "--run" : "--emit-ir",
                       (char *)levels[level],
                       builtins ? "-fbuiltin" : "-fno-builtin",
                       "-o",
                       artifact,
                       NULL};
    int code = validation_run(v, command, NULL, 60);
    if ((code == 0) != expected) {
        char *log = validation_read(v->log);
        validation_fail("memory case failed %s builtins=%d expected=%d: %s\n%s",
                        levels[level],
                        builtins,
                        expected,
                        program,
                        log);
    }
    return !run && expected ? validation_read(artifact) : NULL;
}

static size_t basis(qc_register *r, unsigned value) {
    size_t result = 0;
    for (unsigned b = 0; b < r->width; ++b)
        result |= (size_t)((value >> b) & 1) << r->wires[b];
    return result;
}

static void compare(qc_context *c, const double complex *expected) {
    for (size_t i = 0; i < qc_state_size(c); ++i)
        if (cabs(qc_amplitude(c, i) - expected[i]) > 1e-10)
            validation_fail("memory joint-state mismatch at basis %zu", i);
}

static void states(validation *v) {
    for (unsigned move = 0; move < 2; ++move)
        for (unsigned profile = 0; profile < 3; ++profile)
            for (unsigned level = 0; level < 5; ++level)
                for (unsigned builtins = 0; builtins < 2; ++builtins) {
                    char program[4096];
                    const char *input = profile == 1 ? "int8_t" : "uint8_t";
                    const char *output = profile == 0 ? "int8_t" : "uint8_t";
                    const char *condition = profile == 0 ? "if(c)" : profile == 1 ? "if(!c)" : "";
                    const char *preparation = profile == 1 ? "0" : "superposition(0,255)";
                    validation_path(
                        program,
                        sizeof program,
                        "#include <quantum.h>\n#include <string.h>\n"
                        "void copy_word(quant %s *d,quant const %s *s){"
                        "(void)%s((void*)d,(const void*)s,sizeof(*d));}\n"
                        "int main(void){quant %s a=%s;quant %s b=%d;quant bool c=superposition(0,1);"
                        "%sif(a==3)phase(.37);%scopy_word(&b,&a);"
                        "measure(a);measure(b);measure(c);return 0;}\n",
                        output,
                        input,
                        move ? "memmove" : "memcpy",
                        input,
                        preparation,
                        output,
                        profile == 0   ? -37
                        : profile == 1 ? 129
                                       : 0,
                        profile == 1 ? "fourier(a);" : "",
                        condition);
                    char *ir = compile_case(v, program, level, builtins, 0, 1);
                    for (unsigned tables = 0; tables < 2; ++tables) {
                        qc_context *c = qc_create(1, 7);
                        if (!c)
                            validation_fail("memory state allocation failed");
                        qc_allow_tables(c, tables);
                        if (!qc_document(c, ir, 0, 1))
                            validation_fail("memory state lowering failed: %s", qc_error(c));
                        qc_register *a = qc_lookup(c, 0), *b = qc_lookup(c, 1),
                                    *control = qc_lookup(c, 2);
                        if (!a || !b || !control || c->reset || c->measure != 17)
                            validation_fail(
                                "memory copy introduced an unexpected destructive channel: %llu measured bits, %llu resets",
                                (unsigned long long)c->measure,
                                (unsigned long long)c->reset);
                        double complex *expected = calloc(qc_state_size(c), sizeof *expected);
                        if (!expected)
                            validation_fail("memory reference allocation failed");
                        for (unsigned x = 0; x < 256; ++x)
                            for (unsigned bit = 0; bit < 2; ++bit) {
                                unsigned copied = profile == 2 || bit == (profile == 0);
                                unsigned initial = profile == 0 ? 219 : profile == 1 ? 129 : 0;
                                size_t index = basis(a, x) | basis(b, copied ? x : initial) |
                                               basis(control, bit);
                                expected[index] = cexp(I * (x == 3 ? .37 : 0)) / sqrt(512.0);
                            }
                        compare(c, expected);
                        char inverse[1024];
                        validation_path(
                            inverse,
                            sizeof inverse,
                            "{\"op\":\"qxor_inplace\",\"id\":1,\"expr\":{\"op\":\"cast\","
                            "\"bits\":8,\"signed\":%s,\"args\":[{\"op\":\"reg\",\"id\":0,"
                            "\"bits\":8,\"signed\":%s}]}%s}",
                            profile == 0 ? "true" : "false",
                            profile == 1 ? "true" : "false",
                            profile == 0   ? ",\"controls\":[{\"id\":2,\"value\":1}]"
                            : profile == 1 ? ",\"controls\":[{\"id\":2,\"value\":0}]"
                                           : "");
                        if (!qc_apply(c, inverse, NULL))
                            validation_fail("memory inverse failed: %s", qc_error(c));
                        if (profile != 2) {
                            validation_path(inverse,
                                            sizeof inverse,
                                            "{\"op\":\"qxor_inplace\",\"id\":1,\"value\":%u,"
                                            "\"controls\":[{\"id\":2,\"value\":%u}]}",
                                            profile == 0 ? 219 : 129,
                                            profile == 0 ? 1 : 0);
                            if (!qc_apply(c, inverse, NULL))
                                validation_fail("memory basis-restoration inverse failed: %s",
                                                qc_error(c));
                        }
                        memset(expected, 0, qc_state_size(c) * sizeof *expected);
                        for (unsigned x = 0; x < 256; ++x)
                            for (unsigned bit = 0; bit < 2; ++bit)
                                expected[basis(a, x) |
                                         basis(b,
                                               profile == 0   ? 219
                                               : profile == 1 ? 129
                                                              : 0) |
                                         basis(control, bit)] =
                                    cexp(I * (x == 3 ? .37 : 0)) / sqrt(512.0);
                        compare(c, expected);
                        free(expected);
                        qc_destroy(c);
                        ++checks;
                    }
                    free(ir);
                }
    puts(
        "PASS memcpy/memmove complete phased states and inverse, signed layouts, open/closed controls, builtins and five GCC levels");
}

static void overlap(validation *v) {
    for (unsigned backward = 0; backward < 2; ++backward) {
        char program[2048];
        validation_path(
            program,
            sizeof program,
            "#include <quantum.h>\n#include <string.h>\nint main(void){quant uint8_t a[3]={%s};"
            "memmove((void*)%s,(const void*)%s,2);measure(a[0]);measure(a[1]);measure(a[2]);return 0;}\n",
            backward ? "superposition(0,3),17,42" : "42,17,superposition(0,3)",
            backward ? "(a+1)" : "a",
            backward ? "a" : "(a+1)");
        char *ir = compile_case(v, program, 2, 1, 0, 1);
        for (unsigned tables = 0; tables < 2; ++tables) {
            qc_context *c = qc_create(1, 7);
            cj_doc d = {0};
            if (!c || !cj_parse(&d, ir) || !qc_verify_document(c, &d))
                validation_fail("invalid overlapping memory IR");
            qc_allow_tables(c, tables);
            c->ignore_measurements = 1;
            int list = cj_get(&d, 0, "instructions"), phased = 0;
            for (int t = list + 1; t < d.tokens[list].next; t = d.tokens[t].next) {
                if (!phased && cj_equal(&d, cj_get(&d, t, "op"), "qxor_inplace")) {
                    qc_register *source = qc_lookup(c, backward ? 0 : 2);
                    if (!source || !qc_emit(c,
                                            QC_PHASE,
                                            0,
                                            QC_WIRE(source->wires[0]),
                                            QC_WIRE(source->wires[0]),
                                            .41))
                        validation_fail("overlap phase setup failed");
                    phased = 1;
                }
                uint64_t ignored;
                if (!qc_apply_token(c, &d, t, 0, 0, &ignored))
                    validation_fail("overlap synthesis failed: %s", qc_error(c));
            }
            qc_register *a = qc_lookup(c, 0), *b = qc_lookup(c, 1), *last = qc_lookup(c, 2);
            if (!phased || !a || !b || !last || qc_qubits(c) != 24 || c->reset || c->measure != 24)
                validation_fail("overlap copy changed its storage/channel contract");
            size_t indices[4];
            for (unsigned x = 0; x < 4; ++x) {
                uint8_t reference[3] = {42, 17, 42};
                reference[backward ? 0 : 2] = (uint8_t)x;
                memmove(reference + (backward ? 1 : 0), reference + (backward ? 0 : 1), 2);
                indices[x] =
                    basis(a, reference[0]) | basis(b, reference[1]) | basis(last, reference[2]);
            }
            for (size_t i = 0; i < qc_state_size(c); ++i) {
                double complex expected = 0;
                for (unsigned x = 0; x < 4; ++x)
                    if (i == indices[x])
                        expected = cexp(I * (x & 1 ? .41 : 0)) / 2;
                if (cabs(qc_amplitude(c, i) - expected) > 1e-10)
                    validation_fail("overlap read-before-write mismatch at %zu", i);
            }
            qc_destroy(c);
            cj_free(&d);
            ++checks;
        }
        free(ir);
    }
    puts(
        "PASS overlapping memmove in both directions against C, with retained phase and no ancillas");
}

static void contracts(validation *v) {
    const char *valid[] = {
        "quant uint8_t a=249;quant int8_t b=-17;memcpy((void*)&b,(const void*)&a,1);return measure(b)!=-7||measure(a)!=249;",
        "quant int8_t a=-7;quant uint8_t b=0;memmove((void*)&b,(const void*)&a,1);return measure(b)!=249||measure(a)!=-7;",
        "quant uint8_t a=7;memmove((void*)&a,(const void*)&a,1);return measure(a)!=7;",
        "quant uint8_t a[1]={7};memcpy((void*)(a+1),(const void*)(a+1),0);return measure(a[0])!=7;",
        "uint8_t a[3]={1,2,3},b[3]={0};void *p=memcpy(b,a,3);memmove(b+1,b,2);quant uint8_t q=7;return p!=b||b[1]!=1||b[2]!=2||measure(q)!=7;",
        "quant uint8_t a=7,b=0;quant uint8_t *p=&b;quant const uint8_t *s=&a;(void)memcpy((void*)p,(const void*)s,1);return measure(*p)!=7||measure(a)!=7;",
        "quant bool a=superposition(0,1);quant uint8_t source=a+1,destination=0;memcpy((void*)&destination,(const void*)&source,1);unsigned r=measure(destination),s=measure(a);return r!=s+1;",
        "#include <quantum.h>\nstatic void memcpy(quant uint8_t *d,quant const uint8_t *s,unsigned n){*d+=n;}\nint main(void){quant uint8_t a=7,b=0;memcpy(&b,&a,1);return measure(b)!=1||measure(a)!=7;}\n",
        "quant uint8_t a=7,b=0;for(int i=0;i<1;++i)memcpy((void*)&b,(const void*)&a,1);return measure(b)!=7||measure(a)!=7;"};
    const char *invalid[] = {
        "quant uint8_t a=7,b=superposition(0,3);memcpy((void*)&b,(const void*)&a,1);measure(a);measure(b);",
        "quant uint8_t a=7,b=superposition(0,1);memmove((void*)&b,(const void*)&a,1);measure(a);measure(b);",
        "quant uint8_t a=7;memcpy((void*)&a,(const void*)&a,1);measure(a);",
        "quant uint8_t a[3]={7,0,0};memcpy((void*)(a+1),(const void*)a,2);measure(a[0]);measure(a[1]);measure(a[2]);",
        "quant uint8_t a=7,b=0;size_t n=1;memcpy((void*)&b,(const void*)&a,n);measure(a);measure(b);",
        "quant uint16_t a=7,b=0;memcpy((void*)&b,(const void*)&a,1);measure(a);measure(b);",
        "quant uint8_t a=7;quant uint16_t b=0;memcpy((void*)&b,(const void*)&a,1);measure(a);measure(b);",
        "quant uint8_t a=7,b=0;memcpy((void*)&b,(const void*)&a,2);measure(a);measure(b);",
        "quant uint8_t a=7,b=0;void *p=memcpy((void*)&b,(const void*)&a,1);measure(a);measure(b);return p!=0;",
        "quant uint8_t a=7,b=0;quant uint8_t *p=memcpy((void*)&b,(const void*)&a,1);measure(a);measure(b);measure(*p);",
        "quant uint8_t a=7;uint8_t b=0;memcpy(&b,(const void*)&a,1);measure(a);",
        "uint8_t a=7;quant uint8_t b=0;memcpy((void*)&b,&a,1);measure(b);",
        "quant bool a=true,b=false;memcpy((void*)&b,(const void*)&a,1);measure(a);measure(b);",
        "quant uint8_t a=7,b=0;quant const uint8_t *p=&b;memcpy((void*)p,(const void*)&a,1);measure(a);measure(b);",
        "quant uint8_t a=7,b=0;quant uint8_t child=b+1;memcpy((void*)&b,(const void*)&a,1);measure(child);measure(a);measure(b);",
        "quant uint8_t a=7,b=0;quant uint8_t child=a-a;memcpy((void*)&child,(const void*)&b,1);measure(child);measure(a);measure(b);",
        "quant uint8_t a=7,b=0;int n=2;while(n--)memcpy((void*)&b,(const void*)&a,1);measure(a);measure(b);"};
    for (unsigned level = 0; level < 5; ++level)
        for (unsigned builtins = 0; builtins < 2; ++builtins)
            for (unsigned expected = 0; expected < 2; ++expected) {
                const char **cases = expected ? valid : invalid;
                size_t count =
                    expected ? sizeof valid / sizeof *valid : sizeof invalid / sizeof *invalid;
                for (size_t i = 0; i < count; ++i) {
                    char program[4096];
                    if (*cases[i] == '#')
                        validation_path(program, sizeof program, "%s", cases[i]);
                    else
                        validation_path(
                            program,
                            sizeof program,
                            "#include <quantum.h>\n#include <string.h>\nint main(void){%s}\n",
                            cases[i]);
                    free(compile_case(v, program, level, builtins, expected, expected));
                    ++checks;
                }
            }
    puts(
        "PASS memory bounds, known-basis destination, const, overlap, padding, CPU boundary, return-pointer diagnostics and classical libc preservation");
}

static void wide(validation *v) {
    const char *types[] = {"uint16_t", "uint32_t", "uint64_t"};
    const char *upper[] = {"UINT16_MAX", "UINT32_MAX", "UINT64_MAX"};
    for (unsigned move = 0; move < 2; ++move)
        for (unsigned type = 0; type < 3; ++type)
            for (unsigned level = 0; level < 5; ++level) {
                char program[2048];
                validation_path(
                    program,
                    sizeof program,
                    "#include <quantum.h>\n#include <string.h>\nint main(void){quant %s a=superposition(0,%s),b=0;"
                    "%s((void*)&b,(const void*)&a,sizeof(a));measure(a);measure(b);return 0;}\n",
                    types[type],
                    upper[type],
                    move ? "memmove" : "memcpy");
                char *ir = compile_case(v, program, level, 1, 0, 1);
                qc_context *c = qc_create(0, 7);
                if (!c)
                    validation_fail("wide memory context allocation failed");
                qc_allow_tables(c, 0);
                if (!qc_document(c, ir, 0, 1) || qc_qubits(c) != 2u * (16u << type) || c->reset ||
                    c->measure != 2u * (16u << type) || c->ngates > 4u * (16u << type))
                    validation_fail("wide table-free copy violated its resource contract: %s",
                                    qc_error(c));
                qc_destroy(c);
                free(ir);
                ++checks;
            }
    puts("PASS 16/32/64-bit linear table-free memory synthesis (generation only)");
}

int main(int argc, char **argv) {
    if (argc != 1)
        validation_fail("usage: validate_memcopy");
    validation v;
    validation_init(&v, argv[0]);
    states(&v);
    overlap(&v);
    contracts(&v);
    wide(&v);
    validation_cleanup(&v);
    printf(
        "%u memory-copy checks passed; basis-value transfer is distinct from state cloning and wide generation.\n",
        checks);
    return 0;
}
