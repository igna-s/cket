#include "internal.h"
#include "validation.h"
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks, nowned;
static char *owned[128];

static const char *keep(const char *text) {
    if (nowned == 128)
        validation_fail("test expression budget exceeded");
    size_t n = strlen(text) + 1;
    char *value = malloc(n);
    if (!value)
        validation_fail("allocation failed");
    memcpy(value, text, n);
    owned[nowned++] = value;
    return value;
}

static void clear(void) {
    while (nowned)
        free(owned[--nowned]);
}

static const char *constant(unsigned bits, int sign, int64_t value) {
    char text[160];
    validation_path(text,
                    sizeof text,
                    "{\"op\":\"const\",\"bits\":%u,\"signed\":%s,\"value\":%" PRId64 "}",
                    bits,
                    sign ? "true" : "false",
                    value);
    return keep(text);
}

static const char *
node(const char *op, unsigned bits, int sign, const char *a, const char *b, const char *c) {
    char text[8192];
    validation_path(text,
                    sizeof text,
                    "{\"op\":\"%s\",\"bits\":%u,\"signed\":%s,\"args\":[%s%s%s%s%s]}",
                    op,
                    bits,
                    sign ? "true" : "false",
                    a,
                    b ? "," : "",
                    b ? b : "",
                    c ? "," : "",
                    c ? c : "");
    return keep(text);
}

static void apply(qc_context *c, const char *operation) {
    if (!qc_apply(c, operation, NULL))
        validation_fail("expression operation failed: %s", qc_error(c));
}

static void legacy_signed(void) {
    for (unsigned tables = 0; tables < 2; ++tables) {
        qc_context *c = qc_create(1, 7);
        if (!c)
            validation_fail("allocation failed");
        qc_allow_tables(c, tables);
        apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":8}");
        apply(c, "{\"op\":\"qprepare\",\"id\":0,\"lower\":249,\"upper\":249}");
        apply(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":1,\"signed\":false}");
        const char *input = "{\"op\":\"reg\",\"id\":0,\"bits\":8,\"signed\":true}";
        const char *expr =
            node("gt",
                 1,
                 0,
                 node("neg", 16, 1, node("cast", 16, 1, input, NULL, NULL), NULL, NULL),
                 constant(16, 1, 0),
                 NULL);
        char operation[9000];
        validation_path(
            operation, sizeof operation, "{\"op\":\"qcompute\",\"id\":1,\"expr\":%s}", expr);
        apply(c, operation);
        for (size_t i = 0; i < qc_state_size(c); ++i)
            if (cabs(qc_amplitude(c, i) - (i == 505 ? 1 : 0)) > 1e-10)
                validation_fail("legacy signed basis was interpreted as unsigned");
        qc_destroy(c);
        clear();
        ++checks;
    }
    puts(
        "PASS legacy signed basis: declared expression type, promoted negation and positive result");
}

static int relation(unsigned op, int a, int b) {
    switch (op) {
    case 0:
        return a == b;
    case 1:
        return a != b;
    case 2:
        return a < b;
    case 3:
        return a <= b;
    case 4:
        return a > b;
    default:
        return a >= b;
    }
}

static const char *expression(unsigned variant) {
    const char *x = "{\"op\":\"reg\",\"id\":0,\"bits\":2,\"signed\":false}";
    const char *y = "{\"op\":\"reg\",\"id\":1,\"bits\":2,\"signed\":false}";
    const char *ux = node("cast", 4, 0, x, NULL, NULL), *uy = node("cast", 4, 0, y, NULL, NULL);
    const char *sx = node("cast", 4, 1, node("cast", 2, 1, x, NULL, NULL), NULL, NULL);
    const char *quotient = node("div", 4, 0, constant(4, 0, 3), ux, NULL);
    const char *zero = constant(4, 0, 0), *one = constant(4, 0, 1), *result;
    if (variant >= 16 && variant < 28) {
        unsigned comparison = (variant - 16) / 2, reverse = (variant - 16) & 1;
        const char *ops[] = {"eq", "ne", "lt", "le", "gt", "ge"};
        const char *signed_x = node("cast", 2, 1, x, NULL, NULL), *signed_zero = constant(2, 1, 0);
        const char *condition = node(ops[comparison],
                                     1,
                                     0,
                                     reverse ? signed_zero : signed_x,
                                     reverse ? signed_x : signed_zero,
                                     NULL);
        const char *negative = node("neg", 2, 1, signed_x, NULL, NULL);
        int minimum_takes_true = relation(comparison, reverse ? 0 : -2, reverse ? -2 : 0);
        return node("cast",
                    3,
                    0,
                    node("select",
                         2,
                         1,
                         condition,
                         minimum_takes_true ? signed_zero : negative,
                         minimum_takes_true ? negative : signed_zero),
                    NULL,
                    NULL);
    }
    if (variant == 28) {
        const char *signed_x = node("cast", 2, 1, x, NULL, NULL);
        result = node("select",
                      2,
                      1,
                      node("ne", 1, 0, signed_x, constant(2, 1, -2), NULL),
                      node("neg", 2, 1, signed_x, NULL, NULL),
                      constant(2, 1, 0));
        return node("cast", 3, 0, result, NULL, NULL);
    }
    if (variant == 29) {
        const char *signed_y = node("cast", 2, 1, y, NULL, NULL);
        result = node("select",
                      4,
                      0,
                      node("ge", 1, 0, signed_y, constant(2, 1, 0), NULL),
                      node("shl", 4, 0, ux, node("cast", 4, 1, signed_y, NULL, NULL), NULL),
                      constant(4, 0, 7));
        return node("cast", 3, 0, result, NULL, NULL);
    }
    if (variant == 30)
        return node("land", 1, 0, x, node("ne", 1, 0, ux, constant(4, 0, 3), NULL), NULL);
    if (variant == 31)
        return node("ne", 1, 0, node("neg", 4, 1, sx, NULL, NULL), constant(4, 1, -1), NULL);
    if (variant == 32)
        return node("eq", 1, 0, constant(4, 1, -1), node("bnot", 4, 1, sx, NULL, NULL), NULL);
    if (variant == 33)
        return node("add",
                    1,
                    0,
                    node("cast", 1, 0, x, NULL, NULL),
                    node("cast", 1, 0, y, NULL, NULL),
                    NULL);
    switch (variant) {
    case 0:
        result = node("neg", 4, 0, ux, NULL, NULL);
        break;
    case 1:
        result = node("neg", 4, 1, sx, NULL, NULL);
        break;
    case 2:
        result = node("bnot", 4, 1, sx, NULL, NULL);
        break;
    case 3:
        result = node("bnot", 4, 0, node("mul", 4, 0, ux, uy, NULL), NULL, NULL);
        break;
    case 4:
        result = node("land", 1, 0, x, y, NULL);
        break;
    case 5:
        result = node("lor", 1, 0, x, y, NULL);
        break;
    case 6:
        result = node("land", 1, 0, x, quotient, NULL);
        break;
    case 7:
        result = node("lor", 1, 0, node("not", 1, 0, x, NULL, NULL), quotient, NULL);
        break;
    case 8:
        result = node("select", 4, 0, x, quotient, uy);
        break;
    case 9:
        result = node("select", 4, 0, node("eq", 1, 0, ux, zero, NULL), uy, quotient);
        break;
    case 10: {
        const char *den = node("band", 4, 0, ux, one, NULL);
        result = node("select", 4, 0, den, node("div", 4, 0, constant(4, 0, 3), den, NULL), uy);
        break;
    }
    case 11:
        result = node("select",
                      4,
                      0,
                      node("eq", 1, 0, ux, zero, NULL),
                      uy,
                      node("select", 4, 0, y, node("div", 4, 0, ux, uy, NULL), constant(4, 0, 3)));
        break;
    case 12:
        result = node("select", 4, 0, zero, node("div", 4, 0, one, zero, NULL), uy);
        break;
    case 13:
        result = node("land", 1, 0, zero, node("div", 4, 0, one, zero, NULL), NULL);
        break;
    case 14:
        result = node("lor", 1, 0, one, node("div", 4, 0, one, zero, NULL), NULL);
        break;
    default:
        result = node("shr",
                      4,
                      1,
                      node("neg", 4, 1, node("add", 4, 1, sx, constant(4, 1, 1), NULL), NULL, NULL),
                      y,
                      NULL);
        break;
    }
    return node("cast",
                variant >= 4 && variant <= 7     ? 1
                : variant == 13 || variant == 14 ? 1
                                                 : 3,
                0,
                result,
                NULL,
                NULL);
}

static int reference(unsigned variant, int x, int y) {
    int sx = x >= 2 ? x - 4 : x;
    if (variant >= 16 && variant < 28) {
        unsigned op = (variant - 16) / 2, reverse = (variant - 16) & 1;
        int condition = relation(op, reverse ? 0 : sx, reverse ? sx : 0),
            minimum = relation(op, reverse ? 0 : -2, reverse ? -2 : 0);
        return condition != minimum ? -sx : 0;
    }
    if (variant == 28)
        return sx != -2 ? -sx : 0;
    if (variant == 29) {
        int sy = y >= 2 ? y - 4 : y;
        return sy >= 0 ? x << sy : 7;
    }
    if (variant == 30)
        return x && x != 3;
    if (variant == 31)
        return -sx != -1;
    if (variant == 32)
        return ~sx == -1;
    if (variant == 33)
        return !!((!!x) + (!!y));
    switch (variant) {
    case 0:
        return -x;
    case 1:
        return -sx;
    case 2:
        return ~sx;
    case 3:
        return ~(x * y);
    case 4:
        return x && y;
    case 5:
        return x || y;
    case 6:
        return x && 3 / x;
    case 7:
        return !x || 3 / x;
    case 8:
        return x ? 3 / x : y;
    case 9:
        return !x ? y : 3 / x;
    case 10:
        return (x & 1) ? 3 / (x & 1) : y;
    case 11:
        return !x ? y : y ? x / y : 3;
    case 12:
        return y;
    case 13:
        return 0;
    case 14:
        return 1;
    default: {
        int n = -(sx + 1), scale = 1 << y;
        return n < 0 ? -((-n + scale - 1) / scale) : n / scale;
    }
    }
}

static void joint_states(void) {
    for (unsigned variant = 0; variant < 34; ++variant)
        for (unsigned tables = 0; tables < 2; ++tables) {
            qc_context *c = qc_create(1, 7);
            if (!c)
                validation_fail("allocation failed");
            qc_allow_tables(c, tables);
            for (unsigned id = 0; id < 2; ++id) {
                char operation[200];
                snprintf(operation,
                         sizeof operation,
                         "{\"op\":\"qalloc\",\"id\":%u,\"width\":2,\"signed\":false}",
                         id);
                apply(c, operation);
                snprintf(operation,
                         sizeof operation,
                         "{\"op\":\"qprepare\",\"id\":%u,\"lower\":0,\"upper\":3}",
                         id);
                apply(c, operation);
            }
            unsigned width =
                (variant >= 4 && variant <= 7) || variant == 13 || variant == 14 || variant >= 30
                    ? 1
                    : 3;
            char operation[10000];
            snprintf(operation,
                     sizeof operation,
                     "{\"op\":\"qalloc\",\"id\":2,\"width\":%u,\"signed\":false}",
                     width);
            apply(c, operation);
            if (!qc_emit(c, QC_PHASE, 0, QC_WIRE(0), QC_WIRE(0), .37) ||
                !qc_emit(c, QC_PHASE, 0, QC_WIRE(3), QC_WIRE(3), -.19))
                validation_fail("phase failed");
            const char *expr = expression(variant);
            validation_path(
                operation, sizeof operation, "{\"op\":\"qcompute\",\"id\":2,\"expr\":%s}", expr);
            if (!qc_apply(c, operation, NULL))
                validation_fail(
                    "joint expression variant=%u tables=%u: %s", variant, tables, qc_error(c));
            for (size_t i = 0; i < qc_state_size(c); ++i) {
                unsigned expected =
                    (unsigned)reference(variant, (int)(i & 3), (int)((i >> 2) & 3)) &
                    ((1u << width) - 1);
                double complex phase = cexp(I * ((i & 1 ? .37 : 0) - (i & 8 ? .19 : 0))) / 4;
                if (cabs(qc_amplitude(c, i) - ((i >> 4) == expected ? phase : 0)) > 1e-10)
                    validation_fail("joint expression mismatch variant=%u tables=%u basis=%zu",
                                    variant,
                                    tables,
                                    i);
            }
            validation_path(
                operation, sizeof operation, "{\"op\":\"quncompute\",\"id\":2,\"expr\":%s}", expr);
            apply(c, operation);
            apply(c, "{\"op\":\"qrelease\",\"id\":2}");
            for (size_t i = 0; i < qc_state_size(c); ++i) {
                double complex phase = cexp(I * ((i & 1 ? .37 : 0) - (i & 8 ? .19 : 0))) / 4;
                if (cabs(qc_amplitude(c, i) - (i < 16 ? phase : 0)) > 1e-10)
                    validation_fail("expression inverse left garbage");
            }
            qc_destroy(c);
            clear();
            ++checks;
        }
    puts(
        "PASS 68 joint expression domains: relative phases, C short circuiting, promotions, narrowing, relational/compound guards and clean inverse");
}

static void rejection_and_exports(void) {
    for (unsigned test = 0; test < 3; ++test)
        for (unsigned tables = 0; tables < 2; ++tables) {
            qc_context *c = qc_create(0, 7);
            qc_allow_tables(c, tables);
            apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":3,\"signed\":false}");
            apply(c, "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":7}");
            apply(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":3,\"signed\":false}");
            const char *x = "{\"op\":\"reg\",\"id\":0,\"bits\":3,\"signed\":false}";
            const char *u = node("cast", 4, 0, x, NULL, NULL), *zero = constant(4, 0, 0), *div;
            if (test == 0)
                div = node("neg", 3, 1, node("cast", 3, 1, x, NULL, NULL), NULL, NULL);
            else if (test == 1)
                div =
                    node("select", 4, 0, x, node("div", 4, 0, constant(4, 0, 1), zero, NULL), zero);
            else
                div = node("select",
                           4,
                           0,
                           x,
                           node("div",
                                4,
                                0,
                                constant(4, 0, 1),
                                node("cast", 4, 0, node("cast", 2, 0, u, NULL, NULL), NULL, NULL),
                                NULL),
                           zero);
            const char *expr = node("cast", 3, 0, div, NULL, NULL);
            char operation[10000];
            validation_path(
                operation, sizeof operation, "{\"op\":\"qcompute\",\"id\":1,\"expr\":%s}", expr);
            if (qc_apply(c, operation, NULL) || !*qc_error(c))
                validation_fail("reachable undefined arithmetic accepted case=%u", test);
            qc_destroy(c);
            clear();
            ++checks;
        }
    for (unsigned mode = 0; mode < 6; ++mode) {
        qc_context *c = qc_create(0, 7);
        qc_allow_tables(c, 0);
        apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":64,\"signed\":false}");
        apply(c, "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":18446744073709551615}");
        apply(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":1,\"signed\":false}");
        const char *x = "{\"op\":\"reg\",\"id\":0,\"bits\":64,\"signed\":false}";
        const char *boundary =
            mode < 3
                ? constant(64, 0, 0)
                : keep(
                      mode == 5
                          ? "{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":9223372036854775808}"
                          : "{\"op\":\"const\",\"bits\":64,\"signed\":false,\"value\":18446744073709551615}");
        const char *expr =
            mode == 0   ? node("cast", 1, 0, x, NULL, NULL)
            : mode == 1 ? node("not", 1, 0, x, NULL, NULL)
                        : node(mode == 3 || mode == 5 ? "eq" : "ne", 1, 0, x, boundary, NULL);
        char operation[10000];
        validation_path(
            operation, sizeof operation, "{\"op\":\"qcompute\",\"id\":1,\"expr\":%s}", expr);
        apply(c, operation);
        if (qc_qubits(c) > 70 || !c->ngates)
            validation_fail("64-bit truth test allocated width-sized scratch");
        qc_destroy(c);
        clear();
        ++checks;
    }
    puts(
        "PASS reachable undefined arithmetic rejected; 64-bit truth/zero circuits generated without width-sized scratch (export only)");
}

static void frontend(validation *v) {
    const char *program =
        "#include <quantum.h>\nquant uint8_t choose(quant const bool x,quant const bool y){return x?3/x:y;}\nint main(void){quant bool a[2]={superposition(0,1),superposition(0,1)};for(int i=0;i<2;++i)a[i]^=true;quant uint8_t r=choose(a[0],a[1]);unsigned cr=measure(r);reset(r);quant int8_t n=-((int)a[0]+1);int cn=measure(n);reset(n);quant uint8_t b=~(unsigned)a[0];unsigned cb=measure(b);reset(b);quant bool l=(!a[0]||(3/a[0]))&&a[1];unsigned cl=measure(l);unsigned x=measure(a[0]),y=measure(a[1]);return cr!=(x?3/x:y)||cn!=-(int)(x+1)||cb!=((~x)&255)||cl!=((!x||(3/x))&&y);}\n";
    const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
    char source[4096], compiler[4096], artifact[4096], backend[4096], resources[4096];
    validation_path(source, sizeof source, "%s/expressions.c", v->temp);
    validation_write(source, program);
    validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
    validation_path(artifact, sizeof artifact, "%s/program", v->temp);
    validation_path(backend, sizeof backend, "%s/build/cket_backend", v->root);
    validation_path(resources, sizeof resources, "%s/resources.txt", v->temp);
    for (unsigned level = 0; level < 5; ++level) {
        char *command[] = {
            compiler, source, "--run", "--seed", "7", (char *)levels[level], "-o", artifact, NULL};
        if (validation_run(v, command, NULL, 60)) {
            char *log = validation_read(v->log);
            validation_fail("expression composition failed %s: %s", levels[level], log);
        }
        ++checks;
    }
    for (unsigned variant = 0; variant < 2; ++variant)
        for (unsigned level = 0; level < 5; ++level) {
            const char *wide =
                variant
                    ? "#include <quantum.h>\nint main(void){quant uint64_t x=superposition(0,UINT64_MAX);quant bool r=(x!=0)&&((~x)!=0);measure(r);measure(x);return 0;}\n"
                    : "#include <quantum.h>\nint main(void){quant uint16_t x=superposition(0,65535);quant uint16_t r=x?7/x:0;measure(r);measure(x);return 0;}\n";
            validation_write(source, wide);
            char *compile[] = {
                compiler, source, "--emit-ir", (char *)levels[level], "-o", artifact, NULL};
            char *synthesize[] = {backend, "--resources", artifact, "--no-tables", NULL};
            if (validation_run(v, compile, NULL, 60) ||
                validation_run(v, synthesize, resources, 60)) {
                char *log = validation_read(v->log);
                validation_fail("wide expression export failed %s: %s", levels[level], log);
            }
            ++checks;
        }
    puts(
        "PASS expression C integration: function/array/loop/measurement at five GCC levels; 16-bit guarded division and 64-bit logic exported without tables");
}

#include "expression_basis.inc"
#include "small_remainder_state.inc"
#include "selection_state.inc"
#include "small_division_state.inc"

int main(int argc, char **argv) {
    if (argc != 1)
        validation_fail("usage: validate_expressions");
    legacy_signed();
    joint_states();
    rejection_and_exports();
    small_remainder_states();
    selection_states();
    small_division_states();
    validation v;
    validation_init(&v, argv[0]);
    frontend(&v);
    validation_cleanup(&v);
    printf(
        "%u expression checks passed; generated circuits and complete-state comparisons have separate scopes.\n",
        checks);
    return 0;
}
