#include "internal.h"
#include "validation.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks, allocated;
static char *expressions[128];

static const char *keep(const char *text) {
    if (allocated == 128)
        validation_fail("expression fixture budget exceeded");
    char *copy = malloc(strlen(text) + 1);
    if (!copy)
        validation_fail("out of memory creating expression fixture");
    strcpy(copy, text);
    expressions[allocated++] = copy;
    return copy;
}

static void clear(void) {
    while (allocated)
        free(expressions[--allocated]);
}

static const char *node(const char *op, unsigned bits, int sign, const char *a, const char *b) {
    char text[12000];
    validation_path(text,
                    sizeof text,
                    "{\"op\":\"%s\",\"bits\":%u,\"signed\":%s,\"args\":[%s%s%s]}",
                    op,
                    bits,
                    sign ? "true" : "false",
                    a,
                    b ? "," : "",
                    b ? b : "");
    return keep(text);
}

static const char *constant(unsigned bits, int sign, uint64_t value) {
    char text[256], number[64];
    if (sign)
        validation_path(number, sizeof number, "%lld", (long long)(int64_t)value);
    else
        validation_path(
            number,
            sizeof number,
            "%llu",
            (unsigned long long)(value & (bits == 64 ? UINT64_MAX : (UINT64_C(1) << bits) - 1)));
    validation_path(text,
                    sizeof text,
                    "{\"op\":\"const\",\"bits\":%u,\"signed\":%s,\"value\":%s}",
                    bits,
                    sign ? "true" : "false",
                    number);
    return keep(text);
}

static const char *source(unsigned bits, int sign) {
    char text[256];
    validation_path(text,
                    sizeof text,
                    "{\"op\":\"reg\",\"id\":0,\"bits\":%u,\"signed\":%s}",
                    bits,
                    sign ? "true" : "false");
    return keep(text);
}

static void apply(qc_context *c, const char *operation) {
    if (!qc_apply(c, operation, NULL))
        validation_fail("in-place operation failed: %s\n%s", qc_error(c), operation);
}

static void update(qc_context *c, const char *expr, unsigned wanted, int controlled) {
    char operation[16000];
    validation_path(operation,
                    sizeof operation,
                    "{\"op\":\"qinplace\",\"id\":0,\"expr\":%s%s}",
                    expr,
                    controlled ? wanted ? ",\"controls\":[{\"id\":1,\"value\":1}]"
                                        : ",\"controls\":[{\"id\":1,\"value\":0}]"
                               : "");
    apply(c, operation);
}

static const char *rotation(const char *x, unsigned width, int left) {
    const char *one = constant(16, 0, 1), *distance = constant(16, 0, width - 1);
    if (left)
        return node("bor", 16, 0, node("shl", 16, 0, x, one), node("shr", 16, 0, x, distance));
    return node("bor",
                16,
                0,
                node("shr", 16, 0, x, one),
                node("shl", 16, 0, node("band", 16, 0, x, one), distance));
}

static int signed_variant(unsigned variant) {
    return variant == 2 || variant == 5 || variant == 6;
}

static const char *fixture(unsigned variant, unsigned width) {
    int sign = signed_variant(variant);
    const char *x = node("cast", 16, sign, source(width, sign), NULL), *result;
    if (variant == 0)
        result = node("mul", 16, sign, x, constant(16, sign, 3));
    else if (variant == 1)
        result = node("add",
                      16,
                      sign,
                      constant(16, sign, 5),
                      node("mul", 16, sign, constant(16, sign, 3), x));
    else if (variant == 2)
        result = node("add",
                      16,
                      sign,
                      node("mul", 16, sign, x, constant(16, sign, UINT64_C(0) - 3)),
                      constant(16, sign, 7));
    else if (variant == 3 || variant == 6)
        result = node("bnot", 16, sign, x, NULL);
    else if (variant == 4 || variant == 5)
        result = node("neg", 16, sign, x, NULL);
    else if (variant == 7)
        result = node("sub",
                      16,
                      sign,
                      node("mul",
                           16,
                           sign,
                           node("add",
                                16,
                                sign,
                                node("mul", 16, sign, x, constant(16, sign, 3)),
                                constant(16, sign, 5)),
                           constant(16, sign, 5)),
                      constant(16, sign, 7));
    else if (variant == 8)
        result = node("bxor", 16, sign, x, constant(16, sign, 0xa5));
    else if (variant == 9 || variant == 10)
        result = rotation(x, width, variant == 9);
    else
        result = node("bnot", 16, 0, rotation(x, width, variant == 11), NULL);
    return node("cast", width, sign, result, NULL);
}

static unsigned reference(unsigned variant, unsigned width, unsigned value) {
    unsigned mask = (1u << width) - 1;
    int signed_value = value >= (1u << (width - 1)) ? (int)value - (int)(mask + 1) : (int)value;
    switch (variant) {
    case 0:
        return (value * 3) & mask;
    case 1:
        return (5 + value * 3) & mask;
    case 2:
        return (unsigned)(signed_value * -3 + 7) & mask;
    case 3:
    case 6:
        return ~value & mask;
    case 4:
    case 5:
        return (unsigned)-signed_value & mask;
    case 7:
        return ((value * 3 + 5) * 5 - 7) & mask;
    case 8:
        return (value ^ 0xa5) & mask;
    case 9:
        return ((value << 1) | (value >> (width - 1))) & mask;
    case 10:
        return ((value >> 1) | ((value & 1) << (width - 1))) & mask;
    default:
        return ~((value << 1) | (value >> (width - 1))) & mask;
    }
}

static const char *inverse_fixture(unsigned variant, unsigned width) {
    if (variant == 8)
        return fixture(variant, width);
    if (variant >= 9)
        return fixture(variant == 9 ? 10 : variant == 10 ? 9 : 12, width);
    uint64_t coefficient = variant == 0 || variant == 1 ? 3
                           : variant == 2               ? UINT64_C(0) - 3
                           : variant == 7               ? 15
                                                        : UINT64_MAX;
    uint64_t offset = variant == 1                   ? 5
                      : variant == 2                 ? 7
                      : variant == 7                 ? 18
                      : variant == 3 || variant == 6 ? UINT64_MAX
                                                     : 0;
    uint64_t inverse = 1;
    for (unsigned step = 0; step < 6; ++step)
        inverse *= 2 - coefficient * inverse;
    unsigned mask = (1u << width) - 1;
    inverse &= mask;
    const char *x = node("cast", 64, 0, source(width, signed_variant(variant)), NULL);
    return node("cast",
                width,
                signed_variant(variant),
                node("add",
                     64,
                     0,
                     node("mul", 64, 0, x, constant(64, 0, inverse)),
                     constant(64, 0, (UINT64_C(0) - inverse * offset) & mask)),
                NULL);
}

static void joint_states(void) {
    const unsigned widths[] = {2, 3, 4, 8};
    for (unsigned wi = 0; wi < 4; ++wi)
        for (unsigned variant = 0; variant < 12; ++variant)
            for (unsigned tables = 0; tables < 2; ++tables)
                for (unsigned wanted = 0; wanted < 2; ++wanted) {
                    unsigned width = widths[wi], size = 1u << width;
                    qc_context *c = qc_create(1, 7);
                    if (!c)
                        validation_fail("context allocation failed");
                    qc_allow_tables(c, tables);
                    char operation[256];
                    validation_path(operation,
                                    sizeof operation,
                                    "{\"op\":\"qalloc\",\"id\":0,\"width\":%u,\"signed\":%s}",
                                    width,
                                    signed_variant(variant) ? "true" : "false");
                    apply(c, operation);
                    validation_path(operation,
                                    sizeof operation,
                                    "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":%u}",
                                    signed_variant(variant) ? 0 : size - 1);
                    apply(c, operation);
                    if (signed_variant(variant))
                        apply(c, "{\"op\":\"qfourier\",\"id\":0,\"inverse\":false}");
                    apply(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":1,\"signed\":false}");
                    apply(c, "{\"op\":\"qprepare\",\"id\":1,\"lower\":0,\"upper\":1}");
                    if (!qc_emit(c, QC_PHASE, 0, QC_WIRE(0), QC_WIRE(0), .29) ||
                        !qc_emit(c, QC_PHASE, 0, QC_WIRE(width - 1), QC_WIRE(width - 1), .47))
                        validation_fail("phase setup failed");
                    update(c, fixture(variant, width), wanted, 1);
                    double complex *expected = calloc(2 * size, sizeof *expected);
                    if (!expected)
                        validation_fail("state reference allocation failed");
                    for (unsigned control = 0; control < 2; ++control)
                        for (unsigned x = 0; x < size; ++x) {
                            unsigned y = control == wanted ? reference(variant, width, x) : x;
                            expected[y | (control << width)] =
                                cexp(I * ((x & 1 ? .29 : 0) + (x & (size / 2) ? .47 : 0))) /
                                sqrt(2.0 * size);
                        }
                    if (qc_qubits(c) != width + 1 || c->measure || c->reset)
                        validation_fail(
                            "in-place circuit used private storage or a destructive channel");
                    for (unsigned i = 0; i < 2 * size; ++i)
                        if (cabs(qc_amplitude(c, i) - expected[i]) > 1e-10)
                            validation_fail(
                                "in-place C reference mismatch: width=%u variant=%u tables=%u index=%u",
                                width,
                                variant,
                                tables,
                                i);
                    update(c, inverse_fixture(variant, width), wanted, 1);
                    for (unsigned i = 0; i < 2 * size; ++i) {
                        unsigned x = i & (size - 1);
                        double complex amplitude =
                            cexp(I * ((x & 1 ? .29 : 0) + (x & (size / 2) ? .47 : 0))) /
                            sqrt(2.0 * size);
                        if (cabs(qc_amplitude(c, i) - amplitude) > 1e-10)
                            validation_fail(
                                "in-place inverse mismatch: width=%u variant=%u", width, variant);
                    }
                    free(expected);
                    qc_destroy(c);
                    clear();
                    ++checks;
                }
    puts(
        "PASS 192 in-place joint-state comparisons: both synthesis settings, signed promotions, modular arithmetic, bit rotations, open/closed controls, phase and inverse");
}

static void boolean_states(void) {
    for (unsigned variant = 0; variant < 12; ++variant)
        for (unsigned tables = 0; tables < 2; ++tables)
            for (unsigned wanted = 0; wanted < 2; ++wanted) {
                qc_context *c = qc_create(1, 7);
                qc_allow_tables(c, tables);
                apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":1,\"signed\":false}");
                apply(c, "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":1}");
                apply(c, "{\"op\":\"qalloc\",\"id\":1,\"width\":1,\"signed\":false}");
                apply(c, "{\"op\":\"qprepare\",\"id\":1,\"lower\":0,\"upper\":1}");
                if (!qc_emit(c, QC_PHASE, 0, QC_WIRE(0), QC_WIRE(0), .37))
                    validation_fail("Boolean phase setup failed");
                const char *x = source(1, 0), *expr;
                if (variant == 0)
                    expr = node("not", 1, 0, x, NULL);
                else if (variant == 1)
                    expr = node(
                        "cast", 1, 0, node("neg", 16, 1, node("cast", 16, 1, x, NULL), NULL), NULL);
                else if (variant == 2)
                    expr = node("land", 1, 0, x, constant(1, 0, 1));
                else if (variant == 3)
                    expr = node("not", 1, 0, node("lor", 1, 0, x, constant(1, 0, 0)), NULL);
                else if (variant == 4)
                    expr = node("cast",
                                1,
                                0,
                                node("band",
                                     16,
                                     0,
                                     node("bnot", 16, 0, node("cast", 16, 0, x, NULL), NULL),
                                     constant(16, 0, 1)),
                                NULL);
                else if (variant == 6)
                    expr = node("eq", 1, 0, node("cast", 16, 0, x, NULL), constant(16, 0, 0));
                else if (variant == 7)
                    expr = node("ne", 1, 0, constant(16, 0, 1), node("cast", 16, 0, x, NULL));
                else if (variant == 8)
                    expr = node("eq",
                                1,
                                0,
                                constant(16, 1, 0),
                                node("neg", 16, 1, node("cast", 16, 1, x, NULL), NULL));
                else if (variant >= 9) {
                    unsigned width = variant == 9 ? 16 : variant == 10 ? 32 : 64;
                    int sign = variant == 10;
                    uint64_t factor = variant == 9    ? 2
                                      : variant == 10 ? UINT64_C(0) - 2147483648u
                                                      : UINT64_MAX;
                    expr = node("cast",
                                1,
                                0,
                                node("mul",
                                     width,
                                     sign,
                                     node("cast", width, sign, x, NULL),
                                     constant(width, sign, factor)),
                                NULL);
                } else
                    expr = node(
                        "not", 1, 0, node("neg", 16, 1, node("cast", 16, 1, x, NULL), NULL), NULL);
                update(c, expr, wanted, 1);
                double complex expected[4] = {0};
                for (unsigned control = 0; control < 2; ++control)
                    for (unsigned value = 0; value < 2; ++value) {
                        unsigned result =
                            variant == 1 || variant == 2 || variant >= 9 ? value : !value;
                        if (control != wanted)
                            result = value;
                        expected[result | (control << 1)] = cexp(I * (value ? .37 : 0)) / 2;
                    }
                if (qc_qubits(c) != 2 || c->measure || c->reset)
                    validation_fail("Boolean update consumed storage or destroyed information");
                for (unsigned i = 0; i < 4; ++i)
                    if (cabs(qc_amplitude(c, i) - expected[i]) > 1e-10)
                        validation_fail(
                            "Boolean normalization or logical update mismatch: variant=%u",
                            variant);
                update(c, expr, wanted, 1);
                for (unsigned i = 0; i < 4; ++i)
                    if (cabs(qc_amplitude(c, i) - cexp(I * (i & 1 ? .37 : 0)) / 2) > 1e-10)
                        validation_fail("Boolean inverse mismatch");
                qc_destroy(c);
                clear();
                ++checks;
            }
    puts(
        "PASS 48 Boolean in-place states: truth conversion, logical operations, comparisons, multiplication, phase, controls and inverse");
}

static void policy_and_resources(void) {
    for (unsigned tables = 0; tables < 2; ++tables) {
        qc_context *c = qc_create(1, 7);
        qc_allow_tables(c, tables);
        apply(c, "{\"op\":\"qalloc\",\"id\":0,\"width\":3,\"signed\":false}");
        apply(c, "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":7}");
        const char *expr = node(
            "add", 3, 0, node("bxor", 3, 0, source(3, 0), constant(3, 0, 1)), constant(3, 0, 2));
        char operation[4096];
        validation_path(
            operation, sizeof operation, "{\"op\":\"qinplace\",\"id\":0,\"expr\":%s}", expr);
        int ok = qc_apply(c, operation, NULL);
        if ((int)tables != ok || (!tables && !strstr(qc_error(c), "truth tables are disabled")))
            validation_fail("in-place enumeration ignored the no-tables contract");
        qc_destroy(c);
        clear();
        ++checks;
    }
    for (unsigned variant = 0; variant < 3; ++variant)
        for (unsigned tables = 0; tables < 2; ++tables) {
            qc_context *c = qc_create(1, 7);
            qc_allow_tables(c, tables);
            apply(c,
                  variant == 2 ? "{\"op\":\"qalloc\",\"id\":0,\"width\":8,\"signed\":true}"
                               : "{\"op\":\"qalloc\",\"id\":0,\"width\":8,\"signed\":false}");
            apply(c,
                  variant == 2 ? "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":0}"
                               : "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":255}");
            if (variant == 2)
                apply(c, "{\"op\":\"qfourier\",\"id\":0,\"inverse\":false}");
            const char *expr = node("mul",
                                    8,
                                    variant == 2,
                                    source(8, variant == 2),
                                    constant(8,
                                             variant == 2,
                                             variant == 2 ? 3
                                             : variant    ? 0
                                                          : 2));
            char operation[4096];
            validation_path(
                operation, sizeof operation, "{\"op\":\"qinplace\",\"id\":0,\"expr\":%s}", expr);
            if (qc_apply(c, operation, NULL))
                validation_fail("noninjective or undefined update was accepted");
            qc_destroy(c);
            clear();
            ++checks;
        }
    const unsigned widths[] = {16, 32, 64};
    for (unsigned i = 0; i < 3; ++i) {
        unsigned width = widths[i];
        qc_context *c = qc_create(0, 7);
        qc_allow_tables(c, 0);
        char operation[4096];
        validation_path(operation,
                        sizeof operation,
                        "{\"op\":\"qalloc\",\"id\":0,\"width\":%u,\"signed\":false}",
                        width);
        apply(c, operation);
        validation_path(
            operation,
            sizeof operation,
            "{\"op\":\"qprepare\",\"id\":0,\"lower\":0,\"upper\":%llu}",
            (unsigned long long)(width == 64 ? UINT64_MAX : (UINT64_C(1) << width) - 1));
        apply(c, operation);
        update(c,
               node("add",
                    width,
                    0,
                    node("mul", width, 0, source(width, 0), constant(width, 0, 3)),
                    constant(width, 0, 5)),
               0,
               0);
        if (qc_qubits(c) != width || c->ngates > 100000 || !c->synthesis_runs)
            validation_fail("wide in-place circuit violated its resource contract");
        qc_destroy(c);
        clear();
        ++checks;
    }
    puts(
        "PASS in-place table policy, noninjective/undefined rejections and 16/32/64-bit generation (export only)");
}

static void frontend(validation *v) {
    const char *program =
        "#include <quantum.h>\nvoid transform(quant uint8_t *x){*x=(*x*3+5);*x=(uint8_t)((*x<<1)|(*x>>7));}\nint main(void){quant uint8_t a[2]={superposition(0,15),7};unsigned initial=measure(a[0]);for(int i=0;i<2;++i)transform(&a[i]);unsigned first=(initial*3+5)&255;first=((first<<1)|(first>>7))&255;unsigned second=26;second=((second<<1)|(second>>7))&255;unsigned ra=measure(a[0]),rb=measure(a[1]);quant bool b=superposition(0,1);unsigned old=measure(b);b=!b;unsigned next=measure(b);return ra!=first||rb!=second||next!=!old;}\n";
    const char *levels[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
    char source_path[4096], compiler[4096], artifact[4096], backend[4096], output[4096];
    validation_path(source_path, sizeof source_path, "%s/inplace.c", v->temp);
    validation_path(compiler, sizeof compiler, "%s/build/cket", v->root);
    validation_path(artifact, sizeof artifact, "%s/program", v->temp);
    validation_path(backend, sizeof backend, "%s/build/cket_backend", v->root);
    validation_path(output, sizeof output, "%s/resources.txt", v->temp);
    validation_write(source_path, program);
    for (unsigned level = 0; level < 5; ++level) {
        char *command[] = {compiler,
                           source_path,
                           "--run",
                           "--seed",
                           "7",
                           (char *)levels[level],
                           "-o",
                           artifact,
                           NULL};
        if (validation_run(v, command, NULL, 60)) {
            char *log = validation_read(v->log);
            validation_fail("in-place C composition failed %s: %s", levels[level], log);
        }
        ++checks;
    }
    const char *types[] = {"uint16_t", "uint32_t", "uint64_t"};
    const char *upper[] = {"65535", "UINT32_MAX", "UINT64_MAX"};
    for (unsigned type = 0; type < 3; ++type)
        for (unsigned level = 0; level < 5; ++level) {
            char wide[1024];
            validation_path(
                wide,
                sizeof wide,
                "#include <quantum.h>\nint main(void){quant %s x=superposition(0,%s);x*=3;x=x*5+7;x=~x;x=-x;measure(x);return 0;}\n",
                types[type],
                upper[type]);
            validation_write(source_path, wide);
            char *compile[] = {
                compiler, source_path, "--emit-ir", (char *)levels[level], "-o", artifact, NULL};
            char *generate[] = {backend, "--resources", artifact, "--no-tables", NULL};
            if (validation_run(v, compile, NULL, 60) || validation_run(v, generate, output, 60)) {
                char *log = validation_read(v->log);
                validation_fail(
                    "wide in-place C export failed %s %s: %s", types[type], levels[level], log);
            }
            ++checks;
        }
    puts(
        "PASS in-place functions, fixed memory, control, arithmetic and five GCC optimization levels");
}

int main(int argc, char **argv) {
    if (argc != 1)
        validation_fail("usage: validate_inplace");
    joint_states();
    boolean_states();
    policy_and_resources();
    validation v;
    validation_init(&v, argv[0]);
    frontend(&v);
    validation_cleanup(&v);
    printf(
        "%u in-place checks passed; wide circuit generation is distinct from complete-state validation.\n",
        checks);
    return 0;
}
