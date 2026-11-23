#include "internal.h"
#include "validation.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
static const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};

static void apply(qc_context *c, const char *text) {
    if (!qc_apply(c, text, NULL))
        validation_fail("binding fixture failed: %s", qc_error(c));
}

static void states(void) {
    const char *ops[] = {"add", "mul", "div", "mod", "shl", "shr", "bxor", "gt"};
    for (unsigned op = 0; op < 8; ++op)
        for (unsigned value = 1; value <= 3; ++value)
            for (unsigned control = 0; control < 2; ++control)
                for (unsigned tables = 0; tables < 2; ++tables) {
                    qc_context *c = qc_create(1, 7);
                    if (!c)
                        validation_fail("binding allocation failed");
                    qc_allow_tables(c, tables);
                    apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":3,\"signed\":false}");
                    apply(c, "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":7}");
                    apply(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":4,\"signed\":false}");
                    apply(c, "{\"op\":\"qalloc\",\"id\":2,\"width\":1,\"signed\":false}");
                    apply(c, "{\"op\":\"qprepare\",\"id\":2,\"lower\":0,\"upper\":1}");
                    if (!qc_emit(c, QC_PHASE, 0, QC_WIRE(0), QC_WIRE(0), .31) ||
                        !qc_emit(c, QC_PHASE, 0, QC_WIRE(2), QC_WIRE(2), .47))
                        validation_fail("binding phase preparation failed");
                    char instruction[3072];
                    validation_path(
                        instruction,
                        sizeof instruction,
                        "{\"op\":\"qbind\",\"parameters\":[{\"id\":17,\"bits\":16,\"signed\":false}],"
                        "\"instruction\":{\"op\":\"qcompute\",\"id\":1,\"controls\":[{\"id\":2,\"value\":%u}],"
                        "\"expr\":{\"op\":\"cast\",\"bits\":4,\"signed\":false,\"args\":[{\"op\":\"%s\",\"bits\":%u,\"signed\":false,\"args\":["
                        "{\"op\":\"cast\",\"bits\":16,\"signed\":false,\"args\":[{\"op\":\"reg\",\"id\":0,\"bits\":3,\"signed\":false}]},"
                        "{\"op\":\"parameter\",\"id\":17,\"bits\":16,\"signed\":false}]}]}}}",
                        control,
                        ops[op],
                        op == 7 ? 1 : 16);
                    uint64_t binding = value;
                    if (!qc_apply_bound(c, instruction, &binding, 1, NULL))
                        validation_fail("bound arithmetic failed: %s", qc_error(c));
                    for (unsigned inverse = 0; inverse < 2; ++inverse) {
                        for (size_t i = 0; i < qc_state_size(c); ++i) {
                            unsigned x = (unsigned)i & 7, flag = (unsigned)(i >> 7) & 1, reference;
                            switch (op) {
                            case 0:
                                reference = x + value;
                                break;
                            case 1:
                                reference = x * value;
                                break;
                            case 2:
                                reference = x / value;
                                break;
                            case 3:
                                reference = x % value;
                                break;
                            case 4:
                                reference = x << value;
                                break;
                            case 5:
                                reference = x >> value;
                                break;
                            case 6:
                                reference = x ^ value;
                                break;
                            default:
                                reference = x > value;
                                break;
                            }
                            unsigned result = !inverse && flag == control ? reference & 15 : 0;
                            double complex expected = 0;
                            if (((i >> 3) & 15) == result && !(i >> 8))
                                expected = cexp(I * ((x & 1 ? .31 : 0) + (x & 4 ? .47 : 0))) / 4;
                            if (cabs(qc_amplitude(c, i) - expected) > 1e-10)
                                validation_fail(
                                    "binding state op=%s value=%u control=%u tables=%u inverse=%u basis=%zu",
                                    ops[op],
                                    value,
                                    control,
                                    tables,
                                    inverse,
                                    i);
                        }
                        if (!inverse) {
                            ++binding;
                            validation_path(
                                instruction,
                                sizeof instruction,
                                "{\"op\":\"quncompute\",\"id\":1,\"saved\":true,\"controls\":[{\"id\":2,\"value\":%u}]}",
                                control);
                            apply(c, instruction);
                        }
                    }
                    if (c->measure || c->reset)
                        validation_fail("implicit binding channel");
                    qc_destroy(c);
                    ++checks;
                }
    puts(
        "PASS 96 bound arithmetic domains: full phases, controls, table policies and saved inverse");
    fflush(stdout);
}

static void endpoints(void) {
    const uint64_t values[] = {0, 1, UINT64_MAX, UINT64_C(1) << 63};
    for (unsigned bits = 1; bits <= 64; bits *= 2)
        for (unsigned sign = 0; sign <= (bits != 1); ++sign)
            for (unsigned v = 0; v < 4; ++v) {
                qc_context *c = qc_create(1, 7);
                if (!c)
                    validation_fail("endpoint allocation failed");
                apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":8,\"signed\":false}");
                char instruction[1024];
                validation_path(
                    instruction,
                    sizeof instruction,
                    "{\"op\":\"qbind\",\"parameters\":[{\"id\":0,\"bits\":%u,\"signed\":%s}],"
                    "\"instruction\":{\"op\":\"qcompute\",\"id\":0,\"expr\":{\"op\":\"cast\",\"bits\":8,\"signed\":false,"
                    "\"args\":[{\"op\":\"parameter\",\"id\":0,\"bits\":%u,\"signed\":%s}]}}}",
                    bits,
                    sign ? "true" : "false",
                    bits,
                    sign ? "true" : "false");
                if (!qc_apply_bound(c, instruction, &values[v], 1, NULL))
                    validation_fail("bound endpoint failed: %s", qc_error(c));
                uint64_t mask = bits == 64 ? UINT64_MAX : (UINT64_C(1) << bits) - 1;
                uint64_t raw = bits == 1 ? values[v] != 0 : values[v] & mask;
                if (sign && (raw & (UINT64_C(1) << (bits - 1))))
                    raw |= ~mask;
                unsigned expected = (unsigned)raw & 255;
                for (size_t i = 0; i < qc_state_size(c); ++i)
                    if (cabs(qc_amplitude(c, i) - (i == expected ? 1 : 0)) > 1e-10)
                        validation_fail("CPU binding integer representation mismatch");
                qc_destroy(c);
                ++checks;
            }
    puts("PASS 52 typed binding endpoints including signed minimum and unsigned maximum");
}

static void rejected(void) {
    const char *descriptors[] = {
        "[{\"id\":0,\"bits\":8,\"signed\":true}]",
        "[{\"id\":1,\"bits\":8,\"signed\":false}]",
        "[{\"id\":0,\"bits\":8,\"signed\":false},{\"id\":0,\"bits\":8,\"signed\":false}]",
        "[{\"id\":0,\"bits\":0,\"signed\":false}]",
        "[{\"id\":0,\"bits\":65,\"signed\":false}]",
        "[{\"id\":0,\"bits\":1,\"signed\":true}]",
        "[{\"id\":-1,\"bits\":8,\"signed\":false}]",
        "[{\"id\":0,\"id\":0,\"bits\":8,\"signed\":false}]",
        "[{\"id\":0,\"bits\":8,\"signed\":false,\"extra\":1}]",
        "[]"};
    for (unsigned i = 0; i < sizeof descriptors / sizeof *descriptors; ++i) {
        qc_context *c = qc_create(1, 7);
        if (!c)
            validation_fail("negative binding allocation failed");
        apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":8,\"signed\":false}");
        char instruction[1200];
        validation_path(
            instruction,
            sizeof instruction,
            "{\"op\":\"qbind\",\"parameters\":%s,\"instruction\":{\"op\":\"qcompute\",\"id\":0,"
            "\"expr\":{\"op\":\"parameter\",\"id\":0,\"bits\":8,\"signed\":false}}}",
            descriptors[i]);
        uint64_t values[2] = {7, 8};
        if (qc_apply_bound(c, instruction, values, i == 2 ? 2 : 1, NULL) || c->ngates ||
            cabs(qc_amplitude(c, 0) - 1) > 1e-10)
            validation_fail("invalid CPU binding changed quantum state");
        qc_destroy(c);
        ++checks;
    }
    puts("PASS 10 malformed CPU descriptor cases reject before state changes");
}

static void compile_run(
    validation *v, const char *body, unsigned level, unsigned builtins, const char *diagnostic) {
    char source[4096], executable[4096], compiler[4096], program[8192];
    validation_path(source, sizeof source, "%s/bindings.c", v->temp);
    validation_path(executable, sizeof executable, "%s/bindings", v->temp);
    validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
    if (*body == '#')
        validation_write(source, body);
    else {
        validation_path(
            program,
            sizeof program,
            "#include <quantum.h>\n#include <stdlib.h>\n#include <inttypes.h>\nint main(void){%s}\n",
            body);
        validation_write(source, program);
    }
    char *command[] = {compiler,
                       source,
                       "--run",
                       (char *)levels[level],
                       builtins ? "-fbuiltin" : "-fno-builtin",
                       "-o",
                       executable,
                       NULL};
    int status = validation_run(v, command, NULL, 60);
    char *log = validation_read(v->log);
    if (diagnostic ? !validation_rejected(status, log, diagnostic) : status != 0)
        validation_fail(
            "binding C case failed %s builtins=%u: %s\n%s", levels[level], builtins, body, log);
    free(log);
    ++checks;
}

static void document_contracts(void) {
    const char *ir =
        "{\"format\":\"cket.qir\",\"version\":1,\"instructions\":["
        "{\"op\":\"qalloc\",\"id\":0,\"width\":8,\"signed\":false},"
        "{\"op\":\"qbind\",\"parameters\":[{\"id\":3,\"bits\":8,\"signed\":false}],"
        "\"instruction\":{\"op\":\"qprepare\",\"id\":0,\"expr\":{\"op\":\"parameter\",\"id\":3,\"bits\":8,\"signed\":false}}},"
        "{\"op\":\"qmeasure\",\"id\":0}]}";
    const char *bad[] = {"[]",
                         "[{\"instruction\":2,\"values\":[7]}]",
                         "[{\"instruction\":-1,\"values\":[7]}]",
                         "[{\"instruction\":1,\"values\":[]}]",
                         "[{\"instruction\":1,\"values\":[7,8]}]",
                         "[{\"instruction\":1,\"values\":[true]}]",
                         "[{\"instruction\":1,\"values\":[\"7\"]}]",
                         "[{\"instruction\":1,\"values\":[7],\"extra\":0}]",
                         "[{\"instruction\":1}]",
                         "[{\"instruction\":1,\"values\":[7]},{\"instruction\":1,\"values\":[7]}]"};
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; ++i) {
        qc_context *c = qc_create(1, 7);
        if (!c)
            validation_fail("document contract allocation failed");
        char *bound = qc_bind_document(c, ir, bad[i]);
        if (bound || qc_qubits(c) || c->ngates)
            validation_fail("invalid binding rows changed circuit state");
        free(bound);
        qc_destroy(c);
        ++checks;
    }
    qc_context *c = qc_create(1, 7);
    char *bound = c ? qc_bind_document(c, ir, "[{\"instruction\":1,\"values\":[-7]}]") : NULL;
    if (!bound || !qc_document(c, bound, 0, 1) || c->reset || c->measure != 8)
        validation_fail("bound classical basis preparation failed: %s", qc_error(c));
    for (size_t i = 0; i < qc_state_size(c); ++i)
        if (cabs(qc_amplitude(c, i) - (i == 249 ? 1 : 0)) > 1e-10)
            validation_fail("bound preparation did not preserve the converted C value");
    free(bound);
    qc_destroy(c);
    ++checks;
    puts("PASS 11 whole-document binding/preparation contracts before circuit mutation");
}

static void shifted_search_and_structural(void) {
    /* Verify structural and non-structural qprepare metadata match across shifted
     * aligned, unaligned, negative and zero-crossing intervals, and invalid qfind rejects. */
    const char *prep_docs[] = {"{\"format\":\"cket.qir\",\"version\":1,\"instructions\":["
                               "{\"op\":\"qalloc\",\"id\":0,\"width\":4,\"signed\":false},"
                               "{\"op\":\"qprepare\",\"id\":0,\"lower\":4,\"upper\":7}]}",
                               "{\"format\":\"cket.qir\",\"version\":1,\"instructions\":["
                               "{\"op\":\"qalloc\",\"id\":0,\"width\":4,\"signed\":false},"
                               "{\"op\":\"qprepare\",\"id\":0,\"lower\":3,\"upper\":6}]}",
                               "{\"format\":\"cket.qir\",\"version\":1,\"instructions\":["
                               "{\"op\":\"qalloc\",\"id\":0,\"width\":4,\"signed\":true},"
                               "{\"op\":\"qprepare\",\"id\":0,\"lower\":-6,\"upper\":-3}]}",
                               "{\"format\":\"cket.qir\",\"version\":1,\"instructions\":["
                               "{\"op\":\"qalloc\",\"id\":0,\"width\":4,\"signed\":true},"
                               "{\"op\":\"qprepare\",\"id\":0,\"lower\":-4,\"upper\":3}]}"};
    for (unsigned p = 0; p < 4; ++p) {
        qc_context *st = qc_create(0, 7), *ns = qc_create(1, 7);
        if (!st || !ns)
            validation_fail("structural comparison allocation failed");
        st->validation = st->structural = 1;
        if (!qc_document(st, prep_docs[p], 1, 1) || !qc_document(ns, prep_docs[p], 0, 1))
            validation_fail("shifted qprepare failed (%u): %s / %s", p, qc_error(st), qc_error(ns));
        qc_register *rs = qc_lookup(st, 0), *rn = qc_lookup(ns, 0);
        if (!rs || !rn || rs->uniform != rn->uniform || rs->uniform_lower != rn->uniform_lower ||
            rs->uniform_upper != rn->uniform_upper ||
            rs->support_interval != rn->support_interval ||
            rs->support_lower != rn->support_lower || rs->support_upper != rn->support_upper ||
            rs->zero_mask != rn->zero_mask)
            validation_fail("structural vs non-structural qprepare metadata desync (%u)", p);
        qc_destroy(st);
        qc_destroy(ns);
        ++checks;
    }

    const char *bad_structural_find =
        "{\"format\":\"cket.qir\",\"version\":1,\"instructions\":["
        "{\"op\":\"qalloc\",\"id\":0,\"width\":4,\"signed\":false},"
        "{\"op\":\"qprepare\",\"id\":0,\"lower\":3,\"upper\":6},"
        "{\"op\":\"qbind\",\"parameters\":[{\"id\":1,\"bits\":4,\"signed\":false}],"
        "\"instruction\":{\"op\":\"qprepare\",\"id\":1,\"expr\":{\"op\":\"parameter\",\"id\":1,\"bits\":4,\"signed\":false}}},"
        "{\"op\":\"qfind\",\"id\":0,\"lower\":0,\"upper\":3,\"predicate\":{\"op\":\"eq\",\"bits\":4,\"signed\":false,"
        "\"args\":[{\"op\":\"reg\",\"id\":0,\"bits\":4,\"signed\":false},{\"op\":\"const\",\"bits\":4,\"signed\":false,\"value\":3}]}}]}";
    /* Note id 1 wasn't even allocated or lower mismatches; also test valid id 1 + mismatched qfind
     * bounds. */
    const char *bad_find_bounds =
        "{\"format\":\"cket.qir\",\"version\":1,\"instructions\":["
        "{\"op\":\"qalloc\",\"id\":0,\"width\":4,\"signed\":false},"
        "{\"op\":\"qprepare\",\"id\":0,\"lower\":3,\"upper\":6},"
        "{\"op\":\"qalloc\",\"id\":1,\"width\":4,\"signed\":false},"
        "{\"op\":\"qbind\",\"parameters\":[{\"id\":9,\"bits\":4,\"signed\":false}],"
        "\"instruction\":{\"op\":\"qprepare\",\"id\":1,\"expr\":{\"op\":\"parameter\",\"id\":9,\"bits\":4,\"signed\":false}}},"
        "{\"op\":\"qfind\",\"id\":0,\"lower\":0,\"upper\":3,\"predicate\":{\"op\":\"eq\",\"bits\":4,\"signed\":false,"
        "\"args\":[{\"op\":\"reg\",\"id\":0,\"bits\":4,\"signed\":false},{\"op\":\"const\",\"bits\":4,\"signed\":false,\"value\":3}]}}]}";
    for (unsigned b = 0; b < 2; ++b) {
        qc_context *st = qc_create(0, 7);
        if (!st || qc_document(st, b ? bad_find_bounds : bad_structural_find, 1, 1) ||
            !*qc_error(st))
            validation_fail("invalid qfind inside structural document unexpectedly passed");
        qc_destroy(st);
        ++checks;
    }

    const struct {
        unsigned sign;
        const char *lower, *upper, *target;
        unsigned basis_index, kbits;
    } finds[] = {
        {0, "4", "7", "6", 6, 2},
        {0, "3", "6", "5", 5, 2},
        {1, "-8", "-5", "-7", 9, 2},
        {1, "-6", "-3", "-4", 12, 2},
        {1, "-4", "3", "-2", 14, 3},
    };

    for (unsigned f = 0; f < sizeof finds / sizeof *finds; ++f)
        for (unsigned tables = 0; tables < 2; ++tables) {
            qc_context *c = qc_create(1, 7);
            if (!c)
                validation_fail("shifted find context allocation failed");
            qc_allow_tables(c, tables);
            char ir[1024];
            snprintf(ir,
                     sizeof ir,
                     "{\"format\":\"cket.qir\",\"version\":1,\"instructions\":["
                     "{\"op\":\"qalloc\",\"id\":0,\"width\":4,\"signed\":%s},"
                     "{\"op\":\"qprepare\",\"id\":0,\"lower\":%s,\"upper\":%s},"
                     "{\"op\":\"qfind\",\"id\":0,\"lower\":%s,\"upper\":%s,\"solutions\":1,"
                     "\"predicate\":{\"op\":\"eq\",\"bits\":4,\"signed\":%s,\"args\":["
                     "{\"op\":\"reg\",\"id\":0,\"bits\":4,\"signed\":%s},"
                     "{\"op\":\"const\",\"bits\":4,\"signed\":%s,\"value\":%s}]}}]}",
                     finds[f].sign ? "true" : "false",
                     finds[f].lower,
                     finds[f].upper,
                     finds[f].lower,
                     finds[f].upper,
                     finds[f].sign ? "true" : "false",
                     finds[f].sign ? "true" : "false",
                     finds[f].sign ? "true" : "false",
                     finds[f].target);
            if (!qc_document(c, ir, 0, 1))
                validation_fail(
                    "shifted qfind failed case=%u tables=%u: %s", f, tables, qc_error(c));
            double expected_prob =
                finds[f].kbits == 2 ? 1.0 : pow(sin(5.0 * asin(1.0 / sqrt(8.0))), 2);
            double total_prob = 0, target_prob = 0;
            for (size_t i = 0; i < qc_state_size(c); ++i) {
                double p = pow(cabs(qc_amplitude(c, i)), 2);
                total_prob += p;
                if (i == finds[f].basis_index)
                    target_prob = p;
                if (i >= 16 && p > 1e-12)
                    validation_fail("shifted qfind left garbage ancilla probability");
            }
            if (fabs(total_prob - 1.0) > 1e-10 || fabs(target_prob - expected_prob) > 1e-10)
                validation_fail(
                    "shifted qfind distribution mismatch case=%u total=%g target=%g expected=%g",
                    f,
                    total_prob,
                    target_prob,
                    expected_prob);
            qc_destroy(c);
            ++checks;
        }

    const char *bound_search_ir =
        "{\"format\":\"cket.qir\",\"version\":1,\"instructions\":["
        "{\"op\":\"qalloc\",\"id\":0,\"width\":4,\"signed\":true},"
        "{\"op\":\"qprepare\",\"id\":0,\"lower\":-6,\"upper\":-3},"
        "{\"op\":\"qbind\",\"parameters\":[{\"id\":7,\"bits\":32,\"signed\":false}],"
        "\"instruction\":{\"op\":\"qsearch\",\"id\":0,\"lower\":-6,\"upper\":-3,"
        "\"predicate\":{\"op\":\"eq\",\"bits\":4,\"signed\":true,\"args\":["
        "{\"op\":\"reg\",\"id\":0,\"bits\":4,\"signed\":true},"
        "{\"op\":\"const\",\"bits\":4,\"signed\":true,\"value\":-4}]},"
        "\"attempts\":{\"op\":\"parameter\",\"id\":7,\"bits\":32,\"signed\":false}}}]}";
    qc_context *st = qc_create(0, 7);
    if (!st || !qc_document(st, bound_search_ir, 1, 1) || !st->structural)
        validation_fail("structural validation of bound qsearch attempts failed: %s", qc_error(st));
    qc_destroy(st);
    ++checks;

    qc_context *bad_att_ctx = qc_create(1, 7);
    char *bad_bound =
        bad_att_ctx
            ? qc_bind_document(bad_att_ctx, bound_search_ir, "[{\"instruction\":2,\"values\":[0]}]")
            : NULL;
    if (!bad_bound || qc_document(bad_att_ctx, bad_bound, 0, 1) || !*qc_error(bad_att_ctx))
        validation_fail("zero bound attempts unexpectedly succeeded");
    free(bad_bound);
    qc_destroy(bad_att_ctx);
    ++checks;

    qc_context *srch = qc_create(1, 7);
    char *good_bound =
        srch ? qc_bind_document(srch, bound_search_ir, "[{\"instruction\":2,\"values\":[16]}]")
             : NULL;
    if (!good_bound || !qc_document(srch, good_bound, 0, 1))
        validation_fail("bound shifted qsearch execution failed: %s", qc_error(srch));
    double found_prob = pow(cabs(qc_amplitude(srch, 12)), 2);
    if (fabs(found_prob - 1.0) > 1e-10)
        validation_fail("bound shifted qsearch did not converge to |-4>: prob=%g", found_prob);
    free(good_bound);
    qc_destroy(srch);
    ++checks;

    const char *unsat_search = "{\"format\":\"cket.qir\",\"version\":1,\"instructions\":["
                               "{\"op\":\"qalloc\",\"id\":0,\"width\":4,\"signed\":true},"
                               "{\"op\":\"qprepare\",\"id\":0,\"lower\":-6,\"upper\":-3},"
                               "{\"op\":\"qsearch\",\"id\":0,\"lower\":-6,\"upper\":-3,"
                               "\"predicate\":{\"op\":\"eq\",\"bits\":4,\"signed\":true,\"args\":["
                               "{\"op\":\"reg\",\"id\":0,\"bits\":4,\"signed\":true},"
                               "{\"op\":\"const\",\"bits\":4,\"signed\":true,\"value\":0}]},"
                               "\"attempts\":4}]}";
    qc_context *unsat = qc_create(1, 7);
    if (!unsat || !qc_document(unsat, unsat_search, 0, 1))
        validation_fail("shifted unsat qsearch failed: %s", qc_error(unsat));
    double in_domain = 0;
    for (size_t i = 10; i <= 13; ++i)
        in_domain += pow(cabs(qc_amplitude(unsat, i)), 2);
    if (fabs(in_domain - 1.0) > 1e-10)
        validation_fail("shifted qsearch trial re-preparation leaked outside [-6,-3]: %g",
                        in_domain);
    qc_destroy(unsat);
    ++checks;

    puts(
        "PASS 21 shifted/signed uniform qprepare, qfind, structural sync & bound qsearch attempts");
}

static void programs(validation *v) {
    const char *valid[] = {
        "quant bool n=false;int d=0;quant bool q=n?1/d==0:false;return measure(q)!=0||measure(n)!=0;",
        "quant bool n=true;int d=2;quant bool q=n?4/d>1:false;return measure(q)!=1||measure(n)!=1;",
        "quant bool n=superposition(0,1);int d=2;quant bool q=n?(4/d)==2:false;unsigned qq=measure(q),nn=measure(n);return qq!=nn;",
        "quant bool n=false;int d=32;quant bool q=n?((1u<<d)!=0):false;return measure(q)!=0||measure(n)!=0;",
        "quant bool n=true;int d=1;quant bool q=n?(1u<<d)==2:false;return measure(q)!=1||measure(n)!=1;",
        "quant bool n=false;int d=0;quant bool q=n?(_Bool)(7%d):false;return measure(q)!=0||measure(n)!=0;",
        "quant bool n=superposition(0,1);int d=2;quant uint8_t q=div((int)n+5,d++).quot;unsigned qq=measure(q),nn=measure(n);return qq!=(nn+5)/2||d!=3;",
        "quant bool n=superposition(0,1);long d=2;quant uint8_t q=ldiv((long)n+5,d++).rem;unsigned qq=measure(q),nn=measure(n);return qq!=(nn+5)%2||d!=3;",
        "quant bool n=superposition(0,1);long long d=2;quant uint8_t q=lldiv((long long)n+5,d++).quot;unsigned qq=measure(q),nn=measure(n);return qq!=(nn+5)/2||d!=3;",
        "quant bool n=superposition(0,1);intmax_t d=2;quant uint8_t q=imaxdiv((intmax_t)n+5,d++).rem;unsigned qq=measure(q),nn=measure(n);return qq!=(nn+5)%2||d!=3;",
        "quant bool n=superposition(0,1);unsigned a=5;quant uint8_t y=n+a;++a;if(y==6)phase(.31);measure(n);return a!=6;",
        "quant bool n=superposition(0,1);int d=2;quant bool q=n?div((int)n,d).rem:false;unsigned qq=measure(q),nn=measure(n);return qq!=nn;",
        "quant bool n=false;int d=0;quant bool q=n?1/d:false;return measure(q)!=0||measure(n)!=0;",
        "quant bool n=false;int d=32;quant bool q=n?(1<<d):false;return measure(q)!=0||measure(n)!=0;",
        "quant bool n=superposition(0,1);int64_t a=INT64_MIN;quant uint8_t q=(uint64_t)((int64_t)n+a);unsigned qq=measure(q),nn=measure(n);return qq!=nn;",
        "struct{int a;} p={5};quant bool n=superposition(0,1);int d=2;quant uint8_t q=div((int)n+p.a,d).quot;unsigned qq=measure(q),nn=measure(n);return qq!=(nn+5)/2;",
        "#include <quantum.h>\n#include <stdlib.h>\nstatic int calls;static int denominator(void){++calls;return 2;}\nint main(void){quant bool n=superposition(0,1);quant uint8_t q=div((int)n+5,denominator()).quot;unsigned qq=measure(q),nn=measure(n);return qq!=(nn+5)/2||calls!=1;}\n",
        "#include <quantum.h>\ntypedef struct{bool a,b;} pair;static int calls;static pair make(unsigned n){++calls;return(pair){n&1,!(n&1)};}\nint main(void){quant pair p[2]={make(1),make(2)};unsigned a=measure(p[0].a),b=measure(p[0].b),c=measure(p[1].a),d=measure(p[1].b);return calls!=2||a!=1||b||c||d!=1;}\n",
        "#include <quantum.h>\ntypedef struct{uint8_t a,b;} pair;static int calls;static pair make(unsigned n){++calls;return(pair){n,n+1};}\nint main(void){quant pair p=make(7);unsigned a=measure(p.a),b=measure(p.b);return a!=7||b!=8||calls!=1;}\n",
        "#include <quantum.h>\n#include <stdlib.h>\nstatic int calls;static int denominator(int *d){++calls;return (*d)++;}\nint main(void){quant bool n=superposition(0,1);int d=1;unsigned values[2];for(unsigned k=0;k<2;++k){quant uint8_t q=div((int)n+7,denominator(&d)).quot;values[k]=measure(q);}unsigned value=measure(n);if(calls!=2||d!=3)return 1;for(unsigned k=0;k<2;++k)if(values[k]!=(value+7)/(k+1))return 2;return 0;}\n"};
    const char *invalid[] = {
        "quant bool n=true;int d=32;quant bool q=n?(1u<<d)!=0:false;measure(q);measure(n);",
        "quant bool n=superposition(0,1);volatile int d=2;quant bool q=n?4/d>1:false;measure(q);measure(n);",
        "quant bool n=superposition(0,1);volatile bool d=true;quant bool q=n?d:false;measure(q);measure(n);",
        "quant bool n=superposition(0,1);volatile struct{int a;} p={2};quant bool q=n?p.a>0:false;measure(q);measure(n);",
        "quant bool n=superposition(0,1);volatile int d=2;volatile int *p=&d;quant bool q=n?*p>0:false;measure(q);measure(n);",
        "quant bool n=superposition(0,1);int d=1;quant bool q=n?(++d)>0:false;measure(q);measure(n);",
        "quant bool n=superposition(0,1);int d=0;quant bool q=div((int)n,d).quot;measure(q);measure(n);",
        "quant bool n=superposition(0,1);int d=2;quant bool q=n?div((int)n,d++).quot:false;measure(q);measure(n);",
        "quant bool n=superposition(0,1);unsigned a=5;quant uint8_t q=n+a++;measure(q);measure(n);",
        "quant bool n=true;int d=0;quant bool q=n?1/d:false;measure(q);measure(n);",
        "quant bool n=superposition(0,1);int d=2;int q=div((int)n,d).quot;measure(n);return q;"};
    const char *diagnostics[] = {"shift",
                                 "once-only capture",
                                 "once-only capture",
                                 "once-only capture",
                                 "once-only capture",
                                 "once-only capture",
                                 "zero",
                                 "coherent control",
                                 "once-only capture",
                                 "zero",
                                 "classical initialization"};
    for (unsigned level = 0; level < 5; ++level)
        for (unsigned builtins = 0; builtins < 2; ++builtins) {
            for (unsigned i = 0; i < sizeof valid / sizeof *valid; ++i)
                compile_run(v, valid[i], level, builtins, NULL);
            for (unsigned i = 0; i < sizeof invalid / sizeof *invalid; ++i)
                compile_run(v, invalid[i], level, builtins, diagnostics[i]);
        }
    puts(
        "PASS 310 C cases: CPU capture, old-value cleanup, lazy sanitizer guards, integer widths, products, callbacks in loops and nested initializer regression");
}

static void documents(validation *v) {
    const char *bodies[] = {
        "quant bool n=superposition(0,1);if(n)phase(.31);int d=2;quant uint8_t q=div((int)n+5,d).quot;quant uint8_t r=div((int)n+5,d).rem;measure(q);measure(r);measure(n);return 0;",
        "quant bool n=superposition(0,1);unsigned a=5;quant uint8_t y=n+a;++a;if(y==6)phase(.31);measure(n);return 0;"};
    for (unsigned profile = 0; profile < 2; ++profile)
        for (unsigned level = 0; level < 5; ++level)
            for (unsigned builtins = 0; builtins < 2; ++builtins) {
                char source[4096], compiler[4096], path[4096], program[4096];
                validation_path(source, sizeof source, "%s/document.c", v->temp);
                validation_path(path, sizeof path, "%s/document.json", v->temp);
                validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
                validation_path(program,
                                sizeof program,
                                "#include <quantum.h>\n#include <stdlib.h>\nint main(void){%s}\n",
                                bodies[profile]);
                validation_write(source, program);
                char *command[] = {compiler,
                                   source,
                                   "--emit-ir",
                                   (char *)levels[level],
                                   builtins ? "-fbuiltin" : "-fno-builtin",
                                   "-o",
                                   path,
                                   NULL};
                if (validation_run(v, command, NULL, 60)) {
                    char *log = validation_read(v->log);
                    validation_fail("binding document compile failed: %s", log);
                }
                char *ir = validation_read(path);
                cj_doc d = {0};
                if (!cj_parse(&d, ir))
                    validation_fail("binding document JSON failed");
                char rows[4096] = "[";
                size_t length = 1;
                unsigned index = 0, count = 0;
                int list = cj_get(&d, 0, "instructions");
                for (int token = list + 1; token < d.tokens[list].next;
                     token = d.tokens[token].next, ++index) {
                    if (!cj_equal(&d, cj_get(&d, token, "op"), "qbind"))
                        continue;
                    int parameters = cj_get(&d, token, "parameters");
                    if (d.tokens[parameters].children != 1)
                        validation_fail("unexpected fixture binding count");
                    int written = snprintf(rows + length,
                                           sizeof rows - length,
                                           "%s{\"instruction\":%u,\"values\":[%u]}",
                                           count++ ? "," : "",
                                           index,
                                           profile ? 5 : 2);
                    if (written < 0 || (size_t)written >= sizeof rows - length)
                        validation_fail("fixture binding rows exceeded budget");
                    length += (size_t)written;
                }
                if (!count)
                    validation_fail("CPU fixture did not emit binding templates");
                rows[length++] = ']';
                rows[length] = 0;
                qc_context *type_context = qc_create(0, 7);
                if (!type_context || !qc_document(type_context, ir, 1, 1) ||
                    !type_context->structural)
                    validation_fail("template schema validation failed: %s",
                                    qc_error(type_context));
                FILE *sink = tmpfile();
                if (!sink || qc_qasm(type_context, sink))
                    validation_fail("template validation was exported as gates");
                fclose(sink);
                qc_destroy(type_context);
                for (unsigned tables = 0; tables < 2; ++tables) {
                    qc_context *c = qc_create(1, 7);
                    if (!c)
                        validation_fail("document state allocation failed");
                    qc_allow_tables(c, tables);
                    char *bound = qc_bind_document(c, ir, rows);
                    if (!bound || !qc_document(c, bound, 0, 1) || c->structural || c->reset ||
                        c->measure != (profile ? 1u : 17u))
                        validation_fail("bound document execution failed: %s", qc_error(c));
                    for (size_t i = 0; i < qc_state_size(c); ++i) {
                        double complex expected = 0;
                        if (i == (profile ? 0u : 516u))
                            expected = 1 / sqrt(2.0);
                        if (i == (profile ? 1u : 7u))
                            expected = cexp(I * .31) / sqrt(2.0);
                        if (cabs(qc_amplitude(c, i) - expected) > 1e-10)
                            validation_fail(
                                "bound C document phase/workspace mismatch profile=%u basis=%zu",
                                profile,
                                i);
                    }
                    free(bound);
                    qc_destroy(c);
                    ++checks;
                }
                cj_free(&d);
                free(ir);
            }
    puts(
        "PASS 40 C template specializations: libc projections, CPU mutation, complete phases and structural/export separation");
}

int main(int argc, char **argv) {
    if (argc != 1)
        validation_fail("usage: validate_bindings");
    states();
    endpoints();
    rejected();
    document_contracts();
    shifted_search_and_structural();
    validation v;
    validation_init(&v, argv[0]);
    programs(&v);
    documents(&v);
    validation_cleanup(&v);
    printf(
        "%u CPU binding checks passed; structural templates and bound circuit correctness have separate scopes.\n",
        checks);
    return 0;
}
