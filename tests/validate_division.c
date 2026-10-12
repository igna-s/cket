#include "internal.h"
#include "validation.h"
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
                free(compile_case(v, body, level, builtins, "--resources", fn ? "128" : NULL));
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

int main(int argc, char **argv) {
    if (argc != 1)
        validation_fail("usage: validate_division");
    validation v;
    validation_init(&v, argv[0]);
    raw_states();
    library_states(&v);
    numeric_and_classical(&v);
    generation_and_contracts(&v);
    validation_cleanup(&v);
    printf(
        "%u division checks passed; full-state validation, explicit channels and wide generation have separate scopes.\n",
        checks);
    return 0;
}
