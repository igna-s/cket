#include "internal.h"
#include "validation.h"
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
static const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
static const char *functions[] = {"div", "ldiv", "lldiv", "imaxdiv"};
static const char *types[] = {"int", "long", "long long", "intmax_t"};
static const char *products[] = {"div_t", "ldiv_t", "lldiv_t", "imaxdiv_t"};

static void apply(qc_context *c, const char *operation) {
    if (!qc_apply(c, operation, NULL))
        validation_fail("division operation failed: %s", qc_error(c));
}

static size_t basis(qc_register *r, unsigned value) {
    size_t result = 0;
    for (unsigned bit = 0; bit < r->width; ++bit)
        result |= (size_t)((value >> bit) & 1) << r->wires[bit];
    return result;
}

static char *compile_case(validation *v,
                          const char *body,
                          unsigned level,
                          unsigned builtins,
                          const char *action,
                          const char *diagnostic) {
    char program[8192], source[4096], compiler[4096], artifact[4096];
    if (*body == '#')
        validation_path(program, sizeof program, "%s", body);
    else
        validation_path(
            program,
            sizeof program,
            "#include <quantum.h>\n#include <stdlib.h>\n#include <inttypes.h>\n#include <limits.h>\n"
            "int main(void){%s}\n",
            body);
    validation_path(source, sizeof source, "%s/division.c", v->temp);
    validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
    validation_path(artifact, sizeof artifact, "%s/division.json", v->temp);
    validation_write(source, program);
    char *command[] = {compiler,
                       source,
                       (char *)action,
                       (char *)levels[level],
                       builtins ? "-fbuiltin" : "-fno-builtin",
                       "-o",
                       artifact,
                       NULL};
    int status = validation_run(v, command, NULL, 60);
    char *log = validation_read(v->log);
    if (diagnostic ? !validation_rejected(status, log, diagnostic) : status != 0)
        validation_fail(
            "division case %s builtins=%u failed: %s\n%s", levels[level], builtins, body, log);
    free(log);
    return !diagnostic && !strcmp(action, "--emit-ir") ? validation_read(artifact) : NULL;
}

static void raw_case(
    unsigned width, int sign, int denominator, unsigned remainder, unsigned tables, int control) {
    qc_context *c = qc_create(1, 7);
    if (!c)
        validation_fail("division context allocation failed");
    qc_allow_tables(c, tables);
    char operation[4096], expression[3072];
    validation_path(operation,
                    sizeof operation,
                    "{\"op\":\"qalloc\",\"id\":0,\"width\":%u,\"signed\":%s}",
                    width,
                    sign ? "true" : "false");
    apply(c, operation);
    validation_path(operation,
                    sizeof operation,
                    "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":%u}",
                    sign ? 0 : (1u << width) - 1);
    apply(c, operation);
    if (sign)
        apply(c, "{\"op\":\"qfourier\",\"id\":0,\"inverse\":false}");
    validation_path(operation,
                    sizeof operation,
                    "{\"op\":\"qalloc\",\"id\":1,\"width\":%u,\"signed\":false}",
                    width);
    apply(c, operation);
    if (control >= 0) {
        apply(c, "{\"op\":\"qalloc\",\"id\":2,\"width\":1,\"signed\":false}");
        apply(c, "{\"op\":\"qprepare\",\"id\":2,\"lower\":0,\"upper\":1}");
    }
    if (!qc_emit(c, QC_PHASE, 0, QC_WIRE(0), QC_WIRE(0), .31) ||
        !qc_emit(c, QC_PHASE, 0, QC_WIRE(width - 1), QC_WIRE(width - 1), .47))
        validation_fail("division relative phase preparation failed");
    validation_path(expression,
                    sizeof expression,
                    "{\"op\":\"cast\",\"bits\":%u,\"signed\":false,\"args\":["
                    "{\"op\":\"%s\",\"bits\":16,\"signed\":%s,\"args\":["
                    "{\"op\":\"cast\",\"bits\":16,\"signed\":%s,\"args\":["
                    "{\"op\":\"reg\",\"id\":0,\"bits\":%u,\"signed\":%s}]},"
                    "{\"op\":\"const\",\"bits\":16,\"signed\":%s,\"value\":%d}]}]}",
                    width,
                    remainder ? "mod" : "div",
                    sign ? "true" : "false",
                    sign ? "true" : "false",
                    width,
                    sign ? "true" : "false",
                    sign ? "true" : "false",
                    denominator);
    char condition[96] = "";
    if (control >= 0)
        validation_path(
            condition, sizeof condition, ",\"controls\":[{\"id\":2,\"value\":%d}]", control);
    validation_path(operation,
                    sizeof operation,
                    "{\"op\":\"qcompute\",\"id\":1,\"expr\":%s%s}",
                    expression,
                    condition);
    apply(c, operation);
    unsigned mask = (1u << width) - 1, public_bits = 2 * width + (control >= 0);
    for (unsigned inverse = 0; inverse < 2; ++inverse) {
        for (size_t i = 0; i < qc_state_size(c); ++i) {
            unsigned raw = (unsigned)i & mask;
            int value = sign && raw >= (1u << (width - 1)) ? (int)raw - (1 << width) : (int)raw;
            unsigned flag = control >= 0 ? (unsigned)(i >> (2 * width)) & 1 : 0;
            int active = control < 0 || flag == (unsigned)control;
            unsigned result =
                inverse || !active
                    ? 0
                    : (unsigned)(remainder ? value % denominator : value / denominator) & mask;
            double complex expected = 0;
            if (((i >> width) & mask) == result && !(i >> public_bits))
                expected = cexp(I * ((raw & 1 ? .31 : 0) + (raw & (1u << (width - 1)) ? .47 : 0))) /
                           sqrt((double)(1u << (width + (control >= 0))));
            if (cabs(qc_amplitude(c, i) - expected) > 1e-10)
                validation_fail(
                    "division joint state width=%u signed=%d denominator=%d rem=%u tables=%u control=%d inverse=%u basis=%zu",
                    width,
                    sign,
                    denominator,
                    remainder,
                    tables,
                    control,
                    inverse,
                    i);
        }
        if (!inverse) {
            validation_path(operation,
                            sizeof operation,
                            "{\"op\":\"quncompute\",\"id\":1,\"expr\":%s%s}",
                            expression,
                            condition);
            apply(c, operation);
        }
    }
    if (c->measure || c->reset)
        validation_fail("division introduced a destructive channel");
    qc_destroy(c);
    ++checks;
}

static void raw_states(void) {
    const int denominators[] = {1, 2, 4, 8, -1, -2, -4, -8};
    for (unsigned width = 2; width <= 4; ++width)
        for (unsigned sign = 0; sign < 2; ++sign)
            for (unsigned d = 0; d < (sign ? 8u : 4u); ++d)
                for (unsigned remainder = 0; remainder < 2; ++remainder)
                    for (unsigned tables = 0; tables < 2; ++tables)
                        for (int control = 0; control < 2; ++control)
                            raw_case(width, sign, denominators[d], remainder, tables, control);
    for (unsigned sign = 0; sign < 2; ++sign)
        for (unsigned negative = 0; negative <= sign; ++negative)
            for (unsigned remainder = 0; remainder < 2; ++remainder)
                for (unsigned tables = 0; tables < 2; ++tables)
                    raw_case(8, sign, negative ? -2 : 2, remainder, tables, -1);
    puts(
        "PASS 300 power-of-two domains: full relative phases, signed C rounding, narrowing, both controls and clean inverse");
    fflush(stdout);
}

static void library_states(validation *v) {
    for (unsigned fn = 0; fn < 4; ++fn)
        for (unsigned profile = 0; profile < 3; ++profile)
            for (unsigned level = 0; level < 5; ++level)
                for (unsigned builtins = 0; builtins < 2; ++builtins) {
                    char body[2048];
                    const char *prefix = profile == 1 ? "c?" : profile == 2 ? "!c?" : "";
                    const char *suffix = profile ? ":false" : "";
                    validation_path(
                        body,
                        sizeof body,
                        "quant bool x=superposition(0,1),d=superposition(0,1),c=superposition(0,1);"
                        "if(x)phase(.31);if(d)phase(.47);"
                        "quant bool q=%s%s((%s)x,(%s)d+1).quot%s;"
                        "quant bool r=%s%s((%s)x,(%s)d+1).rem%s;"
                        "measure(q);measure(r);measure(x);measure(d);measure(c);return 0;",
                        prefix,
                        functions[fn],
                        types[fn],
                        types[fn],
                        suffix,
                        prefix,
                        functions[fn],
                        types[fn],
                        types[fn],
                        suffix);
                    char *ir = compile_case(v, body, level, builtins, "--emit-ir", NULL);
                    for (unsigned tables = 0; tables < 2; ++tables) {
                        qc_context *c = qc_create(1, 7);
                        if (!c)
                            validation_fail("library division allocation failed");
                        qc_allow_tables(c, tables);
                        if (!qc_document(c, ir, 0, 1))
                            validation_fail("library division lowering failed: %s", qc_error(c));
                        qc_register *registers[5];
                        for (unsigned j = 0; j < 5; ++j) {
                            registers[j] = qc_lookup(c, j);
                            if (!registers[j] || registers[j]->width != 1)
                                validation_fail("division result register layout changed");
                        }
                        if (c->measure != 5 || c->reset)
                            validation_fail("library division inserted an undeclared channel");
                        size_t indices[8];
                        double complex amplitudes[8];
                        for (unsigned branch = 0; branch < 8; ++branch) {
                            unsigned x = branch & 1, d = (branch >> 1) & 1, flag = branch >> 2;
                            unsigned active = !profile || flag == (profile == 1);
                            indices[branch] = basis(registers[0], x) | basis(registers[1], d) |
                                              basis(registers[2], flag) |
                                              basis(registers[3], active ? x / (d + 1) : 0) |
                                              basis(registers[4], active ? x % (d + 1) : 0);
                            amplitudes[branch] = cexp(I * (x * .31 + d * .47)) / sqrt(8.0);
                        }
                        for (size_t i = 0; i < qc_state_size(c); ++i) {
                            double complex expected = 0;
                            for (unsigned branch = 0; branch < 8; ++branch)
                                if (i == indices[branch])
                                    expected = amplitudes[branch];
                            if (cabs(qc_amplitude(c, i) - expected) > 1e-10)
                                validation_fail(
                                    "library division joint state fn=%s profile=%u level=%s builtin=%u tables=%u basis=%zu",
                                    functions[fn],
                                    profile,
                                    levels[level],
                                    builtins,
                                    tables,
                                    i);
                        }
                        qc_destroy(c);
                        ++checks;
                    }
                    free(ir);
                }
    puts(
        "PASS 240 library joint states: variable denominator, both product projections, coherent control, builtins and five GCC levels");
    fflush(stdout);
}

static void numeric_and_classical(validation *v) {
    for (unsigned fn = 0; fn < 4; ++fn)
        for (unsigned level = 0; level < 5; ++level)
            for (unsigned builtins = 0; builtins < 2; ++builtins) {
                for (int denominator = -2; denominator <= 2; denominator += 4) {
                    char body[2048];
                    validation_path(
                        body,
                        sizeof body,
                        "quant bool n=superposition(0,1);"
                        "quant int8_t q=%s(3*(%s)n-1,%d).quot;int qq=measure(q);reset(q);"
                        "quant int8_t r=%s(3*(%s)n-1,%d).rem;int rr=measure(r);"
                        "int nn=measure(n),a=3*nn-1;return qq!=a/%d||rr!=a%%%d;",
                        functions[fn],
                        types[fn],
                        denominator,
                        functions[fn],
                        types[fn],
                        denominator,
                        denominator,
                        denominator);
                    free(compile_case(v, body, level, builtins, "--run", NULL));
                    ++checks;
                }
                char body[1024];
                validation_path(body,
                                sizeof body,
                                "%s a=13,b=5;%s result=%s(a++,b++);quant bool q=true;"
                                "return result.quot!=2||result.rem!=3||a!=14||b!=6||!measure(q);",
                                types[fn],
                                products[fn],
                                functions[fn]);
                free(compile_case(v, body, level, builtins, "--run", NULL));
                ++checks;
                validation_path(
                    body,
                    sizeof body,
                    "quant bool n=superposition(0,1);quant bool q=%s((%s)n+5,2).quot;"
                    "int qq=measure(q);int nn=measure(n);return qq!=((_Bool)((nn+5)/2));",
                    functions[fn],
                    types[fn]);
                free(compile_case(v, body, level, builtins, "--run", NULL));
                ++checks;
            }
    for (unsigned level = 0; level < 5; ++level)
        for (unsigned builtins = 0; builtins < 2; ++builtins) {
            const char *body = "#include <quantum.h>\n"
                               "typedef struct {int quot,rem;} result;"
                               "static result div(int n,int d){return (result){n+17,d+7};}"
                               "int main(void){result r=div(1,2);quant bool q=true;"
                               "return r.quot!=18||r.rem!=9||!measure(q);}\n";
            free(compile_case(v, body, level, builtins, "--run", NULL));
            ++checks;
        }
    puts(
        "PASS 80 signed numeric executions and 40 ordinary libc executions with once-only argument side effects");
    puts("PASS 40 Boolean conversions use C truth rather than truncating the low bit");
    puts("PASS 10 private homonyms retain their own C behavior");
    fflush(stdout);
}

static void generation_and_contracts(validation *v) {
    for (unsigned fn = 0; fn < 4; ++fn)
        for (unsigned level = 0; level < 5; ++level)
            for (unsigned builtins = 0; builtins < 2; ++builtins) {
                for (unsigned remainder = 0; remainder < 2; ++remainder) {
                    char body[1024];
                    validation_path(body,
                                    sizeof body,
                                    "quant %s n=0;fourier(n);quant uint8_t r=%s(n,%d).%s;"
                                    "measure(r);measure(n);return 0;",
                                    types[fn],
                                    functions[fn],
                                    remainder ? -2 : 2,
                                    remainder ? "rem" : "quot");
                    char *ir = compile_case(v, body, level, builtins, "--emit-ir", NULL);
                    qc_context *c = qc_create(0, 7);
                    if (!c)
                        validation_fail("wide division context allocation failed");
                    qc_allow_tables(c, 0);
                    if (!qc_document(c, ir, 0, 1) || qc_qubits(c) > 128 || c->reset ||
                        c->measure != (fn ? 72u : 40u))
                        validation_fail("wide scalar division failed: %s", qc_error(c));
                    qc_destroy(c);
                    free(ir);
                    ++checks;
                }
                char body[1024];
                validation_path(body,
                                sizeof body,
                                "quant bool n=superposition(0,1);quant %s result=%s((%s)n+5,2);"
                                "measure(result.quot);measure(result.rem);measure(n);return 0;",
                                products[fn],
                                functions[fn],
                                types[fn]);
                free(compile_case(v, body, level, builtins, "--resources", NULL));
                ++checks;
                const char *limits[] = {"INT_MIN", "LONG_MIN", "LLONG_MIN", "INTMAX_MIN"};
                for (unsigned negative = 0; negative < 4; ++negative) {
                    if (negative < 2)
                        validation_path(body,
                                        sizeof body,
                                        "quant %s n=%s;quant bool r=%s(n,%d).rem;"
                                        "measure(r);measure(n);return 0;",
                                        types[fn],
                                        negative ? limits[fn] : "1",
                                        functions[fn],
                                        negative ? -1 : 0);
                    else
                        validation_path(
                            body,
                            sizeof body,
                            "quant bool n=superposition(0,1);%s d=2;"
                            "quant bool r=%s((%s)n,%s).quot;measure(r);measure(n);return 0;",
                            types[fn],
                            functions[fn],
                            types[fn],
                            negative == 2 ? "d" : "d++");
                    free(compile_case(v,
                                      body,
                                      level,
                                      builtins,
                                      "--resources",
                                      negative == 0   ? "zero"
                                      : negative == 1 ? "overflows"
                                                      : "CPU parameter bindings"));
                    ++checks;
                }
            }
    puts(
        "PASS 80 wide table-free projections, 40 whole-product resource contracts and 160 definedness/capture diagnostics");
    fflush(stdout);
}

static uint64_t sparse_reg_value(const uint64_t *words, const qc_register *r) {
    uint64_t v = 0;
    for (unsigned b = 0; b < r->width; ++b) {
        unsigned w = r->wires[b];
        if ((words[w / 64] >> (w % 64)) & 1ULL)
            v |= 1ULL << b;
    }
    return v;
}

static void fmt_i64_literal(char *buf, size_t cap, int is_signed, uint64_t val) {
    int neg = is_signed && ((val >> 63) & 1U);
    uint64_t mag = neg ? (~val + 1ULL) : val;
    snprintf(buf, cap, "%s%" PRIu64, neg ? "-" : "", mag);
}

static void check_wide64_case(
    int is_signed, uint64_t x0, uint64_t x1, uint64_t d0, uint64_t d1, int remainder, int tables) {
    qc_context *c = qc_create_sparse(7, 1024);
    if (!c)
        validation_fail("wide64 sparse context allocation failed");
    qc_allow_tables(c, tables);
    apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":1,\"signed\":false}");
    apply(c, "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":1}");
    apply(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":1,\"signed\":false}");
    apply(c, "{\"op\":\"qprepare\",\"id\":1,\"lower\":0,\"upper\":1}");
    qc_register *s0 = qc_lookup(c, 0), *s1 = qc_lookup(c, 1);
    if (!qc_emit(c, QC_PHASE, 0, QC_WIRE(s0->wires[0]), QC_WIRE(s0->wires[0]), .29) ||
        !qc_emit(c, QC_PHASE, 0, QC_WIRE(s1->wires[0]), QC_WIRE(s1->wires[0]), .43))
        validation_fail("wide64 phase preparation failed");
    char op[4096], expr[2048], s_x0[32], s_x1[32], s_d0[32], s_d1[32];
    fmt_i64_literal(s_x0, sizeof s_x0, is_signed, x0);
    fmt_i64_literal(s_x1, sizeof s_x1, is_signed, x1);
    fmt_i64_literal(s_d0, sizeof s_d0, is_signed, d0);
    fmt_i64_literal(s_d1, sizeof s_d1, is_signed, d1);
    const char *stype = is_signed ? "true" : "false";
    validation_path(
        op, sizeof op, "{\"op\":\"qalloc\",\"id\":2,\"width\":64,\"signed\":%s}", stype);
    apply(c, op);
    validation_path(
        op,
        sizeof op,
        "{\"op\":\"qcompute\",\"id\":2,\"expr\":{\"op\":\"select\",\"bits\":64,\"signed\":%s,\"args\":["
        "{\"op\":\"reg\",\"id\":0,\"bits\":1,\"signed\":false},"
        "{\"op\":\"const\",\"bits\":64,\"signed\":%s,\"value\":%s},"
        "{\"op\":\"const\",\"bits\":64,\"signed\":%s,\"value\":%s}]}}",
        stype,
        stype,
        s_x1,
        stype,
        s_x0);
    apply(c, op);
    int store_div = !is_signed || ((int64_t)d0 > 0 && (int64_t)d1 > 0);
    if (store_div) {
        validation_path(
            op, sizeof op, "{\"op\":\"qalloc\",\"id\":3,\"width\":64,\"signed\":%s}", stype);
        apply(c, op);
        validation_path(
            op,
            sizeof op,
            "{\"op\":\"qcompute\",\"id\":3,\"expr\":{\"op\":\"select\",\"bits\":64,\"signed\":%s,"
            "\"args\":[{\"op\":\"reg\",\"id\":1,\"bits\":1,\"signed\":false},"
            "{\"op\":\"const\",\"bits\":64,\"signed\":%s,\"value\":%s},"
            "{\"op\":\"const\",\"bits\":64,\"signed\":%s,\"value\":%s}]}}",
            stype,
            stype,
            s_d1,
            stype,
            s_d0);
        apply(c, op);
    }
    validation_path(
        op, sizeof op, "{\"op\":\"qalloc\",\"id\":4,\"width\":64,\"signed\":%s}", stype);
    apply(c, op);
    char denom_expr[512];
    if (store_div)
        validation_path(denom_expr,
                        sizeof denom_expr,
                        "{\"op\":\"reg\",\"id\":3,\"bits\":64,\"signed\":%s}",
                        stype);
    else
        validation_path(denom_expr,
                        sizeof denom_expr,
                        "{\"op\":\"select\",\"bits\":64,\"signed\":%s,\"args\":["
                        "{\"op\":\"reg\",\"id\":1,\"bits\":1,\"signed\":false},"
                        "{\"op\":\"const\",\"bits\":64,\"signed\":%s,\"value\":%s},"
                        "{\"op\":\"const\",\"bits\":64,\"signed\":%s,\"value\":%s}]}",
                        stype,
                        stype,
                        s_d1,
                        stype,
                        s_d0);
    validation_path(expr,
                    sizeof expr,
                    "{\"op\":\"%s\",\"bits\":64,\"signed\":%s,\"args\":["
                    "{\"op\":\"reg\",\"id\":2,\"bits\":64,\"signed\":%s},%s]}",
                    remainder ? "mod" : "div",
                    stype,
                    stype,
                    denom_expr);
    validation_path(op, sizeof op, "{\"op\":\"qcompute\",\"id\":4,\"expr\":%s}", expr);
    apply(c, op);
    qc_register *rx = qc_lookup(c, 2), *rd = store_div ? qc_lookup(c, 3) : NULL,
                *rq = qc_lookup(c, 4);
    for (int inv = 0; inv < 2; ++inv) {
        if (qc_sparse_size(c) != 4)
            validation_fail("wide64 unexpected sparse branch count %zu", qc_sparse_size(c));
        qc_mask public_mask = qc_mask_zero();
        const qc_register *regs[5] = {s0, s1, rx, rq, rd};
        for (int r = 0; r < (store_div ? 5 : 4); ++r)
            for (unsigned b = 0; b < regs[r]->width; ++b)
                public_mask = qc_mask_set(public_mask, regs[r]->wires[b]);
        for (size_t row = 0; row < qc_sparse_size(c); ++row) {
            uint64_t words[16] = {0};
            double complex amp = 0;
            if (!qc_sparse_entry_words(c, row, words, 16, &amp))
                validation_fail("wide64 sparse row %zu read failed", row);
            for (unsigned w = 0; w < QC_MAX_PHYSICAL; ++w)
                if (!qc_mask_test(public_mask, w) && ((words[w / 64] >> (w % 64)) & 1ULL))
                    validation_fail("wide64 uncomputed garbage remained on wire %u", w);
            uint64_t b0 = sparse_reg_value(words, s0);
            uint64_t b1 = sparse_reg_value(words, s1);
            uint64_t vx = sparse_reg_value(words, rx);
            uint64_t vd = store_div ? sparse_reg_value(words, rd) : (b1 ? d1 : d0);
            uint64_t vq = sparse_reg_value(words, rq);
            uint64_t ex = b0 ? x1 : x0;
            uint64_t ed = b1 ? d1 : d0;
            uint64_t eq = 0;
            if (!inv) {
                if (is_signed) {
                    int64_t sx = (int64_t)ex, sd = (int64_t)ed;
                    eq = (uint64_t)(remainder ? (sx % sd) : (sx / sd));
                } else {
                    eq = remainder ? (ex % ed) : (ex / ed);
                }
            }
            double complex eamp = cexp(I * ((b0 ? .29 : 0.0) + (b1 ? .43 : 0.0))) / 2.0;
            if (vx != ex || vd != ed || vq != eq || cabs(amp - eamp) > 1e-10)
                validation_fail("wide64 state mismatch signed=%d rem=%d inv=%d b0=%llu b1=%llu",
                                is_signed,
                                remainder,
                                inv,
                                (unsigned long long)b0,
                                (unsigned long long)b1);
        }
        if (!inv) {
            validation_path(op, sizeof op, "{\"op\":\"quncompute\",\"id\":4,\"expr\":%s}", expr);
            apply(c, op);
        }
    }
    qc_destroy(c);
    ++checks;
}

static void wide64_cases(validation *v) {
    for (int rem = 0; rem < 2; ++rem)
        for (int tables = 0; tables < 2; ++tables) {
            check_wide64_case(0, UINT64_MAX - 17ULL, UINT64_MAX, 17ULL, 19ULL, rem, tables);
            check_wide64_case(1,
                              (uint64_t)(INT64_MIN + 5),
                              (uint64_t)INT64_MIN,
                              (uint64_t)(int64_t)-17,
                              (uint64_t)(int64_t)-19,
                              rem,
                              tables);
            check_wide64_case(1,
                              (uint64_t)INT64_MAX,
                              (uint64_t)(INT64_MIN + 1),
                              (uint64_t)(int64_t)17,
                              (uint64_t)(int64_t)23,
                              rem,
                              tables);
            check_wide64_case(1,
                              (uint64_t)INT64_MAX,
                              (uint64_t)INT64_MIN,
                              (uint64_t)(int64_t)-17,
                              (uint64_t)(int64_t)-17,
                              rem,
                              tables);
        }
    for (unsigned level = 0; level < 5; ++level)
        for (unsigned rem = 0; rem < 2; ++rem) {
            char body[1024];
            validation_path(body,
                            sizeof body,
                            "quant bool s=superposition(0,1);quant int64_t n=s?INT64_MIN:INT64_MAX;"
                            "quant int64_t r=n%s(s?-17:-19);"
                            "measure(r);measure(n);measure(s);return 0;",
                            rem ? "%" : "/");
            char *ir = compile_case(v, body, level, 1, "--emit-ir", NULL);
            qc_context *c = qc_create_sparse(7, 1024);
            if (!c)
                validation_fail("wide64 C case sparse context failed");
            qc_allow_tables(c, 0);
            if (!qc_document(c, ir, 0, 1) || qc_qubits(c) > QC_MAX_PHYSICAL || c->reset ||
                qc_sparse_size(c) != 2)
                validation_fail("wide64 C division synthesis failed: %s", qc_error(c));
            qc_register *rs = qc_lookup(c, 0), *rn = qc_lookup(c, 1), *rr = qc_lookup(c, 2);
            for (size_t row = 0; row < qc_sparse_size(c); ++row) {
                uint64_t words[16] = {0};
                double complex amp = 0;
                if (!qc_sparse_entry_words(c, row, words, 16, &amp))
                    validation_fail("wide64 C sparse read failed");
                uint64_t s = sparse_reg_value(words, rs);
                int64_t n = (int64_t)sparse_reg_value(words, rn);
                int64_t r = (int64_t)sparse_reg_value(words, rr);
                int64_t en = s ? INT64_MIN : INT64_MAX;
                int64_t ed = s ? -17 : -19;
                int64_t er = rem ? (en % ed) : (en / ed);
                if (n != en || r != er)
                    validation_fail(
                        "wide64 C value mismatch rem=%u s=%llu", rem, (unsigned long long)s);
            }
            qc_destroy(c);
            free(ir);
            ++checks;
        }
    for (int rem = 0; rem < 2; ++rem) {
        qc_context *c = qc_create_sparse(7, 64);
        if (!c)
            validation_fail("wide64 overflow context allocation failed");
        apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":64,\"signed\":true}");
        char op[512], smin[32];
        fmt_i64_literal(smin, sizeof smin, 1, (uint64_t)INT64_MIN);
        validation_path(
            op,
            sizeof op,
            "{\"op\":\"qcompute\",\"id\":0,\"expr\":{\"op\":\"%s\",\"bits\":64,\"signed\":true,"
            "\"args\":[{\"op\":\"const\",\"bits\":64,\"signed\":true,\"value\":%s},"
            "{\"op\":\"const\",\"bits\":64,\"signed\":true,\"value\":-1}]}}",
            rem ? "mod" : "div",
            smin);
        if (qc_apply(c, op, NULL) || !strstr(qc_error(c), "overflow"))
            validation_fail(
                "INT64_MIN %s -1 should report overflow, got: %s", rem ? "%" : "/", qc_error(c));
        qc_destroy(c);
        ++checks;
    }
    puts(
        "PASS 12 general 64-bit sparse superposition div/mod, 10 C int64_t levels and INT64_MIN/-1 overflow checks");
    fflush(stdout);
}

int main(int argc, char **argv) {
    if (argc != 1)
        validation_fail("usage: validate_division");
    validation v;
    validation_init(&v, argv[0]);
    raw_states();
    library_states(&v);
    numeric_and_classical(&v);
    generation_and_contracts(&v);
    wide64_cases(&v);
    validation_cleanup(&v);
    printf(
        "%u division checks passed; full-state validation, explicit channels and wide generation have separate scopes.\n",
        checks);
    return 0;
}
