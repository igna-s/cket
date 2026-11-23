#include "internal.h"
#include "validation.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
static unsigned compiled, native, states, independent, inverses, rejected;

static void reference_gate(double complex *state, size_t dimension, qc_gate gate, int inverse) {
    if (gate.kind == QC_PHASE) {
        double complex factor = cexp(I * (inverse ? -gate.angle : gate.angle));
        for (size_t i = 0; i < dimension; ++i)
            if (qc_mask_eq(qc_mask_and(qc_mask_from_u64(i), gate.mask), gate.pattern))
                state[i] *= factor;
        return;
    }
    if (gate.kind != QC_H && gate.kind != QC_X)
        validation_fail("readonly borrow reference encountered a nonunitary or unknown gate");
    size_t bit = (size_t)1 << gate.target;
    for (size_t i = 0; i < dimension; ++i) {
        if ((i & bit) || !qc_mask_eq(qc_mask_and(qc_mask_from_u64(i), gate.mask), gate.pattern))
            continue;
        double complex a = state[i], b = state[i | bit];
        state[i] = gate.kind == QC_X ? b : (a + b) / sqrt(2.0);
        state[i | bit] = gate.kind == QC_X ? a : (a - b) / sqrt(2.0);
    }
}

static void check_state(const char *ir, unsigned profile, unsigned tables) {
    qc_context *c = qc_create(1, 7);
    if (!c)
        validation_fail("readonly borrow context allocation failed");
    c->recording = 1;
    qc_allow_tables(c, tables);
    if (!qc_document(c, ir, 0, 1) || c->reset || c->measure != 2 || c->physical > 12)
        validation_fail("readonly borrow execution failed: %s", qc_error(c));
    cj_doc d = {0};
    if (!cj_parse(&d, ir))
        validation_fail("readonly borrow IR parse failed");
    unsigned wires[2], count = 0;
    int list = cj_get(&d, 0, "instructions");
    for (int t = list + 1; t < d.tokens[list].next; t = d.tokens[t].next) {
        if (!cj_equal(&d, cj_get(&d, t, "op"), "qmeasure"))
            continue;
        uint64_t id;
        if (count == 2 || !cj_u64(&d, cj_get(&d, t, "id"), &id))
            validation_fail("readonly borrow public measurement mapping failed");
        qc_register *reg = qc_lookup(c, (uint32_t)id);
        if (!reg || !reg->alive || reg->width != 1)
            validation_fail("readonly borrow lost a public Boolean owner");
        wires[count++] = reg->wires[0];
    }
    if (count != 2 || wires[0] == wires[1])
        validation_fail("readonly borrow expected two distinct public inputs");
    size_t dimension = qc_state_size(c),
           public_mask = ((size_t)1 << wires[0]) | ((size_t)1 << wires[1]);
    double complex *expected = calloc(dimension, sizeof *expected);
    double complex *evolved = calloc(dimension, sizeof *evolved);
    if (!expected || !evolved)
        validation_fail("readonly borrow reference allocation failed");
    for (size_t i = 0; i < dimension; ++i) {
        unsigned x = (unsigned)(i >> wires[0]) & 1, y = (unsigned)(i >> wires[1]) & 1;
        unsigned selected = profile == 2 || profile == 3 ? y : profile == 5 ? x && y : x;
        if (!(i & ~public_mask))
            expected[i] = .5 * cexp(I * (.19 * x - .23 * y + .31 * selected));
        if (cabs(qc_amplitude(c, i) - expected[i]) > 1e-10)
            validation_fail(
                "readonly borrow C phase/private-zero mismatch profile=%u basis=%zu", profile, i);
    }
    ++states;
    size_t unitary = c->ngates;
    while (unitary && c->gates[unitary - 1].kind == QC_MEASURE)
        --unitary;
    if (c->ngates - unitary != 2)
        validation_fail("readonly borrow expected only the two explicit final observations");
    evolved[0] = 1;
    for (size_t i = 0; i < unitary; ++i)
        reference_gate(evolved, dimension, c->gates[i], 0);
    for (size_t i = 0; i < dimension; ++i)
        if (cabs(evolved[i] - expected[i]) > 1e-10)
            validation_fail("readonly borrow independent gate/C state mismatch");
    ++independent;
    for (size_t i = unitary; i > 0; --i)
        reference_gate(expected, dimension, c->gates[i - 1], 1);
    for (size_t i = 0; i < dimension; ++i)
        if (cabs(expected[i] - (i == 0 ? 1 : 0)) > 1e-10)
            validation_fail("readonly borrow independent inverse did not restore zero");
    ++inverses;
    free(expected);
    free(evolved);
    cj_free(&d);
    qc_destroy(c);
}

static void positive(validation *v) {
    const struct {
        const char *definition, *call;
    } cases[] = {
        {"static void mark(quant const bool a,quant const bool b){if(a&&b)phase(.31);}",
         "mark(x[0],x[0]);"},
        {"static void mark(quant const bool *a,quant const bool *b){if(a[0]&&b[0])phase(.31);}",
         "mark(x,x);"},
        {"static void mark(quant const bool *a,quant const bool *b){if(a[1]&&b[0])phase(.31);}",
         "mark(x,x+1);"},
        {"static void mark(quant const bool a,quant const bool *b){if(a&&b[0])phase(.31);}",
         "mark(x[1],x+1);"},
        {"static void inner(quant const bool a,quant const bool b){if(a&&b)phase(.31);}"
         "static void mark(quant const bool *a,quant const bool *b){inner(a[0],b[0]);}",
         "mark(x,x);"},
        {"static void mark(quant const bool *a,quant const bool b,quant const bool *c){"
         "if(a[0]&&b&&c[1])phase(.31);}",
         "mark(x,x[0],x);"},
    };

    char source[4096], compiler[4096], output[4096], executable[4096], program[8192];
    validation_path(source, sizeof source, "%s/readonly.c", v->temp);
    validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
    validation_path(output, sizeof output, "%s/readonly.json", v->temp);
    validation_path(executable, sizeof executable, "%s/readonly", v->temp);
    for (unsigned profile = 0; profile < sizeof cases / sizeof *cases; ++profile) {
        validation_path(program,
                        sizeof program,
                        "#include <quantum.h>\n%s\nint main(void){"
                        "quant bool x[2]={superposition(0,1),superposition(0,1)};"
                        "if(x[0])phase(.19);if(x[1])phase(-.23);%s"
                        "measure(x[0]);measure(x[1]);return 0;}\n",
                        cases[profile].definition,
                        cases[profile].call);
        validation_write(source, program);
        for (unsigned level = 0; level < 5; ++level) {
            char *command[] = {
                compiler, source, "--emit-ir", (char *)levels[level], "-o", output, NULL};
            if (validation_run(v, command, NULL, 60)) {
                char *log = validation_read(v->log);
                validation_fail(
                    "readonly borrow accepted case failed profile=%u: %s", profile, log);
            }
            ++compiled;
            char *ir = validation_read(output);
            for (unsigned tables = 0; tables < 2; ++tables)
                check_state(ir, profile, tables);
            free(ir);
            char *run[] = {
                compiler, source, "--run", (char *)levels[level], "-o", executable, NULL};
            if (validation_run(v, run, NULL, 60)) {
                char *log = validation_read(v->log);
                validation_fail("readonly borrow native execution failed: %s", log);
            }
            ++native;
        }
    }
}

static void negative(validation *v) {
    const struct {
        const char *definition, *call, *diagnostic;
    } cases[] = {
        {"static void bad(quant bool a,quant bool b){a=!b;}",
         "bad(x[0],x[0]);",
         "mutable quantum parameters cannot alias"},
        {"static void bad(quant const bool a,quant bool b){b=!a;}",
         "bad(x[0],x[0]);",
         "mutable quantum parameters cannot alias"},
        {"static void bad(quant bool a,quant const bool b){a=!b;}",
         "bad(x[0],x[0]);",
         "mutable quantum parameters cannot alias"},
        {"static void bad(quant bool *a,quant bool *b){a[0]=!b[0];}",
         "bad(x,x);",
         "mutable quantum pointer parameters cannot overlap"},
        {"static void bad(quant const bool *a,quant bool *b){b[0]=!a[0];}",
         "bad(x,x);",
         "mutable quantum pointer parameters cannot overlap"},
        {"static void bad(quant bool *a,quant const bool *b){a[0]=!b[0];}",
         "bad(x,x);",
         "mutable quantum pointer parameters cannot overlap"},
        {"static void bad(quant const bool *a,quant bool b){b=!a[0];}",
         "bad(x,x[0]);",
         "mutable quantum parameters cannot alias"},
        {"static void bad(quant bool a,quant const bool *b){a=!b[0];}",
         "bad(x[0],x);",
         "mutable quantum pointer parameters cannot overlap"},
        {"static void inner(quant bool a){a=!a;}static void bad(quant const bool *a,quant const bool *b){inner(a[0]);}",
         "bad(x,x);",
         "read-only quantum borrow cannot be passed to a mutable parameter"},
        {"static void bad(quant const bool a,quant const bool b){quant bool stolen=a;measure(stolen);}",
         "bad(x[0],x[0]);",
         "a borrowed parameter cannot be consumed by an ownership move"},
        {"static void bad(quant const bool *a,quant const bool *b){((bool*)a)[0]=true;}",
         "bad(x,x);",
         "assignment cannot modify a read-only quantum borrow"},
        {"static void bad(quant const bool *a,quant const bool *b){((bool*)a)[0]++;}",
         "bad(x,x);",
         "assignment cannot modify a read-only quantum borrow"},
        {"static void bad(quant const bool a,quant const bool b){*(bool*)&a=true;}",
         "bad(x[0],x[0]);",
         "assignment cannot modify a read-only quantum borrow"},
    };

    char source[4096], compiler[4096], output[4096], program[8192];
    validation_path(source, sizeof source, "%s/rejected.c", v->temp);
    validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
    validation_path(output, sizeof output, "%s/rejected.json", v->temp);
    for (unsigned test = 0; test < sizeof cases / sizeof *cases; ++test) {
        validation_path(
            program,
            sizeof program,
            "#include <quantum.h>\n%s\nint main(void){quant bool x[2]={false,true};%s measure(x[0]);measure(x[1]);return 0;}\n",
            cases[test].definition,
            cases[test].call);
        validation_write(source, program);
        for (unsigned level = 0; level < 5; ++level) {
            char *command[] = {
                compiler, source, "--emit-ir", (char *)levels[level], "-o", output, NULL};
            int status = validation_run(v, command, NULL, 60);
            char *log = validation_read(v->log);
            if (!validation_rejected(status, log, cases[test].diagnostic))
                validation_fail("readonly borrow rejection failed test=%u: %s", test, log);
            free(log);
            ++rejected;
        }
    }
}

static void ir_readonly_enforcement(void) {
    static const char *const bad_ops[] = {
        "{\"op\":\"qcompute\",\"id\":0,\"expr\":{\"op\":\"const\",\"bits\":8,\"signed\":false,\"value\":1}}",
        "{\"op\":\"quncompute\",\"id\":0,\"expr\":{\"op\":\"const\",\"bits\":8,\"signed\":false,\"value\":1}}",
        "{\"op\":\"qinplace\",\"id\":0,\"expr\":{\"op\":\"bxor\",\"bits\":8,\"signed\":false,\"args\":[{\"op\":\"reg\",\"id\":0,\"bits\":8,\"signed\":false},{\"op\":\"const\",\"bits\":8,\"signed\":false,\"value\":1}]}}",
        "{\"op\":\"qadd_inplace\",\"id\":0,\"value\":1}",
        "{\"op\":\"qadd_register\",\"id\":0,\"source\":1,\"subtract\":false}",
        "{\"op\":\"qadd_expression\",\"id\":0,\"subtract\":false,\"expr\":{\"op\":\"const\",\"bits\":8,\"signed\":false,\"value\":1},\"checked\":{\"op\":\"const\",\"bits\":8,\"signed\":false,\"value\":1}}",
        "{\"op\":\"qxor_inplace\",\"id\":0,\"value\":1}",
        "{\"op\":\"qfourier\",\"id\":0,\"inverse\":false}",
        "{\"op\":\"qmeasure\",\"id\":0}",
        "{\"op\":\"qreset\",\"id\":0}",
        "{\"op\":\"qswap\",\"id\":0,\"other\":1,\"rebind\":false}",
        "{\"op\":\"qpatch\",\"id\":0,\"history\":1,\"bit_mask\":{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":1},\"expr\":{\"op\":\"const\",\"bits\":8,\"signed\":false,\"value\":1}}",
        "{\"op\":\"qmodule\",\"ids\":[0],\"inverse\":false,\"circuit\":{\"format\":\"cket.qir\",\"version\":1,\"instructions\":[{\"op\":\"qborrow\",\"id\":0,\"width\":8,\"signed\":false},{\"op\":\"qadd_inplace\",\"id\":0,\"value\":1}]}}",
    };
    unsigned checks = 0;
    unsigned nbad = (unsigned)(sizeof bad_ops / sizeof *bad_ops);
    for (unsigned i = 0; i < nbad; ++i) {
        qc_context *c = qc_create(0, 7);
        if (!c ||
            !qc_apply(
                c,
                "{\"op\":\"qborrow\",\"id\":0,\"width\":8,\"signed\":false,\"readonly\":true}",
                NULL) ||
            !qc_apply(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":8,\"signed\":false}", NULL) ||
            (i == 10 &&
             !qc_apply(c, "{\"op\":\"qprepare\",\"id\":1,\"lower\":0,\"upper\":0}", NULL)))
            validation_fail("readonly IR test setup failed: %s", qc_error(c));
        if (qc_apply(c, bad_ops[i], NULL) || !strstr(qc_error(c), "readonly"))
            validation_fail("readonly borrow mutation accepted at case %u: %s", i, qc_error(c));
        qc_destroy(c);
        ++checks;
    }
    /* Read-only source, control, non-mutating module, and omitted readonly remain allowed. */
    qc_context *c = qc_create_sparse(7, 256);
    if (!c ||
        !qc_apply(c,
                  "{\"op\":\"qborrow\",\"id\":0,\"width\":1,\"signed\":false,\"readonly\":true}",
                  NULL) ||
        !qc_apply(c,
                  "{\"op\":\"qborrow\",\"id\":1,\"width\":8,\"signed\":false,\"readonly\":true}",
                  NULL) ||
        !qc_apply(c, "{\"op\":\"qalloc\",\"id\":2,\"width\":8,\"signed\":false}", NULL) ||
        !qc_apply(c, "{\"op\":\"qalloc\",\"id\":3,\"width\":8,\"signed\":false}", NULL) ||
        !qc_apply(
            c,
            "{\"op\":\"qcompute\",\"id\":2,\"controls\":[{\"id\":0,\"value\":1}],\"expr\":{\"op\":\"reg\",\"id\":1,\"bits\":8,\"signed\":false}}",
            NULL) ||
        !qc_apply(
            c,
            "{\"op\":\"qadd_register\",\"id\":3,\"source\":1,\"subtract\":false,\"controls\":[{\"id\":0,\"value\":1}]}",
            NULL) ||
        !qc_apply(
            c, "{\"op\":\"qphase\",\"angle\":0.5,\"controls\":[{\"id\":0,\"value\":1}]}", NULL) ||
        !qc_apply(
            c,
            "{\"op\":\"qmodule\",\"ids\":[0],\"inverse\":false,\"circuit\":{\"format\":\"cket.qir\",\"version\":1,\"instructions\":[{\"op\":\"qborrow\",\"id\":0,\"width\":1,\"signed\":false,\"readonly\":true},{\"op\":\"qphase\",\"angle\":0.25,\"controls\":[{\"id\":0,\"value\":1}]}]}}",
            NULL))
        validation_fail("readonly borrow valid read/control failed: %s", qc_error(c));
    qc_destroy(c);
    ++checks;
    /* Legacy qborrow without readonly remains mutable; qalloc rejects readonly. */
    c = qc_create(0, 7);
    if (!c || !qc_apply(c, "{\"op\":\"qborrow\",\"id\":0,\"width\":8,\"signed\":false}", NULL) ||
        !qc_apply(c, "{\"op\":\"qadd_inplace\",\"id\":0,\"value\":1}", NULL))
        validation_fail("omitted readonly backwards compatibility failed: %s", qc_error(c));
    if (qc_apply(c,
                 "{\"op\":\"qalloc\",\"id\":1,\"width\":8,\"signed\":false,\"readonly\":true}",
                 NULL) ||
        !*qc_error(c))
        validation_fail("qalloc should reject readonly field");
    qc_destroy(c);
    checks += 2;
    printf("PASS %u IR readonly borrow enforcement, read/control and compatibility checks\n",
           checks);
}

int main(int argc, char **argv) {
    if (argc != 1)
        validation_fail("usage: validate_readonly_borrows");
    validation v;
    validation_init(&v, argv[0]);
    positive(&v);
    negative(&v);
    ir_readonly_enforcement();
    validation_cleanup(&v);
    if (compiled != 30 || native != 30 || states != 60 || independent != 60 || inverses != 60 ||
        rejected != 65)
        validation_fail("readonly borrow coverage count mismatch");
    printf(
        "305 readonly borrow checks passed; 30 IR compilations, 30 native executions, 60 C states, 60 independent gate states, 60 complete inverses and 65 mutation/ownership rejections.\n");
    return 0;
}
